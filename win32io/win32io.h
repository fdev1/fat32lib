/*
 * win32io - Win32 Direct IO Driver for fat32lib and smlib
 * Copyright (C) 2013 Fernando Rodriguez (frodriguez.developer@outlook.com)
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

#ifndef WIN32IO_H
#define WIN32IO_H

#include "..\fat32lib\storage_device.h"

//
// gets the interface for I/O access to a physical drive on this
// computer
//
// Arguments:
//	physical drive - the UTF16 path to the physical drive. ie. \\.\E:
//	device - a STORAGE_DEVICE structure pointer to initialize
//
// WARNING! - if you create an interface to a hard drive there's always the
// chance that it will be corrupted so be safe and use removable storage.
//
void win32io_get_storage_device(TCHAR* physical_drive, STORAGE_DEVICE* device);
void win32io_release_storage_device();

#endif