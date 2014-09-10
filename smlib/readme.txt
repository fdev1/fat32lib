smlib - Portable File System/Volume Manager For Embedded Devices
Copyright (C) 2013 Fernando Rodriguez (frodriguez.developer@outlook.com)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License Versioni 3 as 
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

==========================================================================
 
This is the volume manager library.
Please read sm.h for configuration details.

You may also have to modify compiler.h in the compiler folder in
order to compile this library.

How to compile:
===============

The following command can be used to compile using GNU Make:

Windows:
make CC=<path to xc16-gcc.exe> AR=<path to xc16-ar.exe>

Linux:
make RM="rm -f" CC=<path to xc16-gcc> AR=<path to xc16-ar>
