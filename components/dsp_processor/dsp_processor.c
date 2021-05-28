

#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#include "driver/i2s.h"
#include "dsps_biquad.h"
#include "dsps_biquad_gen.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
//#include "websocket_if.h"
#include "driver/dac.h"
#include "driver/i2s.h"
#include "dsp_processor.h"
#include "hal/i2s_hal.h"
//#include "adc1_i2s_private.h"
#include "board_pins_config.h"

#ifdef CONFIG_USE_BIQUAD_ASM
#define BIQUAD dsps_biquad_f32_ae32
#else
#define BIQUAD dsps_biquad_f32
#endif

static const char *TAG = "dspProc";

static uint32_t bits_per_sample = CONFIG_BITS_PER_SAMPLE;

// TODO: allocate these buffers dynamically from heap
static float sbuffer0[1024];
static float sbuffer1[1024];
static float sbuffer2[1024];
static float sbufout0[1024];
static float sbufout1[1024];
static float sbufout2[1024];
static float sbuftmp0[1024];
static uint8_t dsp_audio[4 * 1024];
static uint8_t dsp_audio1[4 * 1024];

extern uint8_t muteCH[4];

ptype_t bq[8];

/*
void setup_dsp_i2s(uint32_t sample_rate, bool slave_i2s) {
  i2s_config_t i2s_config0 = {
      .mode = I2S_MODE_MASTER | I2S_MODE_TX,  // Only TX
      .sample_rate = sample_rate,
      .bits_per_sample = bits_per_sample,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,  // 2-channels
      .communication_format = I2S_COMM_FORMAT_I2S,
      .dma_buf_count = 8,
      .dma_buf_len = 480,
      .intr_alloc_flags = 1,  // Default interrupt priority
      .use_apll = true,
      .fixed_mclk = 0,
      .tx_desc_auto_clear = true  // Auto clear tx descriptor on underflow
  };

  i2s_pin_config_t pin_config0;
  get_i2s_pins(I2S_NUM_0, &pin_config0);

  i2s_driver_install(0, &i2s_config0, 1, &i2s_queue);
  i2s_zero_dma_buffer(0);
  i2s_set_pin(0, &pin_config0);
  // gpio_set_drive_capability(CONFIG_MASTER_I2S_BCK_PIN, 0);
  // gpio_set_drive_capability(CONFIG_MASTER_I2S_LRCK_PIN, 0);
  // gpio_set_drive_capability(CONFIG_MASTER_I2S_DATAOUT_PIN, 0);
  ESP_LOGI("I2S", "I2S interface master setup");
  if (slave_i2s) {
    i2s_config_t i2s_config1 = {
        .mode = I2S_MODE_SLAVE | I2S_MODE_TX,  // Only TX - Slave channel
        .sample_rate = sample_rate,
        .bits_per_sample = bits_per_sample,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,  // 2-channels
        .communication_format = I2S_COMM_FORMAT_I2S,
        .dma_buf_count = 8,
        .dma_buf_len = 480,
        .use_apll = true,
        .fixed_mclk = 0,
        .tx_desc_auto_clear = true  // Auto clear tx descriptor on underflow
    };
    i2s_pin_config_t pin_config1;
    get_i2s_pins(I2S_NUM_1, &pin_config1);
    i2s_driver_install(I2S_NUM_1, &i2s_config1, 7, &i2s_queue);
    i2s_zero_dma_buffer(1);
    i2s_set_pin(1, &pin_config1);
  }
}
*/

int dsp_processor(char *audio, size_t chunk_size, dspFlows_t dspFlow) {
  double dynamic_vol = 1.0;
  int16_t len = chunk_size / 4;

  // ESP_LOGI(TAG,
  //        "got data %p, %d, %u", audio, chunk_size, dspFlow);

  for (uint16_t i = 0; i < len; i++) {
    sbuffer0[i] =
        dynamic_vol * 0.5 *
        ((float)((int16_t)(audio[i * 4 + 1] << 8) + audio[i * 4 + 0])) / 32768;
    sbuffer1[i] =
        dynamic_vol * 0.5 *
        ((float)((int16_t)(audio[i * 4 + 3] << 8) + audio[i * 4 + 2])) / 32768;
    sbuffer2[i] = ((sbuffer0[i] / 2) + (sbuffer1[i] / 2));
  }

  switch (dspFlow) {
    case dspfStereo: {
      for (uint16_t i = 0; i < len; i++) {
        audio[i * 4 + 0] = (muteCH[0] == 1) ? 0 : audio[i * 4 + 0];
        audio[i * 4 + 1] = (muteCH[0] == 1) ? 0 : audio[i * 4 + 1];
        audio[i * 4 + 2] = (muteCH[1] == 1) ? 0 : audio[i * 4 + 2];
        audio[i * 4 + 3] = (muteCH[1] == 1) ? 0 : audio[i * 4 + 3];
      }
    } break;

    case dspfBassBoost: {  // CH0 low shelf 6dB @ 400Hz
      BIQUAD(sbuffer0, sbufout0, len, bq[6].coeffs, bq[6].w);
      BIQUAD(sbuffer1, sbufout1, len, bq[7].coeffs, bq[7].w);
      int16_t valint[2];
      for (uint16_t i = 0; i < len; i++) {
        valint[0] =
            (muteCH[0] == 1) ? (int16_t)0 : (int16_t)(sbufout0[i] * 32768);
        valint[1] =
            (muteCH[1] == 1) ? (int16_t)0 : (int16_t)(sbufout1[i] * 32768);
        dsp_audio[i * 4 + 0] = (valint[0] & 0xff);
        dsp_audio[i * 4 + 1] = ((valint[0] & 0xff00) >> 8);
        dsp_audio[i * 4 + 2] = (valint[1] & 0xff);
        dsp_audio[i * 4 + 3] = ((valint[1] & 0xff00) >> 8);
      }

      // TODO: this copy could be avoided if dsp_audio buffers are
      // allocated dynamically and pointers are exchanged after
      // audio was freed
      memcpy(audio, dsp_audio, chunk_size);

    } break;

    case dspfBiamp: {
      // Process audio ch0 LOW PASS FILTER
      BIQUAD(sbuffer0, sbuftmp0, len, bq[0].coeffs, bq[0].w);
      BIQUAD(sbuftmp0, sbufout0, len, bq[1].coeffs, bq[1].w);

      // Process audio ch1 HIGH PASS FILTER
      BIQUAD(sbuffer0, sbuftmp0, len, bq[2].coeffs, bq[2].w);
      BIQUAD(sbuftmp0, sbufout1, len, bq[3].coeffs, bq[3].w);

      int16_t valint[2];
      for (uint16_t i = 0; i < len; i++) {
        valint[0] =
            (muteCH[0] == 1) ? (int16_t)0 : (int16_t)(sbufout0[i] * 32768);
        valint[1] =
            (muteCH[1] == 1) ? (int16_t)0 : (int16_t)(sbufout1[i] * 32768);
        dsp_audio[i * 4 + 0] = (valint[0] & 0xff);
        dsp_audio[i * 4 + 1] = ((valint[0] & 0xff00) >> 8);
        dsp_audio[i * 4 + 2] = (valint[1] & 0xff);
        dsp_audio[i * 4 + 3] = ((valint[1] & 0xff00) >> 8);
      }

      // TODO: this copy could be avoided if dsp_audio buffers are
      // allocated dynamically and pointers are exchanged after
      // audio was freed
      memcpy(audio, dsp_audio, chunk_size);

    } break;

    case dspf2DOT1: {  // Process audio L + R LOW PASS FILTER
      BIQUAD(sbuffer2, sbuftmp0, len, bq[0].coeffs, bq[0].w);
      BIQUAD(sbuftmp0, sbufout2, len, bq[1].coeffs, bq[1].w);

      // Process audio L HIGH PASS FILTER
      BIQUAD(sbuffer0, sbuftmp0, len, bq[2].coeffs, bq[2].w);
      BIQUAD(sbuftmp0, sbufout0, len, bq[3].coeffs, bq[3].w);

      // Process audio R HIGH PASS FILTER
      BIQUAD(sbuffer1, sbuftmp0, len, bq[4].coeffs, bq[4].w);
      BIQUAD(sbuftmp0, sbufout1, len, bq[5].coeffs, bq[5].w);

      int16_t valint[5];
      for (uint16_t i = 0; i < len; i++) {
        valint[0] =
            (muteCH[0] == 1) ? (int16_t)0 : (int16_t)(sbufout0[i] * 32768);
        valint[1] =
            (muteCH[1] == 1) ? (int16_t)0 : (int16_t)(sbufout1[i] * 32768);
        valint[2] =
            (muteCH[2] == 1) ? (int16_t)0 : (int16_t)(sbufout2[i] * 32768);
        dsp_audio[i * 4 + 0] = (valint[2] & 0xff);
        dsp_audio[i * 4 + 1] = ((valint[2] & 0xff00) >> 8);
        dsp_audio[i * 4 + 2] = 0;
        dsp_audio[i * 4 + 3] = 0;

        dsp_audio1[i * 4 + 0] = (valint[0] & 0xff);
        dsp_audio1[i * 4 + 1] = ((valint[0] & 0xff00) >> 8);
        dsp_audio1[i * 4 + 2] = (valint[1] & 0xff);
        dsp_audio1[i * 4 + 3] = ((valint[1] & 0xff00) >> 8);
      }

      // TODO: this copy could be avoided if dsp_audio buffers are
      // allocated dynamically and pointers are exchanged after
      // audio was freed
      memcpy(audio, dsp_audio, chunk_size);

      ESP_LOGW(TAG, "Don't know what to do with dsp_audio1");

    } break;

    case dspfFunkyHonda: {  // Process audio L + R LOW PASS FILTER
      BIQUAD(sbuffer2, sbuftmp0, len, bq[0].coeffs, bq[0].w);
      BIQUAD(sbuftmp0, sbufout2, len, bq[1].coeffs, bq[1].w);

      // Process audio L HIGH PASS FILTER
      BIQUAD(sbuffer0, sbuftmp0, len, bq[2].coeffs, bq[2].w);
      BIQUAD(sbuftmp0, sbufout0, len, bq[3].coeffs, bq[3].w);

      // Process audio R HIGH PASS FILTER
      BIQUAD(sbuffer1, sbuftmp0, len, bq[4].coeffs, bq[4].w);
      BIQUAD(sbuftmp0, sbufout1, len, bq[5].coeffs, bq[5].w);

      uint16_t scale = 16384;  // 32768
      int16_t valint[5];
      for (uint16_t i = 0; i < len; i++) {
        valint[0] =
            (muteCH[0] == 1) ? (int16_t)0 : (int16_t)(sbufout0[i] * scale);
        valint[1] =
            (muteCH[1] == 1) ? (int16_t)0 : (int16_t)(sbufout1[i] * scale);
        valint[2] =
            (muteCH[2] == 1) ? (int16_t)0 : (int16_t)(sbufout2[i] * scale);
        valint[3] = valint[0] + valint[2];
        valint[4] = -valint[2];
        valint[5] = -valint[1] - valint[2];
        dsp_audio[i * 4 + 0] = (valint[3] & 0xff);
        dsp_audio[i * 4 + 1] = ((valint[3] & 0xff00) >> 8);
        dsp_audio[i * 4 + 2] = (valint[2] & 0xff);
        dsp_audio[i * 4 + 3] = ((valint[2] & 0xff00) >> 8);

        dsp_audio1[i * 4 + 0] = (valint[4] & 0xff);
        dsp_audio1[i * 4 + 1] = ((valint[4] & 0xff00) >> 8);
        dsp_audio1[i * 4 + 2] = (valint[5] & 0xff);
        dsp_audio1[i * 4 + 3] = ((valint[5] & 0xff00) >> 8);
      }

      // TODO: this copy could be avoided if dsp_audio buffers are
      // allocated dynamically and pointers are exchanged after
      // audio was freed
      memcpy(audio, dsp_audio, chunk_size);

      ESP_LOGW(TAG, "Don't know what to do with dsp_audio1");

    } break;

    default: { } break; }

  return 0;
}

// ESP32 DSP processor
//======================================================
// Each time a buffer of audio is passed to the DSP - samples are
// processed according to a dynamic list of audio processing nodes.

// Each audio processor node consist of a data struct holding the
// required weights and states for processing an automomous processing
// function. The high level parameters is maintained in the structure
// as well

// Release - Prove off concept
// ----------------------------------------
// Fixed 2x2 biquad flow Xover for biAmp systems
// Interface for cross over frequency and level

void dsp_setup_flow(double freq, uint32_t samplerate) {
  float f = freq / samplerate / 2.0;

  bq[0] = (ptype_t){LPF, f, 0, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
  bq[1] = (ptype_t){LPF, f, 0, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
  bq[2] = (ptype_t){HPF, f, 0, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
  bq[3] = (ptype_t){HPF, f, 0, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
  bq[4] = (ptype_t){HPF, f, 0, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
  bq[5] = (ptype_t){HPF, f, 0, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
  bq[6] = (ptype_t){LOWSHELF, f, 6, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};
  bq[7] = (ptype_t){LOWSHELF, f, 6, 0.707, NULL, NULL, {0, 0, 0, 0, 0}, {0, 0}};

  pnode_t *aflow = NULL;
  aflow = malloc(sizeof(pnode_t));
  if (aflow == NULL) {
    printf("Could not create node");
  }

  for (uint8_t n = 0; n <= 7; n++) {
    switch (bq[n].filtertype) {
      case LOWSHELF:
        dsps_biquad_gen_lowShelf_f32(bq[n].coeffs, bq[n].freq, bq[n].gain,
                                     bq[n].q);
        break;
      case LPF:
        dsps_biquad_gen_lpf_f32(bq[n].coeffs, bq[n].freq, bq[n].q);
        break;
      case HPF:
        dsps_biquad_gen_hpf_f32(bq[n].coeffs, bq[n].freq, bq[n].q);
        break;
      default:
        break;
    }
    for (uint8_t i = 0; i <= 4; i++) {
      printf("%.6f ", bq[n].coeffs[i]);
    }
    printf("\n");
  }
}

void dsp_set_xoverfreq(uint8_t freqh, uint8_t freql, uint32_t samplerate) {
  float freq = freqh * 256 + freql;
  printf("%f\n", freq);
  float f = freq / samplerate / 2.;
  for (int8_t n = 0; n <= 5; n++) {
    bq[n].freq = f;
    switch (bq[n].filtertype) {
      case LPF:
        for (uint8_t i = 0; i <= 4; i++) {
          printf("%.6f ", bq[n].coeffs[i]);
        }
        printf("\n");
        dsps_biquad_gen_lpf_f32(bq[n].coeffs, bq[n].freq, bq[n].q);
        for (uint8_t i = 0; i <= 4; i++) {
          printf("%.6f ", bq[n].coeffs[i]);
        }
        printf("%f \n", bq[n].freq);
        break;
      case HPF:
        dsps_biquad_gen_hpf_f32(bq[n].coeffs, bq[n].freq, bq[n].q);
        break;
      default:
        break;
    }
  }
}
