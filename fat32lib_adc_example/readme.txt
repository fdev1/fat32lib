fat32lib_adc_sample - Example for sampling from ADC to SD card with Fat32lib.
Copyright (C) 2013 Fernando Rodriguez (frodriguez.developer@outlook.com)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License Version 3 as 
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

=================================================================================

This is an example for sampling a signal with the ADC module on dspic devices
and storing the data on an SD card using Fat32lib. So far I've gotten it to work
at 230 Ksps on a 40 MIPS device with a 10 MBit/s SPI bus without loosing a single 
sample using pre-allocated files. This example pre-allocates 256 MBs on the SD card 
using the sm_file_alloc function which is optimized for flash devices (as long as the 
file is opened with the SM_FILE_FLAG_OPTIMIZE_FOR_FLASH flag).


LED/Switch Functions:
=====================

LEDs from left to right (Explorer 16 Board)

LED 0 (RA7) 	- Heartbeat
LED 1 (RA6)		- Volume mounted indicator
LED 3 (RA5)		- Disk activity indicator
LED 4 (RA4)		- Sampling indicator

Swithces:

S3 (RD6)		- Press and hold for 2 secs. to dismount volume
S6 (RD7)		- Press to start/stop sampling from AN5 (potentiometer)


Configuration:
==============

At the top of main.c you will find the defines for the SD card pins, change them
as necessary. The SD card C/D pin needs a pull-up resistor.



Notes on performance:
=====================

In order to get this example to work without missing any samples you'll need to
make sure that your SD card file system is properly aligned with the flash pages. You
can do this by one of the following:

1. format your SD as FAT32 with Windows 7 or Windows 8 (I'm not sure about earlier 
   versions of windows)
   
2. format your SD card using the format function including in fat32lib (fat_format)

3. format your SD card with the formatter available at sdcard.org:
   https://www.sdcard.org/downloads/formatter_4/

You'll also need to make sure that your SD card is not too fragmented. This is because
this example is right on the limit of what's possible with the resources that we have,
if you don't really need to sample at 230 MSps you should have no problems sampling to
any card at a reasonable rate.