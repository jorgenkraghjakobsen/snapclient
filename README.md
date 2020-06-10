# Snapcast client for ESP32 

### Synchronous Multiroom audio streaming client for [Snapcast](https://github.com/badaix/snapcast) ported to ESP32

## Feature list 
- Opus decoding currently supported
- Wifi connection hardcoded in app   
- Auto connect to snapcast server on network  
- Buffers up to 150 ms on Wroom modules 
- Buffers more then enough on Wrover modules 
- Multiroom sync delay controlled from Snapcast server

## Description 
I have continued the work from @badaix and @bridadan towards a ESP32 Snapcast client. Currently it support basic features like multirum sync, network controlled volume and mute. For now it only support Opus 16bit/48Khz audio streams and the synchornization part is still being worked on. 

Please check out the task list and feel free to fill in.

I have used the Infineon MA12070P Multi level Class D combined coded/amp due to its superior power effecienty on a high supply rail. It allows battery power system with good playback time at normal listen level and stil have the power to start the party.

### Codebase

The codebase is split into components and build on vanilla ESP-IDF. I stil have some refactoring on the todo list as the concept has started to settle and allow for new features can be added in a stuctured manner. In the code you will find parts that are only partly related features and still not on the task list. 
Components 
 - MerusAudio       : Low level communication interface MA12070P 
 - opus             : Opus audio coder/decoder full submodule   
 - rtprx            : Alternative RTP audio client UDP low latency also opus based
 - lightsnapcast    : Port of @bridadan scapcast packages decode library
 - libbuffer        : Generic buffer abstraction
 - esp-dsp          : Port of ESP-DSP library - stripped version - submodule considered 
 - dsp_processor    : Audio Processor and I2S low level interface including sync buffer

### Hardware 
    -   ESP pinout                         MA12070P 
    ------------------------------------------------------
                              ->            I2S_BCK        Audio Clock 3.072 MHz    
                              ->            I2S_WS         Frame Word Select or L/R 
                              ->            GND            Ground       
                              ->            I2S_DI         Audio data 24bits LSB first 
                              ->            MCLK           Master clk connect to I2S_BCK
                              ->            I2C_SCL        I2C clock
                              ->            I2C_SDA        I2C Data
                              ->            GND            Ground
                              ->            NENABLE        Amplifier Enable active low                         
                              ->            NMUTE          Amplifier Mute active low
                              
                               
## Build 

Clone this repo: 

    git clone https://github.com/jorgenkraghjakobsen/snapclint 

Update third party code: 

    git submodule update --init

Configure to match your setup: 
  - Wifi network name and password
  - Audio coded setup

Build, compile and flash:

    idf.py build flash monitor 

## Test 
Setup a snapcast server on your network 

On a linux box: 

Clone snapcast build and start the server

    ./snapserver  

Pipe some audio to the snapcast server fifo 

    mplayer http://ice1.somafm.com/secretagent-128-aac -ao pcm:file=/tmp/snapfifo -af format=s16LE -srate 48000

Test the server config on other knowen platform 

    ./snapclient  from the snapcast repo

Android : snapclient from the app play store 


## Task list
- [ok] Fix to alinge with above 
 * kconfig
 * add codec description 
- [ ] Integrate ESP wifi provision 
- [ok] Find and connect to Avahi broadcasted Snapcast server name
- [ ] Add a client command interface layer like volume/mute control 
- [ ] Build a ESP-ADF branch  

## Minor task 
- [ ] Propergate mute/unute from server message to DSP backend mute control. 
  - [ ] soft mute - play sample in buffer with decresing volume 
  - [ok] hard mute - pass on zero at the DSP hackend    
- [ ] Startup: do not start parsing on samples to codec before sample ring buffer hits requested buffer size. 
  - [ok] Start from empty buffer
  