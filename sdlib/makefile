#
# Makefile
#
# Copyright 2014 Fernando Rodriguez (support@fernansoft.com). 
# All rights reserved
#

#
# Note: This makefile has only been tested with
# GNU Make v3.81 on Windows. You may need to modify
# the "toolchain" section bellow (even if you're running
# it on Windows as your tools path may be different)
#


#
# toolchain
#
HAL=../dspic_hal
CC="C:\Program Files (x86)\Microchip\xc16\v1.21\bin\xc16-gcc.exe" 
AS="C:\Program Files (x86)\Microchip\xc16\v1.21\bin\xc16-gcc.exe"
AR="C:\Program Files (x86)\Microchip\xc16\v1.21\bin\xc16-ar.exe"
RM=del /Q
OPT=-O1

#
# target flags
#
p33f: CFLAGS=-omf=coff $(OPT) -I"../compiler" -I"../dspic_hal" -x c -Wall -mlarge-code -mlarge-data -fno-schedule-insns -fno-schedule-insns2 -mno-eds-warn -mcpu=33FJ256GP710A
p33f: ASFLAGS=-omf=-coff -mcpu=33FJ256GP710A -c -Wa
p33e: CFLAGS=-omf=coff $(OPT) -I"../compiler" -I"../dspic_hal" -x c -Wall -mlarge-code -mlarge-data -fno-schedule-insns -fno-schedule-insns2 -mno-eds-warn -mcpu=33EP512GP502
p33e: ASFLAGS=-omf=coff -mcpu=33EP512GP502 -c -Wa
ARFLAGS=-omf=coff r
#
# target libraries
#
LIBRARY_P33E=sdlib_p33e.a
LIBRARY_P33F=sdlib_p33f.a

#
# sources
#
SOURCES=sd.c
OBJECTS=$(SOURCES:.c=.o)

#
# make
#
all: p33f
p33f: clean $(SOURCES) $(LIBRARY_P33F)
p33e: clean $(SOURCES) $(LIBRARY_P33E)

$(LIBRARY_P33E): $(OBJECTS)
	$(RM) $(LIBRARY_P33E)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)
	
$(LIBRARY_P33F): $(OBJECTS)
	$(RM) $(LIBRARY_P33F)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)
	
.c.o:
	$(CC) $(CFLAGS) -I$(HAL) -c $< -o $@

clean:
	$(RM) $(OBJECTS)
