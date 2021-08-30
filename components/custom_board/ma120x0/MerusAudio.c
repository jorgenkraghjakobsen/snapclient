//
// MA120x0P ESP32 Driver
//
// Merus Audio - September 2018
// Written by Joergen Kragh Jakobsen, jkj@myrun.dk
//
// Register interface thrugh I2C for MA12070P and MA12040P
//   Support a single amplifier/i2c address
//
//

#include "MerusAudio.h"

#include <stdint.h>
#include <stdio.h>

#include "driver/i2c.h"
#include "esp_log.h"
#include "board.h" 
#include "ma120x0.h"
//#include "ma120_rev1_all.h"

#define MA_NENABLE_IO CONFIG_MA120X0_NENABLE_PIN
#define MA_NMUTE_IO CONFIG_MA120X0_NMUTE_PIN
#define MA_NERR_IO CONFIG_MERUS_NERR_PIN
#define MA_NCLIP_IO CONFIG_MERUS_NCLIP_PIN

#define I2C_CHECK(a, str, ret)                                                 \
  if (!(a)) {                                                                  \
    ESP_LOGE(I2C_TAG, "%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
    return (ret);                                                              \
  }

#define I2C_MASTER_NUM I2C_NUM_0    /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ 100000   /*!< I2C master clock frequency */

#define MA120X0_ADDR \
  CONFIG_DAC_I2C_ADDR /*!< slave address for MA120X0 amplifier */

#define WRITE_BIT I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN 0x1           /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0 /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0       /*!< I2C ack value */
#define NACK_VAL 0x1      /*!< I2C nack value */

static i2c_config_t i2c_cfg;

audio_hal_func_t AUDIO_CODEC_MA120X0_DEFAULT_HANDLE = {
    .audio_codec_initialize = ma120x0_init,
    .audio_codec_deinitialize = ma120x0_deinit,
    .audio_codec_ctrl = ma120x0_ctrl,
    .audio_codec_config_iface = ma120x0_config_iface,
    .audio_codec_set_mute = ma120x0_set_mute,
    .audio_codec_set_volume = ma120x0_set_volume,
    .audio_codec_get_volume = ma120x0_get_volume,
    .audio_hal_lock = NULL,
    .handle = NULL,
};

esp_err_t ma120x0_deinit(void) {
  // TODO
  return ESP_OK;
}

esp_err_t ma120x0_ctrl(audio_hal_codec_mode_t mode,
                       audio_hal_ctrl_t ctrl_state) {
  // TODO
  return ESP_OK;
}

esp_err_t ma120x0_config_iface(audio_hal_codec_mode_t mode,
                               audio_hal_codec_i2s_iface_t *iface) {
  // TODO
  return ESP_OK;
}

esp_err_t ma120x0_set_volume(int vol) {
  esp_err_t ret = ESP_OK;
  uint8_t cmd;
  cmd = 128 - vol;
  ma_write_byte(0x20, 1, 64, cmd);
  return ret;
}

esp_err_t ma120x0_get_volume(int *vol) {
  esp_err_t ret = ESP_OK;
  uint8_t rxbuf = 0;
  ma_read_byte(0x20, 1, 64, rxbuf);
  *vol = 128 - rxbuf;
  return ret;
}

esp_err_t ma120x0_set_mute(bool enable) {
  esp_err_t ret = ESP_OK;
  uint8_t nmute = (enable) ? 0 : 1 ; 
  gpio_set_level(MA_NMUTE_IO, nmute);
  return ret;
}

esp_err_t ma120x0_get_mute(bool *enabled) {
  esp_err_t ret = ESP_OK;

  *enabled = false;  // TODO read from register
  return ret;
}

esp_err_t ma120x0_init(audio_hal_codec_config_t *codec_cfg) {
  esp_err_t ret = ESP_OK;
  gpio_config_t io_conf;

  io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << MA_NENABLE_IO | 1ULL << MA_NMUTE_IO);
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;

  printf("setup output %d %d \n", MA_NENABLE_IO, MA_NMUTE_IO);
  gpio_config(&io_conf);

  io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = (1ULL << MA_NCLIP_IO | 1ULL << MA_NERR_IO);
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  printf("setup input %d %d \n", MA_NCLIP_IO, MA_NERR_IO);
  gpio_config(&io_conf);

  gpio_set_level(MA_NMUTE_IO, 0);
  gpio_set_level(MA_NENABLE_IO, 0);
   




  i2c_master_init();

  gpio_set_level(MA_NENABLE_IO, 1);

  uint8_t res = 0; 
  ma_read_byte(MA120X0_ADDR,1,MA_hw_version__a, &res);
  printf("Hardware version: 0x%02x\n",res);

  ma_write_byte(MA120X0_ADDR,1,MA_i2s_format__a,8);          // Set i2s left justified, set audio_proc_enable
  ma_write_byte(MA120X0_ADDR,1,MA_vol_db_master__a,0x28);    // Set vol_db_master

  ma_read_byte(MA120X0_ADDR,1,MA_error__a, &res );
  printf("Errors : 0x%02x\n",res);

  res = 01; // get_MA_audio_in_mode_mon();
  printf("Audio in mode : 0x%02x\n",res);

  printf("Clear errors\n");
  ma_write_byte(MA120X0_ADDR,1,45,0x34);
  ma_write_byte(MA120X0_ADDR,1,45,0x30);
  printf("MA120x0P init done\n");

  printf("\n");

  return ret;
}
static i2c_config_t i2c_cfg = { 
    .mode = I2C_MODE_MASTER,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .sda_io_num = CONFIG_DAC_I2C_SDA,
    .scl_io_num = CONFIG_DAC_I2C_SCL,
    .master.clk_speed = I2C_MASTER_FREQ_HZ,
};

void i2c_master_init() {
  int i2c_master_port = I2C_MASTER_NUM;
  
  esp_err_t res = i2c_param_config(i2c_master_port, &i2c_cfg);
  printf("Driver param setup : %d\n", res);
  res = i2c_driver_install(i2c_master_port, i2c_cfg.mode,
                           I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE,
                           0);
  printf("Driver installed   : %d\n", res);
}

esp_err_t ma_write(uint8_t i2c_addr, uint8_t prot, uint16_t address,
                   uint8_t *wbuf, uint8_t n) {
  bool ack = ACK_VAL;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, i2c_addr << 1 | WRITE_BIT, ACK_CHECK_EN);
  if (prot == 2) {
    i2c_master_write_byte(cmd, (uint8_t)((address & 0xff00) >> 8), ACK_VAL);
    i2c_master_write_byte(cmd, (uint8_t)(address & 0x00ff), ACK_VAL);
  } else {
    i2c_master_write_byte(cmd, (uint8_t)address, ACK_VAL);
  }

  for (int i = 0; i < n; i++) {
    if (i == n - 1) ack = NACK_VAL;
    i2c_master_write_byte(cmd, wbuf[i], ack);
  }
  i2c_master_stop(cmd);
  int ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_FAIL) {
    return ret;
  }
  return ESP_OK;
}

esp_err_t ma_write_byte(uint8_t i2c_addr, uint8_t prot, uint16_t address,
                        uint8_t value) {
  printf("%04x %02x\n", address, value);
  esp_err_t ret = 0;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (i2c_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
  if (prot == 2) {
    i2c_master_write_byte(cmd, (uint8_t)((address & 0xff00) >> 8), ACK_VAL);
    i2c_master_write_byte(cmd, (uint8_t)(address & 0x00ff), ACK_VAL);
  } else {
    i2c_master_write_byte(cmd, (uint8_t)address, ACK_VAL);
  }
  i2c_master_write_byte(cmd, value, ACK_VAL);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_FAIL) {
    printf("ESP_I2C_WRITE ERROR : %d\n", ret);
    return ret;
  }
  return ESP_OK;
}

esp_err_t ma_read(uint8_t i2c_addr, uint8_t prot, uint16_t address,
                  uint8_t *rbuf, uint8_t n) {
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  if (cmd == NULL) {
    printf("ERROR handle null\n");
  }
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (i2c_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
  if (prot == 2) {
    i2c_master_write_byte(cmd, (uint8_t)((address & 0xff00) >> 8), ACK_VAL);
    i2c_master_write_byte(cmd, (uint8_t)(address & 0x00ff), ACK_VAL);
  } else {
    i2c_master_write_byte(cmd, (uint8_t)address, ACK_VAL);
  }
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (i2c_addr << 1) | READ_BIT, ACK_CHECK_EN);
  // if (n == 1 )
  i2c_master_read(cmd, rbuf, n - 1, ACK_VAL);
  // for (uint8_t i = 0;i<n;i++)
  // { i2c_master_read_byte(cmd, rbuf++, ACK_VAL); }
  i2c_master_read_byte(cmd, rbuf + n - 1, NACK_VAL);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 100 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_FAIL) {
    printf("i2c Error read - readback\n");
    return ESP_FAIL;
  }
  return ret;
}

esp_err_t ma_read_byte(uint8_t i2c_addr, uint8_t prot, uint16_t address, uint8_t *value) {
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);  // Send i2c start on bus
  i2c_master_write_byte(cmd, (i2c_addr << 1) | WRITE_BIT, ACK_CHECK_EN);
  if (prot == 2) {
    i2c_master_write_byte(cmd, (uint8_t)((address & 0xff00) >> 8), ACK_VAL);
    i2c_master_write_byte(cmd, (uint8_t)(address & 0x00ff), ACK_VAL);
  } else {
    i2c_master_write_byte(cmd, (uint8_t)address, ACK_VAL);
  }
  i2c_master_start(cmd);  // Repeated start
  i2c_master_write_byte(cmd, (i2c_addr << 1) | READ_BIT, ACK_CHECK_EN);

  i2c_master_read_byte(cmd, value, NACK_VAL);

  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_FAIL) {
    printf("i2c Error read - readback\n");
    return ESP_FAIL;
  }
  return ESP_OK;
}
