/*
 * dspic_hal - Fat32lib hardware abstraction library for dspic MCUs
 * Copyright (C) 2013 Fernando Rodriguez (frodriguez.developer@outlook.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 3 as 
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#ifndef __COMMON_H__
#define __COMMON_H__

#include <p33Fxxxx.h>
/* #include <p33FJ256GP710.h> */
/* #include <p33FJ128GP802.h> */
#include <stdlib.h>
#include <string.h>
#include <compiler.h>
 
/*
// macros to get the PC right before a trap
*/
#define __SP					(*((unsigned int*) 0x1e))
#define __PCL					(*((unsigned int*) (__SP - 14)))
#define __PCH					(*((unsigned int*) (__SP - 12)))
#define __PC() \
	(((((unsigned long) __PCH) << 16) | ((unsigned long) __PCL)) - 2)


/*
// datatype to represent a bit or
// pin pointer
*/
typedef struct _BIT_POINTER 
{
	unsigned int* Address;
	unsigned int BitMask;
} 
BIT_POINTER, PIN_POINTER;

#define BIT0						0x0001
#define BIT1						0x0002
#define BIT2						0x0004
#define BIT3						0x0008
#define BIT4						0x0010
#define BIT5						0x0020
#define BIT6						0x0040
#define BIT7						0x0080
#define BIT8						0x0100
#define BIT9						0x0200
#define BIT10						0x0400
#define BIT11						0x0800
#define BIT12						0x1000
#define BIT13						0x2000
#define BIT14						0x4000
#define BIT15						0x8000

/*
// protect the code expression from being
// interrupted
*/
#define INTERRUPT_PROTECT( exp )			\
{											\
	unsigned int __dummy__;					\
	SET_AND_SAVE_CPU_IPL( __dummy__, 7 );	\
	exp;									\
	RESTORE_CPU_IPL( __dummy__ );			\
}	

/*
// some io macros
*/
#define IO_PIN_READ( port, pin )			PORT##port##bits.R##port##pin
#define IO_PIN_WRITE( port, pin, value )	LAT##port##bits.LAT##port##pin = ( unsigned ) ( value )
#define IO_PIN_SET_AS_INPUT( port, pin )	TRIS##port##bits.TRIS##port##pin = 0x1
#define IO_PIN_SET_AS_OUTPUT( port, pin )	TRIS##port##bits.TRIS##port##pin = 0x0
#define IO_PORT_SET_AS_INPUT( port )		TRIS##port = 0xFFFF
#define IO_PORT_SET_AS_OUTPUT( port )		TRIS##port = 0x0000
#define IO_PORT_SET_DIR_MASK( port, mask )	TRIS##port = ( mask )
#define IO_PORT_GET_DIR_MASK( port )		TRIS##port
#define IO_PORT_WRITE( port, value )		LAT##port = ( value )
#define IO_PORT_READ( port )				PORT##port

/*
// macros for manipulating bit and pin pointers
*/
#define BP_SET(bp)							/*INTERRUPT_PROTECT(*/*((bp).Address) |= ((bp).BitMask) /*) */
#define BP_CLR(bp)							/*INTERRUPT_PROTECT(*/*((bp).Address) &= (~((bp).BitMask)) /*)*/
#define BP_GET(bp)							(((*((bp).Address) & (bp).BitMask) != 0) ? 1 : 0)
#define BP_INIT(bp, address, bit_no)			\
{												\
	(bp).Address = (unsigned int*)address;		\
	(bp).BitMask = 1 << bit_no;					\
}

/*
// reads a multi-bit value from a bit pointer
*/
#define BP_READ(bp, dest)								\
{														\
	unsigned int __tmp1__ = (bp).BitMask;				\
	dest = *(bp).Address & (bp).BitMask;				\
	while (__tmp1__ && !(__tmp1__ & 1))					\
	{													\
		__tmp1__ >>= 1;									\
		dest <<= 1;										\
	}													\
}
		
/*
// write to a multi-bit bit pointer
*/
#define BP_WRITE( bp, value )							\
{														\
	unsigned int __tmp1__ = value;						\
	unsigned int __tmp2__ = (bp).BitMask;				\
	while (__tmp2__ && !(__tmp2__ & 1))					\
	{													\
		__tmp1__ <<= 1;									\
		__tmp2__ >>= 1;									\
	}													\
	*(bp).Address &= (bp).BitMask ^ 0xFFFF;				\
	*(bp).Address |= (bp).BitMask & __tmp1__;			\
}

/*
// min and max macros
*/	
#define MAX( a, b )					( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )
#define MIN( a, b )					( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )

#define LOBYTE( word )				( ( unsigned char ) word )
#define HIBYTE( word )				( ( unsigned char ) ( word >> 8 ) )
#define LO16( dword )				( ( uint16_t ) dword )
#define HI16( dword )				( ( uint16_t ) ( dword >> 16 ) )
#define INDEXOF(chr, str, idx)		(int16_t) (strchr(str + idx, chr) - (int16_t)str);

#endif
