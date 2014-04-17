/*
 * smlib - Portable File System/Volume Manager For Embedded Devices
 * Copyright (C) 2013 Fernando Rodriguez
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Versioni 3 as 
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

#ifndef COMPILER_H
#define COMPILER_H

/*
// use a version of the critical section macros that is useful
// for debugging the critical sections on a single threaded
// system (and it's useless in a real multi-threaded environment)
*/
#define DEBUG_CRITICAL_SECTIONS

/*
// uncomment this define to compile for a big endian cpu
*/
/* #define BIG_ENDIAN */

/*
// define fixed width integers
*/
#if defined(_MSC_VER)
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
#elif defined(__C30__)
typedef unsigned int uint16_t;
typedef unsigned long uint32_t;
typedef unsigned int uintptr_t;
typedef int int16_t;
typedef long int32_t;
#define NO_INT64
#else
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned short int16_t;
typedef unsigned long int32_t;
typedef unsigned int uintptr_t;
#define NO_INT64
#endif

/*
// if your compiler supports inline, align, and struct packing attributes
// define them here. The PACKED macro is used for struct members, BEGIN_PACKED_STRUCT and
// END_PACKED_STRUCT are used before and after the struct definition, you can use
// whichever your compiler supports. Alternatively you can define USE_PRAGMA_PACK
// or NO_STRUCT_PACKING but the latter is a little less efficient except on big endian
// systems where struct packing is not used at all
*/
#if defined(_MSC_VER)
#define INLINE					__inline
#define ALIGN16					
#define PACKED					
#define BEGIN_PACKED_STRUCT		/*__pragma(pack(1))*/
#define END_PACKED_STRUCT		/*__pragma(pack())*/
#define USE_PRAGMA_PACK
#elif defined(__C30__)
#define INLINE					inline
#define ALIGN16					__attribute__ ((aligned(2)))
#define PACKED					__attribute__ ((__packed__))
#define BEGIN_PACKED_STRUCT		
#define END_PACKED_STRUCT
#else
#define INLINE
#define ALIGN16
#define PACKED
#define BEGIN_PACKED_STRUCT
#define END_PACKED_STRUCT
/* #define USE_PRAGMA_PACK */
#define NO_STRUCT_PACKING
#endif

/*
// if compiling on a multi-threaded environment you must provide
// the implementation for critical sections as well as a method for
// relinquishing control of the thread using the following macros
*/
#if defined(DEBUG_CRITICAL_SECTIONS)
#define RELINQUISH_THREAD()
#define DECLARE_CRITICAL_SECTION(section_name)		extern volatile int section_name
#define DEFINE_CRITICAL_SECTION(section_name)		volatile int section_name
#define INITIALIZE_CRITICAL_SECTION(section_name)	section_name = 0
#define DELETE_CRITICAL_SECTION(section_name)
#define LEAVE_CRITICAL_SECTION(section_name)		\
{													\
	while (!section_name)							\
	{												\
		RELINQUISH_THREAD();						\
	}												\
	section_name = 0;								\
}

#define ENTER_CRITICAL_SECTION(section_name)		\
{													\
	while (1)										\
	{												\
		if (section_name == 0)						\
		{											\
			section_name = 1;						\
			break;									\
		}											\
		RELINQUISH_THREAD();						\
	}												\
}
#elif defined(_MSC_VER)
#include <windows.h>
#include <winbase.h>
#define RELINQUISH_THREAD()							Sleep(0)
#define DECLARE_CRITICAL_SECTION(section_name)		extern volatile CRITICAL_SECTION section_name
#define DEFINE_CRITICAL_SECTION(section_name)		volatile CRITICAL_SECTION section_name
#define INITIALIZE_CRITICAL_SECTION(section_name)	InitializeCriticalSection(&section_name)
#define DELETE_CRITICAL_SECTION(section_name)		DeleteCriticalSection(&section_name)
#define LEAVE_CRITICAL_SECTION(section_name)		LeaveCriticalSection(&section_name)
#define ENTER_CRITICAL_SECTION(section_name)		\
{													\
	while (1)										\
	{												\
		__try										\
		{											\
			EnterCriticalSection(&section_name);	\
			break;									\
		}											\
		__except (EXCEPTION_EXECUTE_HANDLER)		\
		{											\
			continue;								\
		}											\
	}												\
}
#elif defined(__C30__)
#define RELINQUISH_THREAD()							asm("nop")
#define DECLARE_CRITICAL_SECTION(section_name)		extern volatile int section_name
#define DEFINE_CRITICAL_SECTION(section_name)		volatile int section_name
#define DELETE_CRITICAL_SECTION(section_name)		section_name = 0
#define LEAVE_CRITICAL_SECTION(section_name)		section_name = 0
#define INITIALIZE_CRITICAL_SECTION(section_name)	section_name = 0
#define ENTER_CRITICAL_SECTION(section_name)		\
{													\
	char saved_ipl;									\
	while (1)										\
	{												\
		SET_AND_SAVE_CPU_IPL(saved_ipl, 7);			\
		if (section_name == 0)						\
		{											\
			section_name = 1;						\
			RESTORE_CPU_IPL(saved_ipl);				\
			break;									\
		}											\
		RESTORE_CPU_IPL(saved_ipl);					\
	}												\
} 
#else
#define DECLARE_CRITICAL_SECTION(section_name)		#error DECLARE_CRITICAL_SECTION not implemented.
#define DEFINE_CRITICAL_SECTION(section_name)		#error DEFINE_CRITICAL_SECTION not implemented.
#define INITIALIZE_CRITICAL_SECTION(section_name)	#error INITIALIZE_CRITICAL_SECTION not implemented.
#define DELETE_CRITICAL_SECTION(section_name)		#error DELETE_CRITICAL_SECTION not implemented.
#define ENTER_CRITICAL_SECTION(section_name)		#error ENTER_CRITICAL_SECTION not implemented.
#define LEAVE_CRITICAL_SECTION(section_name)		#error LEAVE_CRITICAL_SECTION not implemented.
#define RELINQUISH_THREAD()
#endif

/*
// byte order macros
*/
#if defined(BIG_ENDIAN)
#define INT16_BYTE0		1
#define INT16_BYTE1		0
#define INT32_BYTE0		3
#define INT32_BYTE1		2
#define INT32_BYTE2		1
#define INT32_BYTE3		0
#define INT32_WORD0		1
#define INT32_WORD1		0
#else
#define INT16_BYTE0		0
#define INT16_BYTE1		1
#define INT32_BYTE0		0
#define INT32_BYTE1		1
#define INT32_BYTE2		2
#define INT32_BYTE3		3
#define INT32_WORD0		0
#define INT32_WORD1		1
#endif

/*
// error checking
*/
#if defined(NDEBUG) && defined(DEBUG_CRITICAL_SECTIONS)
#line 29
#error DEBUG_CRITICAL_SECTIONS is defined on a release build!
#endif

#endif
