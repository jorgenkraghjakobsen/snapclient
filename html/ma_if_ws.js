// Web socket object for Merus Audio EVK interface platform
//   Handles communication between Web browser, interface board and amplifer
//   Protocol : <function> [<parameter> ...]
//   <function> :=  IF Function (0),               // Get IF type and version (RPI|ESP32)
//                                                   // Scan I2C bus (return list of connected devices with hardware IDs, protocol)
//                  Device communication (1),      // scom read/write byte/block
//					GPIO (2),                      // Get/set 16 gpio's
//					Audio generator (3),           // Control local audio generator, input selector/routing
//					Audio processor (4),           // Control signal processor
//					TBD
//   <subfunction> :=
//   IF functions (0),
//	    0 : Return IF type and version <RPIZ, RPI2, RPI3, ESP32, ESP32W>, <x.z>
//		  1 : Scan I2C bus Address 0x20, 0x21, 0x22 and 0x23 and turn connected devices HWID + <gen1|gen2>
//		  2 : Config Station mode: Access point, password
//		  3 : Boot to Station Mode
//		  4 : Boot to AP mode
//	   Device communication (1),
//	    	  0 : byte write,
//		  1 : byte read,
//		  2 : block write, data
//		  3 : block read, number of bytes
//                16 : Return modulation, powermode and system monitors
//	   GPIO (2),
//	      0 : read bit number <0-15>
//		  1 : write bit number <0-15>
//		  2 : read bank 0, 8 bits
//		  3 : read bank 1, 8 bits
//		  4 : write bank 0, 8 bits
//		  5 : write bank 1, 8 bits
//	   Audio generator (3),
//	      0 : Mute/unmute ch0,ch1,ch2,ch3
//		  1 : set freq, channel, freq
//		  2 : set level channel, level
//		  3 : set source, channel , <sine, noise, player>
//      4 : set offset trim : <ch0|ch1, value>
//	   Audio processor (4),
//	      tbd

// Response from interface code 5 16
// 0: 5, 16                        // response code
//
// 2: 5 bytes from 167(rev1) or 183(rev2)
// 2: 48/119 : pm selected pm forceed
// 3: 48/120 : PMP selected
// 4: 48/121 : PM0/1 monitor
// 5: 48/122 : Mdet_ch0
// 6: 48/123 : Mdet_ch1
//
// 7: 8 byte from 280 prot_sys segment (192)
// 7: 192/88 : errVect_now.errVector_ch0
// 8: 192/89 : errVect_now.errVector_ch1
// 9 : 192/90 : errVect_now.errVector_all__0
// 10: 192/91 : errVect_now.errVector_all__1
// 11: 192/92 : errVect_acc.errVector_ch0
// 12: 192/93 : errVect_acc.errVector_ch1
// 13: 192/94 : errVect_acc.errVector_all__0
// 14: 192/95 : errVect_acc.errVector_all__1

// 15: 3 bytes from 6 PA segment (0)
// 15: 0/6 :  err_pin, clip_pin
// 16: 0/7 :  temp_chip
// 17: 0/8 :  pvdd_chip

// 18: 24 bytes from DSP memory
// 18: 0 : 00 00 00 DataRate :   enum { invalid, 32k, 44.1/48kHz, 88.2/96kHz,176/192kHz }
// 22: 4 : 00 00 byte1 byte0 :   Temperature Raw
// 26: 8 : 00 00 byte1 byte0 :   PVDD Raw
// 30: 12 :  00 00 byte1 byte0 :   Aux0 Raw
// 34: 16 :  00 00 byte1 byte0 :   Aux1 Raw
// 38: 20 :  00 00 byte1 byte0 :   Vin Raw

Array.prototype.sum = Array.prototype.sum || function (){
  return this.reduce(function(p,c){return p+c},0);
};

Array.prototype.avg = Array.prototype.avg || function () {
  return this.sum()/this.length;
};
var tindex = 0;
var tbuf = [30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30];
var canvasContext = null;
var ch0_max = 0,ch0_current = 0;
var greenmax = 50;
var ch1_max = 0,ch1_current = 0;
var WIDTH = 256*2;
var HEIGHT = 40;
var ch0_pm_old = 4;
var ch1_pm_old = 4;
var ch_err_str =  ['CLIP','DC','VCF','OCPS','OCP'];
var ch_err_off =  [ 0    , 2  , -3  , -4   , 3   ];
var ch0_err_old = 0xff;
var ch1_err_old = 0xff;
var ch0_acc_err_old = 0xff;
var ch1_acc_err_old = 0xff;
var sys_err0_old = 0xff;
var sys_err1_old = 0xff;
var sys_err1_str =  ['X','X','DSP3','DSP2','DSP1','DSP0','ERR','PVT_low'];
var sys_err0_str =  ['TW','AUD','CLK','PV_ov','PV_low','PV_uv','OTE','OTW'];
var sys_err1_off =  [0,-20,-40,-35,-30,-25,-20,-20];
var sys_err0_off =  [0,-10,-13,-17,-11,-1,5,0];
var mask = 0;
var ws;
console.log("Hey will load ws function");

function loadWebSocket() {
  console.log("LoadWebSocet called");
  if ("WebSocket" in window)
  { var e = document.getElementById("ws_host_select");
	var wsServer = e.options[e.selectedIndex].value;
	var rpiName = null; 
	if ( rpiName = wsServer.match(/rpi\d{1,3}/) ) {      // Seach rpi[number]; 
	  var ws_str = "ws://" + rpiName + ".local:8000";
	} else { 											   // Address is ip address
      var ws_str = "ws://" + wsServer + ":8088";   
	} 
    console.log(ws_str);
    ws = new WebSocket(ws_str);
    ws.binaryType = "arraybuffer";
    ws.onopen = function()
    {
      console.log("Connection is open");
    }
    ws.onmessage = function (evt)
    { var bufu = new Uint8Array(evt.data);
      var buf = Array.from(bufu);
      console.log(buf);
      if ((buf[0] == 5 ) & (buf[1] == 5))
      { 
		const now = Date.now();
		value =  buf[2]*256*256*256 + buf[3]*256*256 + buf[4]*256 + buf[5] ;
		console.log(value);
      
		delaySeries.addPoint(now,value);
		delayGraph.setDataSeries([delaySeries]);
		delayGraph.updateEndDate();
	
	  }

   
  	}
  }
  else
  {
    alert("WebSocket NOT supported by your Browser!");
  }
}
