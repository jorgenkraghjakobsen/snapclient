# Snapcast client for ESP32 

### Synchronous Multiroom audio streaming client for [Snapcast](https://github.com/badaix/snapcast) ported to ESP32

## Feature list 
- Opus and PCM decoding currently supported
- Wifi setup from menuconfig 
- Auto connect to snapcast server on network  
- Buffers up to 150 ms on Wroom modules 
- Buffers more then enough on Wrover modules 
- Multiroom sync delay controlled from Snapcast server 400ms - 2000ms

## Description 
I have continued the work from @badaix and @bridadan towards a ESP32 Snapcast client. Currently it support basic features like multirum sync, network controlled volume and mute. For now it only support Opus and PCM 16bit/48Khz audio streams and the synchornization part is still being worked on. 

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
 - esp-dsp          : Submodule to the ESP-ADF done by David Douard
 - dsp_processor    : Audio Processor and I2S low level interface including sync buffer

The snapclient functionanlity are implented in a task inclued in main - but will be refactored to a component in near future. 

Sync concept has been changed start 2021 on this implementaion and differ a bit from the way orginal snap clints handle this.

The snapclient frontend handles communiction with the server and after succefuly hello hand shake it dispatches packages from the server. 
 - CODEC_HEADER     : Setup client audio codec (FLAC, OPUS, OGG or PCM) bitrate, n channels and bits pr sample
 - WIRE_CHUNK       : Coded audio data  
 - SERVER_SETTING   : Channel volume, mute state, playback delay etc 
 - TIME             : Ping pong time keeping packages to keep track of time dif from server to client  

Each wire_chunk of audio data comes with a timestamp and client has agreed play that sample playback-delay after the timestamp. 
One way to handle that is to pass on audio data to a buffer with a length that compensate for for playback-delay, network jitter and DAC to speaker.

In this implementation I have seperate the sync task to a backend on the other end of a large ring buffer. 
Now the front end just need to pass on the audio data to the ring buffer with the server timestamp and chunk size. 
The backen read timestamps and waits until the audio chunk has the correct playback-delay to be written to the DAC amplifer speaker pipeline.
When the backend pipeline is in sync, any offset get rolled in by micro tuning the APLL on the ESP. No sample manipulation needed. 
     

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

Update third party code (opus and esp-dsp): 

    git submodule update --init 

Configure to match your setup  
  - Wifi network name and password
  - Audio coded setup

    idf.py menuconfig 

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
  