fat32lib - Portable FAT12/16/32 Filesystem Library
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
 
This is the FAT filesystem driver library.

Please see fat.h for configuration details.

You may need to modify compiler.h in the compiler folder in order to get 
this library to compile correctly.

See the included documentation on doc folder and the examples included
for usage info.

Known bugs:
===========

1. The XC16 compiler handle structure alignment different than
C30 so unless you compiler as strict ANSI code it will give compile
warnings and possible runtime errors. Until I get the time to fix
this the workaround is to compile with the strict option, that way
the library does not rely on struct alignment. Performance on strict
ANSI mode is not as good but it's still OK.

2. When performing unbuffered IO if the output buffer is not word
aligned or if it's size is not divisible by two it will throw an
address error. That is actually a driver bug which I will get fixed
in the next release.

How to compile:
===============

The following command can be used to compile using GNU Make:

Windows:
make CC=<path to xc16-gcc.exe> AR=<path to xc16-ar.exe>

Linux:
make RM="rm -f" CC=<path to xc16-gcc> AR=<path to xc16-ar>

