/**
 * \file    rtprx.c
 * \author  Jorgen Kragh Jakobsen
 * \date    19.10.2019
 * \version   0.1
 *
 * \brief RTP audio stream reciever
 *
 * \warning This software is a PROTOTYPE version and is not designed or intended for use in production, especially not for safety-critical applications!
 * The user represents and warrants that it will NOT use or redistribute the Software for such purposes.
 * This prototype is for research purposes only. This software is provided "AS IS," without a warranty of any kind.
 */
#include "rtprx.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"

#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "driver/i2s.h"
#include "opus.h"

//extern uint32_t sample_rate = 48000;   xxx fixme

static bool rtpRxState = 0;

void rtp_rx_start() {
  if (rtpRxState == 0 )
  { setup_rtp_i2s();
    xTaskCreate(rtp_rx_task, "RTPRx", 12*1024, NULL, 0, NULL);
    rtpRxState = 1;
  }
}

void rtp_rx_stop() {
  if (rtpRxState == 1)
  { i2s_driver_uninstall(0);
    //vTaskDelete(xTaskSignal);    xxx Fix me
    rtpRxState = 0;
  }
}


void rtp_rx_task(void *pvParameters){
  OpusDecoder *decoder;

  //int size = opus_decoder_get_size(2);
  int oe = 0;
  decoder = opus_decoder_create(48000,2,&oe);
  //  int error = opus_decoder_init(decoder, 48000, 2);
 	printf("Initialized Decoder: %d", oe);

  //int32_t *audio32 = (int32_t*)malloc(960*sizeof(int32_t));
	int16_t *audio = (int16_t *)malloc(960*2*sizeof(int16_t));
  i2s_zero_dma_buffer(0);
  static struct netconn *conn;
  static struct netbuf *buf;
  static uint32_t pkg = 0 ;
  static uint32_t pkgerror = 0;
  err_t err;
  uint16_t oldseq = 1;
  uint16_t first = 1;
  conn = netconn_new(NETCONN_UDP);
  if (conn!=NULL)
  { printf("Net RTP RX\n");
    netconn_bind(conn, IP_ADDR_ANY, 1350);
    netconn_listen(conn);
    printf("Net RTP will enter loopn\n");
    while (1) {
      netconn_recv(conn,&buf);
      if (buf == NULL)
      { printf("NETCONN RX error \n"); }
      pkg++;

      uint8_t *p = (buf->p->payload);
      uint16_t seq =  (p[2]<<8)+p[3];
      if ( (seq!=oldseq+1) & (first != 1) )
      { printf("seq : %d, oldseq : %d \n",seq,oldseq);
        uint16_t errors = seq-oldseq-1;
        pkgerror = pkgerror + errors;
        printf("ERROR --- Package drop : %d  %d \n",errors, pkgerror);
        size_t bWritten;
        //for (int i = 0; i;i++ )
        int ret = i2s_write_expand(0, (char*)audio, 960*2*sizeof(int16_t),16,32, &bWritten, 100);
        printf("bWritten : %d  ret : %d \n ",bWritten,ret);

        //opus_pkg = NULL;
      }

      if (seq<oldseq)
      { printf("ERROR --- Packege order:"); }
      oldseq = seq;
      first = 0;
      //printf("UDP package len : %d ->  \n", buf->p->len);
      //printf("UDP package     : %02x %02x %02x %02x\n",p[0],p[1],p[2],p[3]);
      //printf("Timestamp       : %02x %02x %02x %02x\n",p[4],p[5],p[6],p[7]);
      //printf("Sync source     : %02x %02x %02x %02x\n",p[8],p[9],p[10],p[11]);
      //printf("R1              : %d \n",(p[12]&0xf8)>>3) ;
      //int size = 240;
      unsigned char *opus_pkg = buf->p->payload + 12;
      int size = opus_decode(decoder, (unsigned char *)opus_pkg, buf->p->len-12, (opus_int16*)audio, 960, 0);
      if (size < 0 )
      { printf("Decode error : %d \n",size); }

      //for (int i = 0; i < size*2; i++) {
      //  audio[i*2]   = 0x0000;
      //  audio[i*2+1] = 0x0000;
      //}

      size_t bWritten;
      int ret = i2s_write_expand(0, (char*)audio, size*2*sizeof(int16_t),16,32, &bWritten, 100);
      if (ret != 0 ) printf("Error I2S written: %d %d %d \n",ret, size*2*sizeof(int16_t) ,bWritten);

      if ((pkg%1000)==1) {
        //printf("I2S written: %d %d \n", size*sizeof(int32_t) ,bWritten);
        printf("%d > %d %d %d\n",pkg, size,  buf->p->len, buf->p->tot_len );
        printf("UDP package len : %d ->  \n", buf->p->len);
        printf("UDP package     : %02x %02x %02x %02x\n",p[0],p[1],p[2],p[3]);
        printf("Timestamp       : %02x %02x %02x %02x\n",p[4],p[5],p[6],p[7]);
        printf("Sync source     : %02x %02x %02x %02x\n",p[8],p[9],p[10],p[11]);
        printf("R1              : %d \n",(p[12]&0xf8)>>3) ;

        for (int i=0;i<8;i++)
          printf("%02d %04x %04x\n",i,audio[2*i],audio[2*i+1]);
      }

      //netbuf_free(buf);
      netbuf_delete(buf);
    }
  }
  netconn_close(conn);
  netconn_delete(conn);
}

void setup_rtp_i2s()
{
  i2s_config_t i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_TX,                                  // Only TX
    .sample_rate = 48000,
    .bits_per_sample = 32,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
    .communication_format = I2S_COMM_FORMAT_I2S,
    .dma_buf_count = 8,
    .dma_buf_len = 480,
    //.intr_alloc_flags = 1,                                                  //Default interrupt priority
    .use_apll = true,
    .fixed_mclk = 0,
    .tx_desc_auto_clear = true                                                //Auto clear tx descriptor on underflow
  };

  i2s_driver_install(0, &i2s_config, 0, NULL);
  i2s_zero_dma_buffer(0);

  i2s_pin_config_t pin_config = {
    .bck_io_num   = 12, //CONFIG_EXAMPLE_I2S_BCK_PIN,
    .ws_io_num    = 13, //CONFIG_EXAMPLE_I2S_LRCK_PIN,
    .data_out_num = 14, //CONFIG_EXAMPLE_I2S_DATA_PIN,
    .data_in_num = -1                                                       //Not used
  };

  i2s_set_pin(0, &pin_config);
  printf("Here... set pin\n");
}
