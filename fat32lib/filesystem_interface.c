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

/* #if defined(FAT_FILESYSTEM_INTERFACE) */

#include "fat.h"
#include "filesystem_interface.h"
#include "..\smlib\filesystem.h"

void fat_get_filesystem_interface(FILESYSTEM* filesystem)
{
	FAT_DIRECTORY_ENTRY tmp;
	filesystem->name = "fat";
	filesystem->path_separator = '\\';
	filesystem->file_handle_size = sizeof(FAT_FILE);
	filesystem->volume_handle_size = sizeof(FAT_VOLUME);
	filesystem->query_handle_size = sizeof(FAT_FILESYSTEM_QUERY);
	filesystem->dir_entry_size = sizeof(FAT_DIRECTORY_ENTRY);
	filesystem->dir_entry_name_offset = (uintptr_t) tmp.name - (uintptr_t) &tmp;
	filesystem->dir_entry_size_offset = (uintptr_t) &tmp.size - (uintptr_t) &tmp;
	filesystem->dir_entry_attributes_offset = (uintptr_t) &tmp.attributes - (uintptr_t) &tmp;
	filesystem->dir_entry_create_time_offset = (uintptr_t) &tmp.create_time - (uintptr_t) &tmp;
	filesystem->dir_entry_modify_time_offset = (uintptr_t) &tmp.modify_time - (uintptr_t) &tmp;
	filesystem->dir_entry_access_time_offset = (uintptr_t) &tmp.access_time - (uintptr_t) &tmp;
	#if defined(FAT_DISABLE_LONG_FILENAMES)
	filesystem->dir_entry_name_size = 13;
	#else
	filesystem->dir_entry_name_size = 256;
	#endif

	#if defined(FAT_ALLOCATE_FILE_BUFFERS)
	filesystem->file_buffer_allocation_required = 0;
	#else
	filesystem->file_buffer_allocation_required = 1;
	#endif

	filesystem->create_directory = (FILESYSTEM_CREATE_DIRECTORY) &fat_create_directory;
	filesystem->file_alloc = (FILESYSTEM_FILE_ALLOC) &fat_file_alloc;
	filesystem->file_flush = (FILESYSTEM_FILE_FLUSH) &fat_file_flush;
	filesystem->file_close = (FILESYSTEM_FILE_CLOSE) &fat_file_close;
	filesystem->file_delete = (FILESYSTEM_FILE_DELETE) &fat_file_delete;
	filesystem->file_open = (FILESYSTEM_FILE_OPEN) &fat_file_open;
	filesystem->file_read = (FILESYSTEM_FILE_READ) &fat_file_read;
	filesystem->file_read_async = (FILESYSTEM_FILE_READ_ASYNC) &fat_file_read_async;
	filesystem->file_rename = (FILESYSTEM_FILE_RENAME) &fat_file_rename;
	filesystem->file_seek = (FILESYSTEM_FILE_SEEK) &fat_file_seek;
	filesystem->file_write = (FILESYSTEM_FILE_WRITE) &fat_file_write;
	filesystem->file_write_async = (FILESYSTEM_FILE_WRITE_ASYNC) &fat_file_write_async;
	filesystem->file_set_buffer = (FILESYSTEM_FILE_SET_BUFFER) &fat_file_set_buffer;
	filesystem->mount_volume = (FILESYSTEM_MOUNT_VOLUME) &fat_mount_volume;
	filesystem->dismount_volume = (FILESYSTEM_DISMOUNT_VOLUME) &fat_dismount_volume;
	filesystem->find_first_entry = (FILESYSTEM_FIND_FIRST_ENTRY) &fat_find_first_entry;
	filesystem->find_next_entry = (FILESYSTEM_FIND_NEXT_ENTRY) &fat_find_next_entry;
	filesystem->get_sector_size = (FILESYSTEM_GET_SECTOR_SIZE) &fat_get_sector_size;
	filesystem->get_file_entry = (FILESYSTEM_GET_FILE_ENTRY) &fat_get_file_entry;
	filesystem->file_get_unique_id = (FILESYSTEM_FILE_GET_UNIQUE_ID) &fat_file_get_unique_id;
	#if defined(FAT_STREAMING_IO)
	filesystem->file_write_stream = (FILESYSTEM_FILE_WRITE_STREAM) &fat_file_write_stream;
	#endif

}

/* #endif */
