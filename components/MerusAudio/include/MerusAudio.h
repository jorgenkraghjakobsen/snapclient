#ifndef _MERUSAUDIO_H_ 
#define _MERUSAUDIO_H_
  
void setup_ma120x0(void);
void setup_ma120(void);
void ma120_read_error(uint8_t i2c_addr);
void ma120_setup_audio(uint8_t i2c_addr);

void i2c_master_init(void);

esp_err_t ma_write_byte(uint8_t i2c_addr, uint8_t prot, uint16_t address, uint8_t value); 
esp_err_t ma_write(uint8_t i2c_addr, uint8_t prot, uint16_t address, uint8_t *wbuf, uint8_t n);
 
uint8_t ma_read_byte(uint8_t i2c_addr, uint8_t prot, uint16_t address);
esp_err_t ma_read(uint8_t i2c_addr, uint8_t prot, uint16_t address, uint8_t *rbuf, uint8_t n);


#endif /* _MERUSAUDIO_H_  */ 

  
