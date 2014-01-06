fat32lib_file_io_example - Example for using Fat32 lib
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

===========================================================================

This is an example for using Fat32lib to read and write file on a dspic
device. When an SD card is detected it will mount the volume and write a 
32MB file to the card. The file consists of sequential 32-bit unsigned integers.
The file test.cpp on win32test contains a function called test_check_file
that you can use to verify the integrity of this file. You can also define
VERIFY_DATA to have the file checked after writing (this will block execution
while verifying file so heartbeat LED will stop flashing).

LED/Switch Functions:
=====================

LEDs from left to right (Explorer 16 Board)

LED 0 (RA7) 	- Heartbeat
LED 1 (RA6)		- Volume mounted indicator
LED 3 (RA5)		- Disk activity indicator

Swithces:

S3 (RD6)		- Press and hold for 2 secs. to dismount volume


Configuration:
==============

At the top of main.c you will find the defines for the SD card pins, change them
as necessary. The SD card C/D pin needs a pull-up resistor.

