/**
 *
 */

#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "soc/rtc.h"

#include "driver/i2s.h"
#include "driver/timer.h"

#include "MedianFilter.h"
#include "board_pins_config.h"
#include "player.h"
#include "snapcast.h"

#define SYNC_TASK_PRIORITY 20
#define SYNC_TASK_CORE_ID tskNO_AFFINITY

static const char *TAG = "player";

/**
 * @brief Pre define APLL parameters, save compute time
 *        | bits_per_sample | rate | sdm0 | sdm1 | sdm2 | odir
 *
 *        apll_freq = xtal_freq * (4 + sdm2 + sdm1/256 + sdm0/65536)/((o_div +
 * 2) * 2) I2S bit clock is (apll_freq / 16)
 */
static const int apll_predefine[][6] = {
    {16, 11025, 38, 80, 5, 31},   {16, 16000, 147, 107, 5, 21},
    {16, 22050, 130, 152, 5, 15}, {16, 32000, 129, 212, 5, 10},
    {16, 44100, 15, 8, 5, 6},     {16, 48000, 136, 212, 5, 6},
    {16, 96000, 143, 212, 5, 2},  {0, 0, 0, 0, 0, 0}};

static const int apll_predefine_48k_corr[][6] = {
    {16, 48048, 27, 215, 5, 6},   // ~ 48kHz * 1.001
    {16, 47952, 20, 210, 5, 6},   // ~ 48kHz * 0.999
    {16, 48005, 213, 212, 5, 6},  // ~ 48kHz * 1.0001
    {16, 47995, 84, 212, 5, 6},   // ~ 48kHz * 0.9999
};

static SemaphoreHandle_t latencyBufSemaphoreHandle = NULL;

static int8_t latencyBuffFull = 0;

static sMedianFilter_t latencyMedianFilterLong;
static sMedianNode_t latencyMedianLongBuffer[MEDIAN_FILTER_LONG_BUF_LEN];

static int64_t latencyToServer = 0;

// buffer_.setSize(500);
//    shortBuffer_.setSize(100);
//    miniBuffer_.setSize(20);

static sMedianFilter_t shortMedianFilter;
static sMedianNode_t shortMedianBuffer[SHORT_BUFFER_LEN];

static int8_t currentDir = 0;  //!< current apll direction, see apll_adjust()

static QueueHandle_t pcmChunkQueueHandle = NULL;
#define PCM_CHNK_QUEUE_LENGTH \
  500  // TODO: one chunk is hardcoded to 20ms, change it to be dynamically
       // adjustable.
static StaticQueue_t pcmChunkQueue;
static uint8_t pcmChunkQueueStorageArea[PCM_CHNK_QUEUE_LENGTH *
                                        sizeof(wire_chunk_message_t *)];

static int64_t i2sDmaLAtency = 0;

static TaskHandle_t syncTaskHandle = NULL;

static xQueueHandle i2s_event_queue = NULL;

static const size_t chunkInBytes =
    (WIRE_CHUNK_DURATION_MS * SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8)) /
    1000;

static uint32_t i2sDmaBufCnt;

static void tg0_timer_init(void);
static void tg0_timer_deinit(void);
static void snapcast_sync_task(void *pvParameters);

/*
#define CONFIG_MASTER_I2S_BCK_PIN 5
#define CONFIG_MASTER_I2S_LRCK_PIN 25
#define CONFIG_MASTER_I2S_DATAOUT_PIN 26
#define CONFIG_SLAVE_I2S_BCK_PIN 26
#define CONFIG_SLAVE_I2S_LRCK_PIN 12
#define CONFIG_SLAVE_I2S_DATAOUT_PIN 5
*/

static esp_err_t player_setup_i2s(uint32_t sample_rate, i2s_port_t i2sNum) {
  int chunkInFrames = chunkInBytes / (CHANNELS * (BITS_PER_SAMPLE / 8));
  int __dmaBufCnt;
  int __dmaBufLen;
  const int __dmaBufMaxLen = 1024;

  __dmaBufCnt = 1;
  __dmaBufLen = chunkInFrames;
  while ((__dmaBufLen >= __dmaBufMaxLen) || (__dmaBufCnt <= 1)) {
    if ((__dmaBufLen % 2) == 0) {
      __dmaBufCnt *= 2;
      __dmaBufLen /= 2;
    } else {
      ESP_LOGE(TAG,
               "player_setup_i2s: Can't setup i2s with this configuration");

      return -1;
    }
  }

  i2sDmaLAtency =
      (1000000LL * __dmaBufLen / SAMPLE_RATE) *
      0.9;  // this value depends on __dmaBufLen, optimized for __dmaBufLen =
            // 480 @opus, 192000bps, complexity 10 20ms chunks it will be
            // smaller for lower values of __dmaBufLen. The delay was measured
            // against raspberry client
            // TODO: find a way to calculate this value without the need to
            // measure delay to another client

  i2sDmaBufCnt = __dmaBufCnt * 2 +
                 1;  // we do double buffering of chunks at I2S DMA, +1 needed
                     // because of the way i2s driver works, it will only
                     // allocate (i2sDmaBufCnt - 1) queue elements

  ESP_LOGI(TAG, "player_setup_i2s: dma_buf_len is %d, dma_buf_count is %d",
           __dmaBufLen, __dmaBufCnt * 2);

  i2s_config_t i2s_config0 = {
      .mode = I2S_MODE_MASTER | I2S_MODE_TX,  // Only TX
      .sample_rate = sample_rate,
      .bits_per_sample = BITS_PER_SAMPLE,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,  // 2-channels
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .dma_buf_count = i2sDmaBufCnt,
      .dma_buf_len = __dmaBufLen,
      .intr_alloc_flags = 1,  // Default interrupt priority
      .use_apll = true,
      .fixed_mclk = 0,
      .tx_desc_auto_clear = true  // Auto clear tx descriptor on underflow
  };

  i2s_pin_config_t pin_config0;
  get_i2s_pins(i2sNum, &pin_config0);

  /*
    i2s_pin_config_t pin_config0 = {
        .bck_io_num = CONFIG_MASTER_I2S_BCK_PIN,
        .ws_io_num = CONFIG_MASTER_I2S_LRCK_PIN,
        .data_out_num = CONFIG_MASTER_I2S_DATAOUT_PIN,
        .data_in_num = -1  // Not used
    };
  */

  i2s_driver_install(i2sNum, &i2s_config0, 1, &i2s_event_queue);
  i2s_set_pin(i2sNum, &pin_config0);

  return 0;
}

// ensure this is called after http_task was killed!
int deinit_player(void) {
  int ret = 0;

  wire_chunk_message_t *chnk = NULL;

  // stop the task
  if (syncTaskHandle == NULL) {
    ESP_LOGW(TAG, "no sync task created?");
  } else {
    vTaskDelete(syncTaskHandle);
  }

  if (pcmChunkQueueHandle == NULL) {
    ESP_LOGW(TAG, "no pcm chunk queue created?");
  } else {
    // free all allocated memory
    while (uxQueueMessagesWaiting(pcmChunkQueueHandle)) {
      ret = xQueueReceive(pcmChunkQueueHandle, &chnk, pdMS_TO_TICKS(2000));
      if (ret != pdFAIL) {
        if (chnk != NULL) {
          free(chnk->payload);
          free(chnk);
          chnk = NULL;
        }
      }
    }

    // delete the queue
    vQueueDelete(pcmChunkQueueHandle);
    pcmChunkQueueHandle = NULL;
  }

  if (latencyBufSemaphoreHandle == NULL) {
    ESP_LOGW(TAG, "no latency buffer semaphore created?");
  } else {
    vSemaphoreDelete(latencyBufSemaphoreHandle);
    latencyBufSemaphoreHandle = NULL;
  }

  tg0_timer_deinit();

  ESP_LOGI(TAG, "deinit player done");

  return ret;
}

/**
 *  call before http task creation!
 */
QueueHandle_t init_player(void) {
  int ret;

  ret = player_setup_i2s(SAMPLE_RATE, I2S_PORT);
  if (ret < 0) {
    ESP_LOGE(TAG, "player_setup_i2s failed: %d", ret);

    return NULL;
  }

  // create semaphore for time diff buffer to server
  if (latencyBufSemaphoreHandle == NULL) {
    latencyBufSemaphoreHandle = xSemaphoreCreateMutex();
  }

  // create snapcast receive buffer
  if (pcmChunkQueueHandle == NULL) {
    pcmChunkQueueHandle = xQueueCreateStatic(
        PCM_CHNK_QUEUE_LENGTH, sizeof(wire_chunk_message_t *),
        pcmChunkQueueStorageArea, &pcmChunkQueue);
  }

  // init diff buff median filter
  latencyMedianFilterLong.numNodes = MEDIAN_FILTER_LONG_BUF_LEN;
  latencyMedianFilterLong.medianBuffer = latencyMedianLongBuffer;
  reset_latency_buffer();

  tg0_timer_init();

  if (syncTaskHandle == NULL) {
    ESP_LOGI(TAG, "Start snapcast_sync_task");

    //							snapcastTaskCfg.outputBufferDacTime_us
    //= outputBufferDacTime_us;
    // snapcastTaskCfg.buffer_us = (int64_t)buffer_ms * 1000LL;
    xTaskCreatePinnedToCore(snapcast_sync_task, "snapcast_sync_task", 8 * 1024,
                            NULL, SYNC_TASK_PRIORITY, &syncTaskHandle,
                            SYNC_TASK_CORE_ID);
  }

  ESP_LOGI(TAG, "init player done");

  return pcmChunkQueueHandle;
}

int8_t player_latency_insert(int64_t newValue) {
  int64_t medianValue;

  medianValue = MEDIANFILTER_Insert(&latencyMedianFilterLong, newValue);
  if (xSemaphoreTake(latencyBufSemaphoreHandle, pdMS_TO_TICKS(1)) == pdTRUE) {
    if (MEDIANFILTER_isFull(&latencyMedianFilterLong)) {
      latencyBuffFull = true;
    }

    latencyToServer = medianValue;

    xSemaphoreGive(latencyBufSemaphoreHandle);
  } else {
    ESP_LOGW(TAG, "couldn't set latencyToServer = medianValue");
  }

  return 0;
}

/**
 *
 */
int8_t player_notify_buffer_ms(uint32_t ms) {
  if (syncTaskHandle == NULL) {
    return -1;
  }

  // notify task of changed parameters
  xTaskNotify(syncTaskHandle, ms, eSetBits);

  return 0;
}

/**
 *
 */
int8_t reset_latency_buffer(void) {
  // init diff buff median filter
  if (MEDIANFILTER_Init(&latencyMedianFilterLong) < 0) {
    ESP_LOGE(TAG, "reset_diff_buffer: couldn't init median filter long. STOP");

    return -2;
  }

  if (latencyBufSemaphoreHandle == NULL) {
    ESP_LOGE(TAG, "reset_diff_buffer: latencyBufSemaphoreHandle == NULL");

    return -2;
  }

  if (xSemaphoreTake(latencyBufSemaphoreHandle, portMAX_DELAY) == pdTRUE) {
    latencyBuffFull = false;
    latencyToServer = 0;

    xSemaphoreGive(latencyBufSemaphoreHandle);
  } else {
    ESP_LOGW(TAG, "reset_diff_buffer: can't take semaphore");

    return -1;
  }

  return 0;
}

/**
 *
 */
int8_t latency_buffer_full(void) {
  int8_t tmp;

  if (latencyBufSemaphoreHandle == NULL) {
    ESP_LOGE(TAG, "latency_buffer_full: latencyBufSemaphoreHandle == NULL");

    return -2;
  }

  if (xSemaphoreTake(latencyBufSemaphoreHandle, 0) == pdFALSE) {
    ESP_LOGW(TAG, "latency_buffer_full: can't take semaphore");

    return -1;
  }

  tmp = latencyBuffFull;

  xSemaphoreGive(latencyBufSemaphoreHandle);

  return tmp;
}

/**
 *
 */
int8_t get_diff_to_server(int64_t *tDiff) {
  static int64_t lastDiff = 0;

  if (latencyBufSemaphoreHandle == NULL) {
    ESP_LOGE(TAG, "get_diff_to_server: latencyBufSemaphoreHandle == NULL");

    return -2;
  }

  if (xSemaphoreTake(latencyBufSemaphoreHandle, 0) == pdFALSE) {
    *tDiff = lastDiff;

    // ESP_LOGW(TAG, "get_diff_to_server: can't take semaphore. Old diff
    // retrieved");

    return -1;
  }

  *tDiff = latencyToServer;
  lastDiff = latencyToServer;  // store value, so we can return a value if
                               // semaphore couldn't be taken

  xSemaphoreGive(latencyBufSemaphoreHandle);

  return 0;
}

/**
 *
 */
int8_t server_now(int64_t *sNow) {
  struct timeval now;
  int64_t diff;

  // get current time
  if (gettimeofday(&now, NULL)) {
    ESP_LOGE(TAG, "server_now: Failed to get time of day");

    return -1;
  }

  if (get_diff_to_server(&diff) == -1) {
    ESP_LOGW(TAG,
             "server_now: can't get current diff to server. Retrieved old one");
  }

  if (diff == 0) {
    // ESP_LOGW(TAG, "server_now: diff to server not initialized yet");

    return -1;
  }

  *sNow = ((int64_t)now.tv_sec * 1000000LL + (int64_t)now.tv_usec) + diff;

  //	ESP_LOGI(TAG, "now: %lldus", (int64_t)now.tv_sec * 1000000LL +
  //(int64_t)now.tv_usec); 	ESP_LOGI(TAG, "diff: %lldus", diff);
  // ESP_LOGI(TAG, "serverNow: %lldus", *snow);

  return 0;
}

/*
 * Timer group0 ISR handler
 *
 * Note:
 * We don't call the timer API here because they are not declared with
 * IRAM_ATTR. If we're okay with the timer irq not being serviced while SPI
 * flash cache is disabled, we can allocate this interrupt without the
 * ESP_INTR_FLAG_IRAM flag and use the normal API.
 */
void IRAM_ATTR timer_group0_isr(void *para) {
  timer_spinlock_take(TIMER_GROUP_0);

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  // Retrieve the interrupt status and the counter value
  //   from the timer that reported the interrupt
  uint32_t timer_intr = timer_group_get_intr_status_in_isr(TIMER_GROUP_0);

  // Clear the interrupt
  //   and update the alarm time for the timer with without reload
  if (timer_intr & TIMER_INTR_T1) {
    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_1);

    // Notify the task in the task's notification value.
    xTaskNotifyFromISR(syncTaskHandle, 0, eNoAction, &xHigherPriorityTaskWoken);
  }

  timer_spinlock_give(TIMER_GROUP_0);

  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

static void tg0_timer_deinit(void) { timer_deinit(TIMER_GROUP_0, TIMER_1); }

/*
 *
 */
static void tg0_timer_init(void) {
  // Select and initialize basic parameters of the timer
  timer_config_t config = {
      //.divider = 8,		// 100ns ticks
      .divider = 80,  // 1µs ticks
      .counter_dir = TIMER_COUNT_UP,
      .counter_en = TIMER_PAUSE,
      .alarm_en = TIMER_ALARM_EN,
      .auto_reload = TIMER_AUTORELOAD_DIS,
  };  // default clock source is APB
  timer_init(TIMER_GROUP_0, TIMER_1, &config);

  // Configure the alarm value and the interrupt on alarm.
  timer_set_alarm_value(TIMER_GROUP_0, TIMER_1, 0);
  timer_enable_intr(TIMER_GROUP_0, TIMER_1);
  if (timer_isr_register(TIMER_GROUP_0, TIMER_1, timer_group0_isr, NULL,
                         ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
                         NULL) != ESP_OK) {
    ESP_LOGE(TAG, "unable to register timer 1 callback");
  }
}

/**
 *
 */
static void tg0_timer1_start(uint64_t alarm_value) {
  timer_pause(TIMER_GROUP_0, TIMER_1);
  timer_set_counter_value(TIMER_GROUP_0, TIMER_1, 0);
  timer_set_alarm_value(TIMER_GROUP_0, TIMER_1, alarm_value);
  timer_set_alarm(TIMER_GROUP_0, TIMER_1, TIMER_ALARM_EN);
  timer_start(TIMER_GROUP_0, TIMER_1);

  //    ESP_LOGI(TAG, "started age timer");
}

// void rtc_clk_apll_enable(bool enable, uint32_t sdm0, uint32_t sdm1, uint32_t
// sdm2, uint32_t o_div); apll_freq = xtal_freq * (4 + sdm2 + sdm1/256 +
// sdm0/65536)/((o_div + 2) * 2) xtal == 40MHz on lyrat v4.3 I2S bit_clock =
// rate * (number of channels) * bits_per_sample
void adjust_apll(int8_t direction) {
  int sdm0, sdm1, sdm2, o_div;
  int index = 2;  // 2 for slow adjustment, 0 for fast adjustment

  // only change if necessary
  if (currentDir == direction) {
    return;
  }

  if (direction == 1) {
    // speed up
    sdm0 = apll_predefine_48k_corr[index][2];
    sdm1 = apll_predefine_48k_corr[index][3];
    sdm2 = apll_predefine_48k_corr[index][4];
    o_div = apll_predefine_48k_corr[index][5];
  } else if (direction == -1) {
    // slow down
    sdm0 = apll_predefine_48k_corr[index + 1][2];
    sdm1 = apll_predefine_48k_corr[index + 1][3];
    sdm2 = apll_predefine_48k_corr[index + 1][4];
    o_div = apll_predefine_48k_corr[index + 1][5];
  } else {
    // reset to normal playback speed
    sdm0 = apll_predefine[5][2];
    sdm1 = apll_predefine[5][3];
    sdm2 = apll_predefine[5][4];
    o_div = apll_predefine[5][5];

    direction = 0;
  }

  rtc_clk_apll_enable(1, sdm0, sdm1, sdm2, o_div);

  currentDir = direction;
}

/**
 *
 */
static void snapcast_sync_task(void *pvParameters) {
  wire_chunk_message_t *chnk = NULL;
  int64_t serverNow = 0;
  int64_t age;
  BaseType_t ret;
  int64_t chunkDuration_us = WIRE_CHUNK_DURATION_MS * 1000;
  int64_t sampleDuration_ns =
      (1000000 / 48);  // 16bit, 2ch, 48kHz (in nano seconds)
  char *p_payload = NULL;
  size_t size = 0;
  uint32_t notifiedValue;
  uint64_t timer_val;
  int32_t alarmValSub = 0;
  int initialSync = 0;
  int64_t avg = 0;
  int dir = 0;
  i2s_event_t i2sEvent;
  uint32_t i2sDmaBufferCnt = 0;
  int64_t buffer_ms_local;

  ESP_LOGI(TAG, "started sync task");

  //	tg0_timer_init();		// initialize initial sync timer

  initialSync = 0;

  currentDir = 1;  // force adjust_apll to set correct playback speed
  adjust_apll(0);

  shortMedianFilter.numNodes = SHORT_BUFFER_LEN;
  shortMedianFilter.medianBuffer = shortMedianBuffer;
  if (MEDIANFILTER_Init(&shortMedianFilter)) {
    ESP_LOGE(TAG, "snapcast_sync_task: couldn't init shortMedianFilter. STOP");

    return;
  }

  while (1) {
    // get notification value which holds buffer_ms as communicated by
    // snapserver
    xTaskNotifyWait(pdFALSE,         // Don't clear bits on entry.
                    pdFALSE,         // Don't clear bits on exit
                    &notifiedValue,  // Stores the notified value.
                    0);

    buffer_ms_local = (int64_t)notifiedValue * 1000LL;

    if (chnk == NULL) {
      ret = xQueueReceive(pcmChunkQueueHandle, &chnk, pdMS_TO_TICKS(2000));
      if (ret != pdFAIL) {
        //				ESP_LOGW(TAG, "got pcm chunk");
      }
    } else {
      //			ESP_LOGW(TAG, "already retrieved chunk needs
      // service");
      ret = pdPASS;
    }

    if (ret != pdFAIL) {
      if (server_now(&serverNow) >= 0) {
        age = serverNow -
              ((int64_t)chnk->timestamp.sec * 1000000LL +
               (int64_t)chnk->timestamp.usec) -
              (int64_t)buffer_ms_local + (int64_t)i2sDmaLAtency +
              (int64_t)DAC_OUT_BUFFER_TIME_US;
      } else {
        // ESP_LOGW(TAG, "couldn't get server now");

        if (chnk != NULL) {
          free(chnk->payload);
          free(chnk);
          chnk = NULL;
        }

        vTaskDelay(pdMS_TO_TICKS(1));

        continue;
      }

      /*
      // wait for early time syncs to be ready
      int tmp = latency_buffer_full();
      if ( tmp <= 0 ) {
              if (tmp < 0) {
                      ESP_LOGW(TAG, "test");

                      vTaskDelay(1);

                      continue;
              }

              if (age >= 0) {
                      if (chnk != NULL) {
                              free(chnk->payload);
                              free(chnk);
                              chnk = NULL;
                      }
              }

              ESP_LOGW(TAG, "diff buffer not full");

              vTaskDelay( pdMS_TO_TICKS(10) );

              continue;
      }
      */

      if (chnk != NULL) {
        p_payload = chnk->payload;
        size = chnk->size;
      }

      if (age < 0) {  // get initial sync using hardware timer
        if (initialSync == 0) {
          if (MEDIANFILTER_Init(&shortMedianFilter)) {
            ESP_LOGE(
                TAG,
                "snapcast_sync_task: couldn't init shortMedianFilter. STOP");

            return;
          }

          /*
          ESP_LOGI(TAG, "age before sync %lld", age);

          // ensure enough time for resync
          if (age > -(int64_t)WIRE_CHUNK_DURATION_MS * 1000 * 2) {
                  if (chnk != NULL) {
                          free(chnk->payload);
                          free(chnk);
                          chnk = NULL;
                  }

                  ESP_LOGI(TAG, "trying to resync");

                  vTaskDelay(1);

                  continue;
          }
          */

          adjust_apll(0);  // reset to normal playback speed

          i2s_zero_dma_buffer(I2S_PORT);
          i2s_stop(I2S_PORT);

          size_t written;
          if (i2s_write(I2S_PORT, p_payload, (size_t)size, &written, 0) !=
              ESP_OK) {
            ESP_LOGE(TAG, "i2s_playback_task: I2S write error");
          }
          size -= written;
          p_payload += written;

          if ((chnk != NULL) && (size == 0)) {
            free(chnk->payload);
            free(chnk);
            chnk = NULL;
          }

          // tg0_timer1_start((-age * 10) - alarmValSub));	// alarm a
          // little earlier to account for context switch duration from
          // freeRTOS, timer with 100ns ticks
          tg0_timer1_start(-age -
                           alarmValSub);  // alarm a little earlier to account
                                          // for context switch duration from
                                          // freeRTOS, timer with 1µs ticks

          // Wait to be notified of a timer interrupt.
          xTaskNotifyWait(pdFALSE,         // Don't clear bits on entry.
                          pdFALSE,         // Don't clear bits on exit.
                          &notifiedValue,  // Stores the notified value.
                          portMAX_DELAY);
          // vTaskDelay( pdMS_TO_TICKS(-age / 1000) );

          i2s_start(I2S_PORT);

          // get timer value so we can get the real age
          timer_get_counter_value(TIMER_GROUP_0, TIMER_1, &timer_val);
          timer_pause(TIMER_GROUP_0, TIMER_1);

          // get actual age after alarm
          // age = ((int64_t)timer_val - (-age) * 10) / 10;	// timer with
          // 100ns ticks
          age = (int64_t)timer_val - (-age);  // timer with 1µs ticks

          if (i2s_write(I2S_PORT, p_payload, (size_t)size, &written,
                        portMAX_DELAY) != ESP_OK) {
            ESP_LOGE(TAG, "i2s_playback_task: I2S write error");
          }
          size -= written;
          p_payload += written;

          if ((chnk != NULL) && (size == 0)) {
            free(chnk->payload);
            free(chnk);
            chnk = NULL;

            ret = xQueueReceive(pcmChunkQueueHandle, &chnk, portMAX_DELAY);
            if (ret != pdFAIL) {
              p_payload = chnk->payload;
              size = chnk->size;
              if (i2s_write(I2S_PORT, p_payload, (size_t)size, &written,
                            portMAX_DELAY) != ESP_OK) {
                ESP_LOGE(TAG, "i2s_playback_task: I2S write error");
              }
              size -= written;
              p_payload += written;

              if ((chnk != NULL) && (size == 0)) {
                free(chnk->payload);
                free(chnk);
                chnk = NULL;
              }
            }
          }

          initialSync = 1;

          ESP_LOGI(TAG, "initial sync %lldus", age);

          continue;
        }
      } else if ((age > 0) && (initialSync == 0)) {
        if (chnk != NULL) {
          free(chnk->payload);
          free(chnk);
          chnk = NULL;
        }

        int64_t t;
        get_diff_to_server(&t);
        ESP_LOGW(TAG, "RESYNCING HARD 1 %lldus, %lldus", age, t);

        dir = 0;

        initialSync = 0;
        alarmValSub = 0;

        continue;
      }

      if (initialSync == 1) {
        const uint8_t enableControlLoop = 1;
        const int64_t age_expect = -chunkDuration_us * 2;
        const int64_t maxOffset = 100;              //µs, softsync 1
        const int64_t maxOffset_dropSample = 1000;  //µs, softsync 2
        const int64_t hardResyncThreshold = 3000;   //µs, hard sync

        avg = MEDIANFILTER_Insert(&shortMedianFilter, age + (-age_expect));
        if (MEDIANFILTER_isFull(&shortMedianFilter) == 0) {
          avg = age + (-age_expect);
        } else {
          // resync hard if we are off too far
          if ((avg < -hardResyncThreshold) || (avg > hardResyncThreshold) ||
              (initialSync == 0)) {
            if (chnk != NULL) {
              free(chnk->payload);
              free(chnk);
              chnk = NULL;
            }

            int64_t t;
            get_diff_to_server(&t);
            ESP_LOGW(TAG, "RESYNCING HARD 2 %lldus, %lldus, %lldus", age, avg,
                     t);

            initialSync = 0;
            alarmValSub = 0;

            i2sDmaBufferCnt = i2sDmaBufCnt * 1;  // ensure dma is empty
            do {
              // wait until DMA queue is empty
              ret = xQueueReceive(i2s_event_queue, &i2sEvent, portMAX_DELAY);
              if (ret != pdFAIL) {
                if (i2sEvent.type == I2S_EVENT_TX_DONE) {
                  ESP_LOGI(TAG, "I2S_EVENT_TX_DONE, %u", i2sDmaBufferCnt);

                  i2sDmaBufferCnt--;
                }
              }
            } while (i2sDmaBufferCnt > 0);

            continue;
          }
        }

        int samples = 1;
        int sampleSize = 4;
        int ageDiff = 0;
        size_t written;

        if (enableControlLoop == 1) {
          if (avg < -maxOffset) {  // we are early
            dir = -1;

            /*
            //if ( MEDIANFILTER_isFull(&shortMedianFilter))
            {
                    if (avg < -maxOffset_dropSample) {
                            //ageDiff = (int)(age_expect - avg);
                            ageDiff = -(int)avg;
                            samples = ageDiff / (sampleDuration_ns / 1000);
                            if (samples > 4) {
                                    samples = 4;
                            }

                            ESP_LOGI(TAG, "insert %d samples", samples);

                            // TODO: clean insert at periodic positions, so
            stretching won't be audible. char *newBuf = NULL; newBuf = (char
            *)heap_caps_malloc(sizeof(char) * (size + samples * sampleSize),
            MALLOC_CAP_SPIRAM); memcpy(newBuf, p_payload, size);
                            free(p_payload);
                            p_payload = newBuf;
                            memcpy(&p_payload[size], &p_payload[size - 1 -
            samples * sampleSize], samples * sampleSize); size += samples *
            sampleSize;

                            chnk->payload = p_payload;
                            chnk->size = size;
                    }
            }
            */
          } else if ((avg >= -maxOffset) && (avg <= maxOffset)) {
            dir = 0;
          } else if (avg > maxOffset) {  // we are late
            dir = 1;

            /*
            //if ( MEDIANFILTER_isFull(&shortMedianFilter))
            {
                    if (avg > maxOffset_dropSample) {
                            //ageDiff = (int)(avg - age_expect);
                            ageDiff = (int)avg;
                            samples = ageDiff / (sampleDuration_ns / 1000);
                            if (samples > 4) {
                                    samples = 4;
                            }

                            if (size >= samples * sampleSize) {
                                    // drop samples
                                    p_payload += samples * sampleSize;
                                    size -= samples * sampleSize;

                                    ESP_LOGI(TAG, "drop %d samples", samples);
                            }
                    }
            }
            */
          }

          adjust_apll(dir);
        }

        if (i2s_write(I2S_PORT, p_payload, (size_t)size, &written,
                      portMAX_DELAY) != ESP_OK) {
          ESP_LOGE(TAG, "i2s_playback_task: I2S write error");
        }
        size -= written;
        p_payload += written;

        if ((chnk != NULL) && (size == 0)) {
          free(chnk->payload);
          free(chnk);
          chnk = NULL;
        }
      }

      int64_t t;
      get_diff_to_server(&t);
      ESP_LOGI(TAG, "%d: %lldus, %lldus %lldus", dir, age, avg, t);

      if ((chnk != NULL) && (size == 0)) {
        free(chnk->payload);
        free(chnk);
        chnk = NULL;
      }
    } else {
      int64_t t;
      get_diff_to_server(&t);
      ESP_LOGE(
          TAG,
          "Couldn't get PCM chunk, recv: messages waiting %d, latency %lldus",
          uxQueueMessagesWaiting(pcmChunkQueueHandle), t);

      dir = 0;

      initialSync = 0;
      alarmValSub = 0;
    }
  }
}
