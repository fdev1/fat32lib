#!/bin/sh

cd dspic_hal
echo
echo 'Building dspic_hal for P33F...'
make \
	RM="rm -f" \
	CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc \
	AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33f

echo
echo 'Building dspic_hal for P33E...'
make \
	RM="rm -f" \
	CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc \
	AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33e

cd ../fat32lib
echo
echo 'Building fat32lib for P33F...'
make \
	RM="rm -f" \
	CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc \
	AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33f

echo
echo 'Building fat32lib for P33E...'
make \
	RM="rm -f" \
	CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc \
	AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33e

cd ../sdlib
echo
echo 'Building sdlib for dsPIC33F...'
make \
	RM="rm -f" \
	CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc \
	AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33f

echo
echo 'Building sdlib for dsPIC33E...'
make \
	RM="rm -f" \
	CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc \
	AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33e

cd ../smlib
echo
echo 'Building smlib for dsPIC33F...'
make \
	RM="rm -f" \
	CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc \
	AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33f

echo
echo 'Building smlib for dsPIC33E...'
make \
	RM="rm -f" \
	CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc \
	AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33e

echo

