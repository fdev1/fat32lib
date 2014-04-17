/*
 * smlib - Portable File System/Volume Manager For Embedded Devices
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

#ifndef STOREMANLIB_FILESYSTEM_H
#define STOREMANLIB_FILESYSTEM_H
#include "../compiler/compiler.h"

/*! \file filesystem.h
 * \brief This header file defines the interface used
 * by this module to interface to a file system driver. It also
 * defines most of the result codes returned by the functions
 * in this module.
 */


#define FILESYSTEM_SUCCESS			0x0
#define FILESYSTEM_UNKNOWN_ERROR						( 0x20 + 0x1 )
#define FILESYSTEM_CANNOT_READ_MEDIA					( 0x20 + 0x2 )
#define FILESYSTEM_CANNOT_WRITE_MEDIA					( 0x20 + 0x3 )
#define FILESYSTEM_NOT_A_DIRECTORY						( 0x20 + 0x4 )
#define FILESYSTEM_INVALID_FILENAME						( 0x20 + 0x5 )
#define FILESYSTEM_FILENAME_ALREADY_EXISTS				( 0x20 + 0x6 )
#define FILESYSTEM_INVALID_PATH							( 0x20 + 0x7 )
#define FILESYSTEM_CORRUPTED_FILE						( 0x20 + 0x8 )
#define FILESYSTEM_ILLEGAL_FILENAME						( 0x20 + 0x9 )
#define FILESYSTEM_FILENAME_TOO_LONG					( 0x20 + 0xA )
#define FILESYSTEM_NOT_A_FILE							( 0x20 + 0xB )
#define FILESYSTEM_FILE_NOT_FOUND						( 0x20 + 0xC )
#define FILESYSTEM_DIRECTORY_DOES_NOT_EXIST				( 0x20 + 0xD )
#define FILESYSTEM_INSUFFICIENT_DISK_SPACE				( 0x20 + 0xE )
#define FILESYSTEM_FEATURE_NOT_SUPPORTED				( 0x20 + 0xF )
#define FILESYSTEM_OP_IN_PROGRESS						( 0x20 + 0x10 )
#define FILESYSTEM_SECTOR_SIZE_NOT_SUPPORTED			( 0x20 + 0x11 )
#define FILESYSTEM_LFN_GENERATED						( 0x20 + 0x12 )
#define FILESYSTEM_SHORT_LFN_GENERATED					( 0x20 + 0x13 )
#define FILESYSTEM_SEEK_FAILED							( 0x20 + 0x14 )
#define FILESYSTEM_FILE_NOT_OPENED_FOR_WRITE_ACCESS		( 0x20 + 0x15 )
#define FILESYSTEM_INVALID_HANDLE						( 0x20 + 0x16 )
#define FILESYSTEM_INVALID_CLUSTER						( 0x20 + 0x17 )
#define FILESYSTEM_INVALID_FAT_VOLUME					( 0x20 + 0x18 )
#define FILESYSTEM_INVALID_VOLUME_LABEL					( 0x20 + 0x19 )
#define FILESYSTEM_INVALID_FORMAT_PARAMETERS			( 0x20 + 0x1A )
#define FILESYSTEM_ROOT_DIRECTORY_LIMIT_EXCEEDED		( 0x20 + 0x1B )
#define FILESYSTEM_DIRECTORY_LIMIT_EXCEEDED				( 0x20 + 0x1C )
#define FILESYSTEM_INVALID_PARAMETERS					( 0x20 + 0x1D )
#define FILESYSTEM_FILE_HANDLE_IN_USE					( 0x20 + 0x1E )
#define FILESYSTEM_FILE_BUFFER_NOT_SET					( 0x20 + 0x1F )
#define FILESYSTEM_AWAITING_DATA						( 0x20 + 0x21 )


/*
	@manonly */

typedef void* FILESYSTEM_FILE;
typedef void (*FILESYSTEM_ASYNC_CALLBACK)(void* context, uint16_t* result);
typedef void (*FILESYSTEM_STREAM_CALLBACK)(void* context, uint16_t* result, unsigned char** buffer, uint16_t* response);

typedef uint16_t (*FILESYSTEM_MOUNT_VOLUME)(void* volume, void* storage_interface);
typedef uint16_t (*FILESYSTEM_DISMOUNT_VOLUME)(void* volume);
typedef uint16_t (*FILESYSTEM_FIND_FIRST_ENTRY)(void* volume, char* path, unsigned char attributes, void** dir_entry, void* query);
typedef uint16_t (*FILESYSTEM_FIND_NEXT_ENTRY)(void* volume, void** dir_entry, void* query);
typedef uint16_t (*FILESYSTEM_CREATE_DIRECTORY)(void* volume, char* pathname);
typedef uint16_t (*FILESYSTEM_FILE_DELETE)(void* volume, char* filename);
typedef uint16_t (*FILESYSTEM_FILE_RENAME)(void* volume, char* original_filename, char* new_filename);
typedef uint16_t (*FILESYSTEM_FILE_OPEN)(void* volume, char* filename, unsigned char access_flags, void* file);
typedef uint16_t (*FILESYSTEM_FILE_ALLOC)(void* file, uint32_t bytes);
typedef uint16_t (*FILESYSTEM_FILE_SEEK)(void* file, uint32_t offset, char mode);
typedef uint16_t (*FILESYSTEM_FILE_WRITE)(void* file, unsigned char* buffer, uint32_t length);
typedef uint16_t (*FILESYSTEM_FILE_WRITE_ASYNC)(void* file, unsigned char* buffer, uint32_t length, uint16_t* result, FILESYSTEM_ASYNC_CALLBACK callback, void* callback_context);
typedef uint16_t (*FILESYSTEM_FILE_WRITE_STREAM)(void* file, unsigned char* buffer, uint32_t length, uint16_t* result, FILESYSTEM_STREAM_CALLBACK callback, void* callback_context);
typedef uint16_t (*FILESYSTEM_FILE_READ)(void* file, unsigned char* buffer, uint32_t length, uint32_t* bytes_read);
typedef uint16_t (*FILESYSTEM_FILE_READ_ASYNC)(void* file, unsigned char* buffer, uint32_t length, uint32_t* bytes_read, uint16_t* result, FILESYSTEM_ASYNC_CALLBACK callback, void* callback_context);
typedef uint16_t (*FILESYSTEM_FILE_FLUSH)(void* file);
typedef uint16_t (*FILESYSTEM_FILE_CLOSE)(void* file);
typedef uint16_t (*FILESYSTEM_GET_SECTOR_SIZE)(void* volume);
typedef uint16_t (*FILESYSTEM_FILE_SET_BUFFER)(void* file, unsigned char* buffer);
typedef uint16_t (*FILESYSTEM_GET_FILE_ENTRY)(void* volume, char* filename, void* file_entry);
typedef uint32_t (*FILESYSTEM_FILE_GET_UNIQUE_ID)(void* file);

/*!
 * <summary>
 * Stores all the function pointers needed by this library
 * to interface to a file system driver.
 * </summary>
 */
typedef struct FILESYSTEM
{
	char* name;
	char path_separator;
	char file_buffer_allocation_required;
	unsigned int volume_handle_size;
	unsigned int file_handle_size;
	unsigned int query_handle_size;
	unsigned int dir_entry_size;
	unsigned int dir_entry_name_offset;
	unsigned int dir_entry_name_size;
	unsigned int dir_entry_size_offset;
	unsigned int dir_entry_create_time_offset;
	unsigned int dir_entry_modify_time_offset;
	unsigned int dir_entry_access_time_offset;
	unsigned int dir_entry_attributes_offset;

	FILESYSTEM_MOUNT_VOLUME mount_volume;
	FILESYSTEM_DISMOUNT_VOLUME dismount_volume;
	FILESYSTEM_FIND_FIRST_ENTRY find_first_entry;
	FILESYSTEM_FIND_NEXT_ENTRY find_next_entry;
	FILESYSTEM_CREATE_DIRECTORY create_directory;
	FILESYSTEM_FILE_DELETE file_delete;
	FILESYSTEM_FILE_RENAME file_rename;
	FILESYSTEM_FILE_OPEN file_open;
	FILESYSTEM_FILE_ALLOC file_alloc;
	FILESYSTEM_FILE_SEEK file_seek;
	FILESYSTEM_FILE_WRITE file_write;
	FILESYSTEM_FILE_WRITE_ASYNC file_write_async;
	FILESYSTEM_FILE_READ file_read;
	FILESYSTEM_FILE_READ_ASYNC file_read_async;
	FILESYSTEM_FILE_FLUSH file_flush;
	FILESYSTEM_FILE_CLOSE file_close;
	FILESYSTEM_FILE_SET_BUFFER file_set_buffer;
	FILESYSTEM_GET_SECTOR_SIZE get_sector_size;
	FILESYSTEM_GET_FILE_ENTRY get_file_entry;
	FILESYSTEM_FILE_GET_UNIQUE_ID file_get_unique_id;
	FILESYSTEM_FILE_WRITE_STREAM file_write_stream;
}
FILESYSTEM;

/*
	@endmanonly */

#endif
