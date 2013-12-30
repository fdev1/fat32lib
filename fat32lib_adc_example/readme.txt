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
