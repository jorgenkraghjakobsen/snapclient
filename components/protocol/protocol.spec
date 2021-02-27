

/* Web socket object for Merus Audio EVK interface platform
   Handles communication between Web browser, interface board and amplifer	*/
 

Interface Protocol 

Protocol : <function> <length> [<subfunction>,<parameter>,...]

<function> :=  IF Function (0),            // Get IF type and version (RPI|ESP32)
                                           // Scan I2C bus (return list of connected 
									       // devices with hardware IDs, protocol)
               Device communication (1),   // I2C device communication read/write - byte/block
               GPIO (2),                   // Get/set 16 gpio's
               Audio generator (3),        // Control local audio generator, input selector/routing
               Audio processor (4),        // Control signal processor
               Production test (5)         // Control test system   

For each function a set of subfunction are defined : 

//   <subfunction> :=
//   IF functions (0),
//	      0 : Return IF type and version <RPIZ, RPI2, RPI3, ESP32, ESP32W>, <x.z>
//		  1 : Scan I2C bus Address 0x20, 0x21, 0x22 and 0x23 and turn connected devices HWID + <gen1|gen2>
//		  2 : Config Station mode: Access point, password
//		  3 : Boot to Station Mode
//		  4 : Boot to AP mode
//	   Device communication (1),<subfunction><i2c_addr><prot><addr high><addr low>[<value>|<numberOfByteRead>]   
//	    	  0 : byte write,         
//		      1 : byte read,
//		      2 : block write, data
//		      3 : block read, number of bytes
//           16 : Return modulation, powermode and system monitors
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
//     Production test (5),
//        0 : All off  
//        1 : start test retrun pid or 0           
//        2 : set test parameters <strlen 2 bytes> 

