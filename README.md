# Snapcast client for ESP32 

### Synchronous Multiroom audio streaming client for [Snapcast](https://github.com/badaix/snapcast) ported to ESP32

## Feature list 
- Opus decoding currently supported
- Wifi connection hardcoded in app   
- Snapcast server address hardcoded  
- Buffers up to 150 ms on Wroom modules 
- Buffers more then enough on Wrover modules 
  

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
- [ ] Fix to alinge with above 
 * kconfig
 * add codec description 
- [ ] Integrate ESP wifi provision 
- [ok] Find and connect to Avahi broadcasted Snapcast server name
- [ ] Add a client command interface layer like volume/mute control 
- [ ] Build a ESP-ADF branch  

## Minor task 
- Propergate mute/unute from server message to DSP backend mute control. 
  - soft mute - play sample in buffer with decresing volume 
  - hard mute - pass on zero at the DSP hackend    
- Startup: do not start parsing on samples to codec before sample ring buffer hits requested buffer size. 
