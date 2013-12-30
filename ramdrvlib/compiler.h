/*
 * fat32lib - Portable FAT12/16/32 Filesystem Library
 * Copyright (C) 2013 Fernando Rodriguez
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
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
typedef unsigned long long uint64_t;
typedef unsigned int uintptr_t;
typedef int int16_t;
typedef long int32_t;
typedef long long int64_t;

#endif

#if defined(_MSC_VER)

#define INLINE					__inline
#define ALIGN16
#define PACKED
#define BEGIN_PACKED_STRUCT		__pragma(pack(1))
#define END_PACKED_STRUCT		__pragma(pack())

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

#endif

#endif
