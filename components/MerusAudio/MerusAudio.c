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

#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"
#include "driver/i2c.h"

#include "MerusAudio.h"

#include "ma120x0.h"
//#include "ma120_rev1_all.h"


#define MA_NENABLE_IO  CONFIG_MA120X0_NENABLE_PIN
#define MA_ENABLE_IO   CONFIG_MA120X0_ENABLE_PIN
#define MA_NMUTE_IO    CONFIG_MA120X0_NMUTE_PIN
#define MA_NERR_IO     CONFIG_MA120X0_NERR_PIN
#define MA_NCLIP_IO    CONFIG_MA120X0_NCLIP_PIN


static const char* I2C_TAG = "i2c";
#define I2C_CHECK(a, str, ret)  if(!(a)) {                                             \
        ESP_LOGE(I2C_TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);      \
        return (ret);                                                                   \
        }


#define I2C_MASTER_SCL_IO CONFIG_MA120X0_SCL_PIN  //4   /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO CONFIG_MA120X0_SDA_PIN  //0
    /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0
 /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ    100000     /*!< I2C master clock frequency */

#define MA120X0_ADDR  CONFIG_MA120X0_I2C_ADDR  /*!< slave address for MA120X0 amplifier */

#define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */


void setup_ma120x0()
{  // Setup control pins nEnable and nMute
   gpio_config_t io_conf;

   io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
   io_conf.mode = GPIO_MODE_OUTPUT;
   io_conf.pin_bit_mask = (1ULL<<MA_NENABLE_IO | 1ULL<<MA_NMUTE_IO);
   io_conf.pull_down_en = 0;
   io_conf.pull_up_en = 0;

   gpio_config(&io_conf);

   gpio_set_level(MA_NMUTE_IO, 0);
   gpio_set_level(MA_NENABLE_IO, 1);

   i2c_master_init();

   gpio_set_level(MA_NENABLE_IO, 0);

   uint8_t res = ma_read_byte(MA120X0_ADDR,1,MA_hw_version__a);
   printf("Hardware version: 0x%02x\n",res);

   ma_write_byte(MA120X0_ADDR,1,MA_i2s_format__a,8);          // Set i2s left justified, set audio_proc_enable
   ma_write_byte(MA120X0_ADDR,1,MA_vol_db_master__a,0x50);    // Set vol_db_master

   res = ma_read_byte(MA120X0_ADDR,1,MA_error__a);
   printf("Errors : 0x%02x\n",res);

   res = 01; // get_MA_audio_in_mode_mon();
   printf("Audio in mode : 0x%02x\n",res);

   printf("Clear errors\n");
   ma_write_byte(MA120X0_ADDR,1,45,0x34);
   ma_write_byte(MA120X0_ADDR,1,45,0x30);
   printf("MA120x0P init done\n");

   gpio_set_level(MA_NMUTE_IO, 1);
   printf("Unmute\n");
}

void setup_ma120()
{
   gpio_config_t io_conf;

   io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
   io_conf.mode = GPIO_MODE_OUTPUT;
   io_conf.pin_bit_mask = (1ULL<<MA_ENABLE_IO | 1ULL<<MA_NMUTE_IO );
   io_conf.pull_down_en = 0;
   io_conf.pull_up_en = 0;

   printf("setup output %d %d \n",MA_ENABLE_IO, MA_NMUTE_IO);
   gpio_config(&io_conf);

   io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
   io_conf.mode = GPIO_MODE_INPUT;
   io_conf.pin_bit_mask = (1ULL<<MA_NCLIP_IO | 1ULL<<MA_NERR_IO );
   io_conf.pull_down_en = 0;
   io_conf.pull_up_en = 0;


   printf("setup input %d %d \n",MA_NCLIP_IO, MA_NERR_IO);
   gpio_config(&io_conf);


   gpio_set_level(MA_NMUTE_IO, 0);
   gpio_set_level(MA_ENABLE_IO, 0);

   i2c_master_init();

   gpio_set_level(MA_ENABLE_IO, 1);

   uint8_t res = ma_write_byte(0x20,2,1544,0);
   res = ma_read_byte(0x20,2,1544);
   printf("Hardware version: 0x%02x\n",res);
   printf("Scan I2C bus: ");
   for (uint8_t addr = 0x20; addr <= 0x23 ; addr++ )
   {  res = ma_read_byte(addr,2,0);

      printf(" 0x%02x => GEN2 ,",addr);
      //printf("Scan i2c address 0x%02x read address 0 : 0x%02x \n", addr ,res);
   }
   printf("\n");
   uint8_t rxbuf[32];
   uint8_t otp[1024];
   for (uint8_t i=0;i<16; i++)
   { ma_read(0x20,2,0x8000+i*32,rxbuf,32);
     //printf("%04x : ",0x8000+i*32 );
     for (uint8_t j=0; j<32 ; j++ )
     { otp[i*32+j] = rxbuf[j];
     }
   }
   for (uint16_t i=0;i<16*32; i++)
   { if (i%32==0) {
     printf("\n0x%04x : ",0x8000+i);
     }
     printf("%02x ",otp[i]);
   }
   res = ma_write_byte(0x20,2,0x060c,0);
   res = ma_read_byte(0x20,2,0x060c);
   printf("\nHardware version: 0x%02x\n",res);

   printf("\n");
}
uint8_t b[32];
#define CHECK(ADDR,L) ma_read(0x20,2,ADDR,b,L); printf("Check 0x%04x :",ADDR); for (int ci=0;ci<L;ci++) printf(" 0x%02x",b[ci]); printf("\n");

void ma120_setup_audio(uint8_t i2c_addr)
{ uint8_t cmd[32];
  //system("$SCOM w 0x0003 0x50 0x50 0x02");
  cmd[0] = 0x50;
  cmd[1] = 0x50;
  cmd[2] = 0x02;
  ma_write(i2c_addr,2,0x0003,cmd,3);
  //CHECK(0x0003,3);
  //system("$SCOM w 0x0015 0x12");
  ma_write_byte(i2c_addr,2,0x0015, 0x12);
  //CHECK(0x0015,1);
  //system("$SCOM w 0x0025 0x0e");
  ma_write_byte(i2c_addr,2,0x0025, 0x0e);
  //CHECK(0x0025,1);
  //system("$SCOM w 0x003d 0x73");
  //system("$SCOM w 0x003d 0xfe 0x03");
  cmd[0] = 0xfe;
  cmd[1] = 0x03;
  ma_write(i2c_addr,2,0x003d,cmd,2);
  //CHECK(0x003d,2);
  //system("$SCOM w 0x0040 0xf0 0x03");
  cmd[0] = 0xf0;
  cmd[1] = 0x03;
  ma_write(i2c_addr,2,0x0040,cmd,2);
  //CHECK(0x0040,2);
  //system("$SCOM w 0x0043 0xf0 0x07");
  cmd[0] = 0xf0;
  cmd[1] = 0x07;
  ma_write(i2c_addr,2,0x0043,cmd,2);
  //CHECK(0x0043,2);

  //system("$SCOM w 0x0079 0x9b");
  ma_write_byte(i2c_addr,2,0x0079, 0x9b);
  //CHECK(0x0079,1);

  //system("$SCOM w 0x008e 0xef");
  ma_write_byte(i2c_addr,2,0x008e, 0xef);
  //CHECK(0x008e,1);
  //system("$SCOM w 0x0090 0x20 0x32 0x45");
  cmd[0] = 0x20;
  cmd[1] = 0x32;
  cmd[2] = 0x45;
  ma_write(i2c_addr,2,0x0090,cmd,3);
  //CHECK(0x0090,3);
  //system("$SCOM w 0x00c0 0x7c");
  ma_write_byte(i2c_addr,2,0x00c0, 0x7c);
  //CHECK(0x00c0,1);
  //system("$SCOM w 0x00c3 0x07 0xff");
  cmd[0] = 0x07;
  cmd[1] = 0xff;
  ma_write(i2c_addr,2,0x00c3,cmd,2);
  //CHECK(0x00c3,2);

  //system("$SCOM w 0x00d9 0xd6");
  ma_write_byte(i2c_addr,2,0x00d9, 0xd6);
  //CHECK(0x00d9,1);
  //------------- not imp
  //system("$SCOM w 0x00f0 0x00 0x00");
  cmd[0] = 0x00;
  cmd[1] = 0x00;
  ma_write(i2c_addr,2,0x00f0,cmd,2);
  //CHECK(0x00f0,2);
  //system("$SCOM w 0x010f 0x0f 0x0f 0x64");
  cmd[0] = 0x0f;
  cmd[1] = 0x0f;
  cmd[2] = 0x64;
  ma_write(i2c_addr,2,0x010f,cmd,3);
  //CHECK(0x010f,3);
  //system("$SCOM w 0x0140 0x5c");
  ma_write_byte(i2c_addr,2,0x0140, 0x5c);
  //CHECK(0x0140,1);

  //system("$SCOM w 0x0152 0x00 0x77 0x00");
  cmd[0] = 0x00;
  cmd[1] = 0x77;
  cmd[2] = 0x00;
  ma_write(i2c_addr,2,0x0152,cmd,3);
  //CHECK(0x0152,3);

  //system("$SCOM w 0x025d 0x0a");
  ma_write_byte(i2c_addr,2,0x025d, 0x0a);
  //CHECK(0x025d,1);

  //system("$SCOM w 0x025f 0x4d");
  ma_write_byte(i2c_addr,2,0x025f, 0x4d);
  //CHECK(0x025f,1);

  //system("$SCOM w 0x050b 0x3f");
  ma_write_byte(i2c_addr,2,0x050b, 0x3f);
  //CHECK(0x050b,1);

  const char dsp0[16] = {0x10 ,0x00 ,0x40 ,0x00 ,0x13 ,0xb6 ,0x40 ,0x00 ,0x13 ,0x47 ,0x41 ,0x00 ,0x15 ,0x06 ,0x00 ,0x00};
  const char dsp1[16] = {0x15 ,0x17 ,0x00 ,0x00 ,0x15 ,0x47 ,0x40 ,0x00 ,0x13 ,0x56 ,0x41 ,0x00 ,0x13 ,0x67 ,0x41 ,0x00};
  const char dsp2[16] = {0x15 ,0x26 ,0x00 ,0x00 ,0x15 ,0x37 ,0x00 ,0x00 ,0x13 ,0x76 ,0x41 ,0x00 ,0x13 ,0x87 ,0x41 ,0x00};
  const char dsp3[16] = {0x15 ,0x46 ,0x00 ,0x00 ,0x15 ,0x57 ,0x00 ,0x00 ,0x10 ,0x30 ,0x40 ,0x00};
  ma_write(i2c_addr,2,0x1000,dsp0,16);
  ma_write(i2c_addr,2,0x1010,dsp1,16);
  ma_write(i2c_addr,2,0x1020,dsp2,16);
  ma_write(i2c_addr,2,0x1030,dsp3,12);
  //CHECK(0x1000,16);
  //CHECK(0x1010,16);
  //CHECK(0x1020,16);
  //CHECK(0x1030,16);

  //system("$SCOM w 0x1000 0x10 0x00 0x40 0x00 0x13 0xb6 0x40 0x00 0x13 0x47 0x41 0x00 0x15 0x06 0x00 0x00");
  //system("$SCOM w 0x1010 0x15 0x17 0x00 0x00 0x15 0x47 0x40 0x00 0x13 0x56 0x41 0x00 0x13 0x67 0x41 0x00");
  //system("$SCOM w 0x1020 0x15 0x26 0x00 0x00 0x15 0x37 0x00 0x00 0x13 0x76 0x41 0x00 0x13 0x87 0x41 0x00");
  //system("$SCOM w 0x1030 0x15 0x46 0x00 0x00 0x15 0x57 0x00 0x00 0x10 0x30 0x40 0x00");
  printf("Audio setup done\n");

  ma_write_byte(0x20,2,0x608,0x33);
  ma_write_byte(0x20,2,0x609,0);
  // ma_write_byte(0x20,2,513,0x11);
  //ma_write_byte(i2c_addr,2,MA_core__test__d1_mux_sel__a, 0x03);

  gpio_set_level(MA_NMUTE_IO, 0);

}

//var sys_err1_str =  ['X','X','DSP3','DSP2','DSP1','DSP0','ERR','PVT_low'];
//var sys_err0_str =  ['TW','AUD','CLK','PV_ov','PV_low','PV_uv','OTE','OTW'];
//const char * syserr1_str = { "X", "X", "DSP3","DSP2","DSP1","DSP0","ERR","PVT_low" } ;
//const char * syserr0_str = { "TW","AUD","CLK","PV_ov","PV_low","PV_uv","OTE","OTW" } ;
void ma120_read_error(uint8_t i2c_addr)
{ //0x0118 error now ch0 [clip_stuck  dc  vcf_err  ocp_severe  ocp]
  //0x0119 error now ch1 [clip_stuck  dc  vcf_err  ocp_severe  ocp]
  //0x011a error now system [ AE CE ... ]
  //0x011b error now system [DSP3 DSP2 DSP1 DSP0 OC OE]
  //0x011c error acc ch0 [clip_stuck  dc  vcf_err  ocp_severe  ocp]
  //0x011d error acc ch1 [clip_stuck  dc  vcf_err  ocp_severe  ocp]
  //0x011e error acc system [7..0]
  //0x011f error acc system [13..8]
  uint8_t rxbuf[10] = {0};
  char * l1;
  l1 = malloc(40);
  uint8_t res = 0xff ; // ma_read(i2c_addr,2,MA_core__prot_sys__errVect_now__errVector_ch0__a,rxbuf,8);
  //for (int i = 0; i<=7;i++)
  //{
  //  printf("%d %s",i,((rxbuf[2] & (1<<i))==(1<<i)) ? sysstr0_str[i] : "   ");
  //}

  //printf("0x011b : 0x%02x %s", rxbuf[2], l1 );
  printf("\nError vectors :");
  for (int i = 0; i<8; i++)
  { printf("%02x ", rxbuf[i]);
  }
  printf("\n");

}

void i2c_master_init()
{  int i2c_master_port = I2C_MASTER_NUM;
   i2c_config_t conf;
   conf.mode = I2C_MODE_MASTER;
   conf.sda_io_num = I2C_MASTER_SDA_IO;
   conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
   conf.scl_io_num = I2C_MASTER_SCL_IO;
   conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
   conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
   esp_err_t res = i2c_param_config(i2c_master_port, &conf);
   printf("Driver param setup : %d\n",res);
   res = i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
   printf("Driver installed   : %d\n",res);
}

esp_err_t ma_write( uint8_t i2c_addr,uint8_t prot, uint16_t address, uint8_t *wbuf, uint8_t n)
{
  bool ack = ACK_VAL;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, i2c_addr<<1 | WRITE_BIT, ACK_CHECK_EN);
  if (prot == 2) {
    i2c_master_write_byte(cmd, (uint8_t)((address&0xff00)>>8), ACK_VAL);
    i2c_master_write_byte(cmd, (uint8_t)(address&0x00ff), ACK_VAL);
  } else
  {
    i2c_master_write_byte(cmd, (uint8_t) address, ACK_VAL);
  }

  for (int i=0 ; i<n ; i++)
  { if (i==n-1) ack = NACK_VAL;
    i2c_master_write_byte(cmd, wbuf[i], ack);
  }
  i2c_master_stop(cmd);
  int ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_FAIL) { return ret; }
  return ESP_OK;
}

esp_err_t ma_write_byte(uint8_t i2c_addr,uint8_t prot, uint16_t address, uint8_t value)
{ printf("%04x %02x\n",address,value);
  esp_err_t ret=0;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (i2c_addr<<1) | WRITE_BIT , ACK_CHECK_EN);
  if (prot == 2) {
    i2c_master_write_byte(cmd, (uint8_t)((address&0xff00)>>8), ACK_VAL);
    i2c_master_write_byte(cmd, (uint8_t)(address&0x00ff), ACK_VAL);
  } else
  {
    i2c_master_write_byte(cmd, (uint8_t) address, ACK_VAL);
  }
  i2c_master_write_byte(cmd, value, ACK_VAL);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_FAIL) {
     printf("ESP_I2C_WRITE ERROR : %d\n",ret);
	 return ret;
  }
  return ESP_OK;
}

esp_err_t ma_read(uint8_t i2c_addr, uint8_t prot, uint16_t address, uint8_t *rbuf, uint8_t n)
{ esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  if (cmd == NULL ) { printf("ERROR handle null\n"); }
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (i2c_addr<<1) | WRITE_BIT, ACK_CHECK_EN);
  if (prot == 2) {
    i2c_master_write_byte(cmd, (uint8_t)((address&0xff00)>>8), ACK_VAL);
    i2c_master_write_byte(cmd, (uint8_t)(address&0x00ff), ACK_VAL);
  } else
  {
    i2c_master_write_byte(cmd, (uint8_t) address, ACK_VAL);
  }
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (i2c_addr<<1) | READ_BIT, ACK_CHECK_EN);

  i2c_master_read(cmd, rbuf, n-1 ,ACK_VAL);
 // for (uint8_t i = 0;i<n;i++)
 // { i2c_master_read_byte(cmd, rbuf++, ACK_VAL); }
  i2c_master_read_byte(cmd, rbuf + n-1 , NACK_VAL);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 100 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_FAIL) {
      printf("i2c Error read - readback\n");
	  return ESP_FAIL;
  }
  return ret;
}

uint8_t ma_read_byte(uint8_t i2c_addr,uint8_t prot, uint16_t address)
{
  uint8_t value = 0;
  esp_err_t ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);								// Send i2c start on bus
  i2c_master_write_byte(cmd, (i2c_addr<<1) | WRITE_BIT, ACK_CHECK_EN );
  if (prot == 2) {
    i2c_master_write_byte(cmd, (uint8_t)((address&0xff00)>>8), ACK_VAL);
    i2c_master_write_byte(cmd, (uint8_t)(address&0x00ff), ACK_VAL);
  } else
  {
    i2c_master_write_byte(cmd, (uint8_t) address, ACK_VAL);
  }
  i2c_master_start(cmd);							    // Repeated start
  i2c_master_write_byte(cmd, (i2c_addr<<1) | READ_BIT, ACK_CHECK_EN);
  i2c_master_read_byte(cmd, &value, NACK_VAL);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_FAIL) {
      printf("i2c Error read - readback\n");
	  return ESP_FAIL;
  }

  return value;
}
