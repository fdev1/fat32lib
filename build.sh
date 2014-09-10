#!/bin/sh

cd dspic_hal
make RM="rm -f" CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33f
make RM="rm -f" CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33e
cd ../fat32lib
make RM="rm -f" CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33f
make RM="rm -f" CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33e
cd ../sdlib
make RM="rm -f" CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33f
make RM="rm -f" CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33e
cd ../smlib
make RM="rm -f" CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33f
make RM="rm -f" CC=/opt/microchip/xc16/v1.21/bin/xc16-gcc AR=/opt/microchip/xc16/v1.21/bin/xc16-ar p33e
echo

