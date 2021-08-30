#ifndef _MERUSAUDIO_H_
#define _MERUSAUDIO_H_

#include <stdint.h>
#include <esp_err.h>
#include "board.h"

void setup_ma120x0(void);
//void setup_ma120(void);
//void ma120_read_error(uint8_t i2c_addr);
//void ma120_setup_audio(uint8_t i2c_addr);

esp_err_t ma120x0_init(audio_hal_codec_config_t *codec_cfg);
esp_err_t ma120x0_deinit(void);
esp_err_t ma120x0_set_volume(int vol);
esp_err_t ma120x0_get_volume(int *value);
esp_err_t ma120x0_set_mute(bool enable);
esp_err_t ma120x0_get_mute(bool *enabled);
esp_err_t ma120x0_ctrl(audio_hal_codec_mode_t, audio_hal_ctrl_t);
esp_err_t ma120x0_config_iface(audio_hal_codec_mode_t , audio_hal_codec_i2s_iface_t *);

void i2c_master_init(void);

esp_err_t ma_write_byte(uint8_t i2c_addr, uint8_t prot, uint16_t address,
                        uint8_t value);
esp_err_t ma_write(uint8_t i2c_addr, uint8_t prot, uint16_t address,
                   uint8_t *wbuf, uint8_t n);

esp_err_t ma_read_byte(uint8_t i2c_addr, uint8_t prot, uint16_t address, uint8_t *data);
esp_err_t ma_read(uint8_t i2c_addr, uint8_t prot, uint16_t address,
                  uint8_t *rbuf, uint8_t n);

#endif /* _MERUSAUDIO_H_  */
