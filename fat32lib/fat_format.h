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
 
 #include <compiler.h>

#ifndef FAT32LIB_FAT_UTILITIES
#define FAT32LIB_FAT_UTILITIES

/*
/// <summary>
/// Formats a FAT volume. The sector size of the drive must be 512 bytes.
/// </summary>
///
/// <param name="fs_type">
/// A constant identifying the file system type.
/// Possible values:
/// FAT_FS_TYPE_FAT12
/// FAT_FS_TYPE_FAT16
/// FAT_FS_TYPE_FAT32
/// </param>
/// <param name="volume_label">The volume label for the drive. Must be 12 characters or less.</param>
/// <param name="no_of_sectors_per_cluster">The number of sectors per cluster. Must be a power of 2 between 2 and 64.</param>
/// <param name="device">A pointer to the storage device driver STORAGE_DEVICE structure.</param>
/// <returns>One of the return codes defined in fat.h.</returns>
*/
uint16_t fat_format_volume
(
	unsigned char fs_type,
	char* const volume_label,
	uint32_t no_of_sectors_per_cluster,
	STORAGE_DEVICE* device
);

#endif
