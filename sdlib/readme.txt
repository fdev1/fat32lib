sdlib - SD card driver for fat32lib
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

==========================================================================

This is the SD driver.
It is pretty much portable but you will need to implement the
SPI, DMA, and IO port routines on the HAL library in order to port
it.

Please see sd.h for configuration details.
You may also have to modify compiler.h in the compiler folder to get
this library to compile correctly.
