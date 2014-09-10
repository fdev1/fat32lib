dspic_hal - Fat32lib hardware abstraction library for dspic MCUs
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

This library is the hardware abstraction layer for for Fat32lib.
It contains abstractions (mostly macro based) for the SPI and DMA modules
as well as IO ports. As an added bonus it comes with an LCD driver for the
LCD on the Explorer 16 development board.
 

How to compile:
===============

The following command can be used to compile using GNU Make:

Windows:
make CC=<path to xc16-gcc.exe> AR=<path to xc16-ar.exe>

Linux:
make RM="rm -f" CC=<path to xc16-gcc> AR=<path to xc16-ar>
