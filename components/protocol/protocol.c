// MADIF protocol handler

// Read incomming messeage from queue and do the work

// Protocol description
/*
  0 Interface functions
      Scan clients on i2c bus.
      Get host system  (ESP32, RPI, Cypress)
      Select audio source (Snapcast, RTPrx, Bluetooth, Signal generator)
      Enable DSP processing
  1 Device communication I2C
  2 GPIO manipolation
  3 Audio generator
  4 Telemetri subsystem
  5 Test function
  6 RTPrx module
  7 DSP processor configuration
*/

#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "stdint.h"
#include "protocol.h"
//#include "MerusAudio.h"
#include "rtprx.h"
#include "driver/i2s.h"
#include "dsp_processor.h"

extern enum dspFlows dspFlow;

// Protcol format
// Byte     0         1           2          3      4     ...
// Cont: Function   length   subCommand    data   <data>  ...

enum audio_sources { SNAPCAST, RTPRX, BLUETOOTH, SIGNALGENERATOR };
static const char TAG[] = "PROT";
static uint8_t audio_source ;
extern uint8_t muteCH[4];

void protocolHandlerTask(void *pvParameter)
{ audio_source = 1;
  ESP_LOGI(TAG,"Protocol handler started");
  while (1)
  { uint8_t *msg;
    xQueueReceive(prot_queue, &msg, portMAX_DELAY);
    //ESP_LOGI(TAG,"Recieved message %d length %d",*(msg),*(msg+1));
    switch (*msg) {
      case 0 :                                     // Interface functions
              switch (*(msg+2)) {
                case 0 : { ESP_LOGI(TAG,"Change Source to : %d\n",(int)*(msg+3));
                           if (audio_source != *(msg+3) )
                           { if (audio_source == SNAPCAST) {
                              i2s_zero_dma_buffer(0);
                              // Not implemnted do nothing
                             }
                             else if (audio_source == RTPRX)
                             { ESP_LOGI(TAG,"Stop RTPrx");
                               rtp_rx_stop();

                             }
                             else if (audio_source == BLUETOOTH)
                             { // Not implemnted do nothing
                             }
                             else if (audio_source == SIGNALGENERATOR)
                             {  //signalgenerator_stop();
                                ESP_LOGI(TAG,"Stop generator");
                             }
                           }
                           audio_source = *(msg+3);
                           switch (audio_source) {
                             case SNAPCAST  : break;
                             case RTPRX     : { ESP_LOGI(TAG,"Start RTPrx");
                                              rtp_rx_start();
                                              } break;
                             case BLUETOOTH : break;
                             //case SIGNALGENERATOR : { ESP_LOGI(TAG,"Start signal ");
                             //                       signalgenerator_start();
                             //                     }
                             //                  break;
                            default : break;
                           }

                         }
                         break;
                case 1 : { ESP_LOGI(TAG,"Measure system PVDD");
                         }
                         break;
                case 2 : { ESP_LOGI(TAG,"Scan i2s bus...");
                         }
                        break;
                default : break;
                }
                break;
      case 1 :                                     // Device communication  // Fuse and otp decoding
               switch (*(msg+2)) {
                 case 0 :                    //   Byte read
                    { uint8_t value  = 0; //ma_read_byte( *(msg+3), *(msg+4), (*(msg+5)<<8)+*(msg+6) ) ;
                      printf("Value read : %d\n", value);
                      // return value to sender
                      break;
                    }
                 case 1 : {                   //   Byte write
                    //ma_write_byte(*(msg+3),*(msg+4),(*(msg+5)<<8)+*(msg+6),*(msg+7) );
                    printf("i2c_write : \n");
                    break;
                 }
                 case 3 :
                    {  uint8_t l = (*(msg+1)-7);
                       uint8_t *writebuf;
                       //ESP_LOGI(TAG,"I2C write block : %d\n",*(msg+7));
                       writebuf = malloc(l);
                       for (int8_t i = 0; i<l ; i++ )
                         writebuf[i] = *(msg+7+i);
                       //ma_write(*(msg+3),*(msg+4),(*(msg+5)<<8)+*(msg+6),writebuf,l);
                       break;
                    }

                 default : break;
               }
               break;
      case 2 : //GPIO
               break;
      /*case 3 : // Signal Generator
               switch (*(msg+2)) {
                   case 0: // Stop generator
                           signalgenerator_stop();
                           ESP_LOGI(TAG, "Stop_SignalGenerator :%d",*(msg+2));
                           break;
                   case 1: // Start generator
                           signalgenerator_start();
                           ESP_LOGI(TAG, "Start_SignalGenerator :%d",*(msg+2));
                           break;
                   case 2: // Change Mode :
                           signalgenerator_mode(*(msg+3));          // cont, sweep, burst
                           ESP_LOGI(TAG, "SignalGenerator change mode :%d",*(msg+3));
                           break;
                   case 3: // Change Oscillator Type  :            //  Sine, tri, square,  noise
                           signalgenerator_osctype(*(msg+3));
                           ESP_LOGI(TAG, "SignalGenerator change oscType :%d",*(msg+3));
                           break;
                   case 4: signalgenerator_freq(*(msg+3),*(msg+4),*(msg+5));   // freq int >> 4
                           break;
                   default : break;
               }
               break;
*/
      case 4 : break;
      case 5 : break;
               
      case 6 : //RTPrx
               switch (*(msg+2)) {
                   case 0: // Stop RX reciever
                           rtp_rx_stop();
                           break;
                   case 1: // Start Rx reciever
                           rtp_rx_start();
                           break;
                   default : break;
               }
               break;
      case 7 : // DSP
                 switch (*(msg+2)) {


                   case 0: // Bypass 0 or 1 or 2 
                           dspFlow = (int) *(msg+3);
                           break;
                   case 1: // Change Xover frequency
                           //dsp_set_xoverfreq(*(msg+3),*(msg+4));
                           break;
                   case 99: { uint8_t ch = *(msg+3);
                              uint8_t state = *(msg+4);
                              printf("Mute %d %d\n",ch,state);
                              if ( (ch<=1) & (state<=1) )
                              { muteCH[ch] = state;
                              }
                            }
                            break;

                   default : break;
               }
               break;

      default : break;
    }
    free(msg);
  }
}
