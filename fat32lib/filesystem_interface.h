/*
 * fat32lib - Portable FAT12/16/32 Filesystem Library
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

/*! \file filesystem_interface.h
 * \brief This header contains the function that needs to be called
 * to obtain the interface needed by smlib to access this file system
 * driver.
 */

#ifndef FAT_FILESYSTEM_INTERFACE_H
#define FAT_FILESYSTEM_INTERFACE_H
/*
	@manonly */

#include "..\smlib\filesystem.h"
/*
	@endmanonly */

/*!
 * <summary>
 * Initializes a FILESYSTEM structure that will be used
 * by smlib to access the file system driver.
 * </summary>
 * <param name="filesystem">
 * A pointer to a FILESYSTEM structure.
 * </param>
 */
void fat_get_filesystem_interface(FILESYSTEM* filesystem);

#endif
