/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "i2c_bus.h"
#include "board.h"
#include "esp_log.h"
#include "tas57xx.h"
#include "tas57xx_reg_cfg.h"

static const char *TAG = "TAS57XX";

#define TAS57XX_BASE_ADDR     0x98
#define TAS57XX_RST_GPIO      get_pa_enable_gpio()
#define TAS57XX_VOLUME_MAX    255
#define TAS57XX_VOLUME_MIN    0

#define TAS57XX_ASSERT(a, format, b, ...) \
    if ((a) != 0) { \
        ESP_LOGE(TAG, format, ##__VA_ARGS__); \
        return b;\
    }

esp_err_t tas57xx_ctrl(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state);
esp_err_t tas57xx_config_iface(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface);
static i2c_bus_handle_t     i2c_handler;
static int tas57xx_addr;

/*
 * i2c default configuration
 */
static i2c_config_t i2c_cfg = {
    .mode = I2C_MODE_MASTER,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 100000,
};

/*
 * Operate fuction of PA
 */
audio_hal_func_t AUDIO_CODEC_TAS57XX_DEFAULT_HANDLE = {
    .audio_codec_initialize = tas57xx_init,
    .audio_codec_deinitialize = tas57xx_deinit,
    .audio_codec_ctrl = tas57xx_ctrl,
    .audio_codec_config_iface = tas57xx_config_iface,
    .audio_codec_set_mute = tas57xx_set_mute,
    .audio_codec_set_volume = tas57xx_set_volume,
    .audio_codec_get_volume = tas57xx_get_volume,
    .audio_hal_lock = NULL,
    .handle = NULL,
};

static esp_err_t tas57xx_transmit_registers(const tas57xx_cfg_reg_t *conf_buf, int size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while (i < size) {
		ret = i2c_bus_write_bytes(
			i2c_handler, tas57xx_addr,
			(unsigned char *)(&conf_buf[i].offset), 1,
			(unsigned char *)(&conf_buf[i].value), 1);
		i++;
	}
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to load configuration to tas57xx");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "%s:  write %d reg done", __FUNCTION__, i);
    return ret;
}

esp_err_t tas57xx_init(audio_hal_codec_config_t *codec_cfg)
{
    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "Power ON CODEC with GPIO %d", TAS57XX_RST_GPIO);
	// probably unnecessary...
	/*
    gpio_config_t io_conf;
    io_conf.pin_bit_mask = BIT64(TAS57XX_RST_GPIO);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(TAS57XX_RST_GPIO, 0);
    vTaskDelay(20 / portTICK_RATE_MS);
    gpio_set_level(TAS57XX_RST_GPIO, 1);
    vTaskDelay(200 / portTICK_RATE_MS);
	*/

    ret = get_i2c_pins(I2C_NUM_0, &i2c_cfg);
    i2c_handler = i2c_bus_create(I2C_NUM_0, &i2c_cfg);
    if (i2c_handler == NULL) {
        ESP_LOGW(TAG, "failed to create i2c bus handler\n");
        return ESP_FAIL;
    }

	uint8_t data[] = {0, 0};
	for (int i=0; i<4; i++)
	{
		tas57xx_addr = TAS57XX_BASE_ADDR + 2*i;
		ESP_LOGI(TAG, "Looking for a tas57xx chip at address 0x%x", tas57xx_addr);
		ret = i2c_bus_write_data(i2c_handler, tas57xx_addr, data, 0);
		if (ret == ESP_OK) {
			ESP_LOGI(TAG, "Found a tas57xx chip at address 0x%x", tas57xx_addr);
			break;
		}
	}

    TAS57XX_ASSERT(ret, "Fail to detect tas57xx PA", ESP_FAIL);
    ret |= tas57xx_transmit_registers(tas57xx_init_seq, sizeof(tas57xx_init_seq) / sizeof(tas57xx_init_seq[0]));

    TAS57XX_ASSERT(ret, "Fail to iniitialize tas57xx PA", ESP_FAIL);
    return ret;
}

esp_err_t tas57xx_set_volume(int vol)
{
    // vol is given as 1/2dB step with
	// 255: -inf (mute)
    // 254: -103dB
	// 48:  0dB
    // 0 (max): +24dB

    if (vol < TAS57XX_VOLUME_MIN) {
        vol = TAS57XX_VOLUME_MIN;
    }
    if (vol > TAS57XX_VOLUME_MAX) {
        vol = TAS57XX_VOLUME_MAX;
    }
    uint8_t cmd[2] = {0, 0};
    esp_err_t ret = ESP_OK;

    cmd[1] = vol;

    cmd[0] = TAS57XX_REG_VOL_L;
    ret = i2c_bus_write_bytes(i2c_handler, tas57xx_addr, &cmd[0], 1, &cmd[1], 1);
    cmd[0] = TAS57XX_REG_VOL_R;
    ret |= i2c_bus_write_bytes(i2c_handler, tas57xx_addr, &cmd[0], 1, &cmd[1], 1);
    ESP_LOGW(TAG, "Volume set to 0x%x", cmd[1]);
    return ret;
}

esp_err_t tas57xx_get_volume(int *value)
{
    /// FIXME: Got the digit volume is not right.
    uint8_t cmd[2] = {TAS57XX_REG_VOL_L, 0x00};
    esp_err_t ret = i2c_bus_read_bytes(i2c_handler, tas57xx_addr, &cmd[0], 1, &cmd[1], 1);
    TAS57XX_ASSERT(ret, "Fail to get volume", ESP_FAIL);
    ESP_LOGI(TAG, "Volume is %d", cmd[1]);
    *value = cmd[1];
    return ret;
}

esp_err_t tas57xx_set_mute(bool enable)
{
    esp_err_t ret = ESP_OK;
    uint8_t cmd[2] = {TAS57XX_REG_MUTE, 0x00};
    ret |= i2c_bus_read_bytes(i2c_handler, tas57xx_addr, &cmd[0], 1, &cmd[1], 1);

    if (enable) {
        cmd[1] |= 0x11;
    } else {
        cmd[1] &= (~0x11);
    }
    ret |= i2c_bus_write_bytes(i2c_handler, tas57xx_addr, &cmd[0], 1, &cmd[1], 1);

    TAS57XX_ASSERT(ret, "Fail to set mute", ESP_FAIL);
    return ret;
}

esp_err_t tas57xx_get_mute(bool *enabled)
{
    esp_err_t ret = ESP_OK;
    uint8_t cmd[2] = {TAS57XX_REG_MUTE, 0x00};
    ret |= i2c_bus_read_bytes(i2c_handler, tas57xx_addr, &cmd[0], 1, &cmd[1], 1);

    TAS57XX_ASSERT(ret, "Fail to get mute", ESP_FAIL);
    *enabled = (bool) (cmd[1] & 0x11);
    ESP_LOGI(TAG, "Get mute value: %s", *enabled ? "muted" : "unmuted");
    return ret;
}

esp_err_t tas57xx_deinit(void)
{
    // TODO
    return ESP_OK;
}

esp_err_t tas57xx_ctrl(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    // TODO
    return ESP_OK;
}

esp_err_t tas57xx_config_iface(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    //TODO
    return ESP_OK;
}
