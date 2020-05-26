##Snapcast client for ESP32 

Synchronous Multiroom audio streaming clint for [Snapcast](https://github.com/badaix/snapcast) 

#Feature list 
  Opus decoding currently supported
  Wifi connection hardcoded in app   
  Snapcast server address hardcoded  
  Buffers up to 150 ms on Wroom modules 
  Buffers more then enough on Wrover modules 
  

#Build 
Clone this repo 
git clone https://github.com/jorgenkraghjakobsen/snapclint 
Update third party code 
git submodule update --init

Configure to match your setup 
  - Wifi network name and password
  - Audio coded setup
Build and compile 
idf.py build flash monitor 

#Test 
Setup a snapcast server on your network 
On a linux box: 
Clone snapcast build and start the server
./snapserver  
Pipe some audio to the snapcast server fifo 
mplayer http://ice1.somafm.com/secretagent-128-aac -ao pcm:file=/tmp/snapfifo -af format=s16LE -srate 48000
Test the server config on other knowen platform 
./snapclient  from the snapcast repo
Android : snapclient from the app play store 


#Task list 
- [ ] Integrate ESP wifi provision 
- [ ] Find and connect to Avahi broadcasted Snapcast server name
- [ ] Add a client command interface layer like volume/mute control 
- [ ] Build a ESP-ADF branch  
