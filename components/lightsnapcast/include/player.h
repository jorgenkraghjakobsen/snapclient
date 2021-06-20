#ifndef __PLAYER_H__
#define __PLAYER_H__

#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"
#include "snapcast.h"

// TODO: make the following configurable through menuconfig
// @ 48kHz, 2ch, 16bit audio data and 24ms wirechunks (hardcoded for now) we
// expect 0.024 * 2 * 16/8 * 48000 = 4608 Bytes
#define WIRE_CHUNK_DURATION_MS CONFIG_WIRE_CHUNK_DURATION_MS
#define SAMPLE_RATE CONFIG_PCM_SAMPLE_RATE
#define CHANNELS CONFIG_CHANNELS
#define BITS_PER_SAMPLE CONFIG_BITS_PER_SAMPLE

#define I2S_PORT I2S_NUM_0

#define DAC_OUT_BUFFER_TIME_US 0

#define MEDIAN_FILTER_LONG_BUF_LEN 299

#define SHORT_BUFFER_LEN 99

typedef struct pcm_chunk_fragment pcm_chunk_fragment_t;
struct pcm_chunk_fragment {
  size_t size;
  char *payload;
  pcm_chunk_fragment_t *nextFragment;
};

typedef struct pcm_chunk_message {
  tv_t timestamp;
  pcm_chunk_fragment_t *fragment;
} pcm_chunk_message_t;

QueueHandle_t init_player(void);
int deinit_player(void);

int8_t insert_pcm_chunk(wire_chunk_message_t *decodedWireChunk);
int8_t free_pcm_chunk(pcm_chunk_message_t *pcmChunk);

int8_t player_latency_insert(int64_t newValue);
int8_t player_notify_buffer_ms(uint32_t ms);

int8_t reset_latency_buffer(void);
int8_t latency_buffer_full(void);
int8_t get_diff_to_server(int64_t *tDiff);
int8_t server_now(int64_t *sNow);

// void tg0_timer_init(void);
// void snapcast_sync_task(void *pvParameters);

#endif  // __PLAYER_H__
