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

#ifndef __FAT_H__
#define __FAT_H__

/*! \file fat.h
 * \brief This is the header file for the FAT driver. It allows low level
 * access to the file system. The functions defined in smlib\sm.h allow
 * access to all the functionality provided by this header, most users
 * should use those instead. The only all users need to call on this
 * module is fat_get_filesystem_interface which is defined in filesystem_interface.h 
 * and allows smlib (the volume manager) to interface with fat32lib (the
 * file system driver.)
 * <seealso cref="smlib\sm.h">smlib\sm.h</seealso>
 */

#include <compiler.h>
#include <time.h>
#include "storage_device.h"


/* #################################
// compile options
// ################################# */

/*
// uncomment this line to compile the library without long filenames support
*/
#define FAT_DISABLE_LONG_FILENAMES

/*
// if this option is not specified the library will only maintain 1 copy of
// the FAT table, otherwise it will maintain all the tables in the volume
// (usually two)
*/
/* #define FAT_MAINTAIN_TWO_FAT_TABLES */

/*
// If this option is defined the library will use the C time() function
// to obtain the date/time used for timestamps, otherwise the application needs to
// provide a function to get the time and register it with the library using the
// fat_register_system_time_function function. If no such function is registered
// the library will use 8/2/1985 @ 12:05 PM for all timestamps.
*/
#define FAT_USE_SYSTEM_TIME

/*
// Defines the maximun sector size (in bytes) that this library should
// support. An attempt to mount a volume with a sector size larger than this
// value will fail.
*/
#define MAX_SECTOR_LENGTH				0x200

/*
// Defines that fat_get_filesystem_interface should be included. This function
// initializes a FILESYSTEM structure with pointers to all the library functions
// and is used by smlib to access the filesystem. If you're not using smlib you
// can comment this out to save a few bytes.
*/
#define FAT_FILESYSTEM_INTERFACE

/*
// These definions define whether a buffer of size MAX_SECTOR_LENGTH should be allocated 
// for use by library functions. If none of these options is defined the buffers will be
// allocated from the stack as needed.
//
// FAT_ALLOCATE_VOLUME_BUFFER defines that one such buffer should be allocated for each
//	volume (as a member of the VOLUME_INFO structure). 
//
// FAT_ALLOCATE_SHARED_BUFFER defines that the buffer should be allocated globally and 
//	shared by all volumes.
//
// FAT_ALLOCATE_DYNAMIC_BUFFERS defines that the buffers should be allocated from the
//	heap as needed. This gives the best performance on multi-threaded systems when multiple
//	access are conducted concurrently on the same volume but it introduces the possibility
//	of an operation failing for lack of memory. This option also has the advantage of
//	only allocating the memory needed to store a single sector for the mounted device
//	so it makes MAX_SECTOR_LENGTH meaningless.
//
// For single-threaded systems FAT_ALLOCATE_SHARED_BUFFER is always the best choice. 
//
// For multi-threaded systems FAT_ALLOCATE_VOLUME_BUFFER will improve performance when
// accessing more that one volume at the expense of MAX_SECTOR_LENGTH bytes for each
// volume mounted and commenting all of these out or selecting FAT_ALLOCATE_DYNAMIC_BUFFERS 
// will increase performance when accessing the same volume from more than one thread 
// concurrently at the expense of MAX_SECTOR_LENGTH bytes for each concurrent access -1.
//
// On some situations not defining any of these will cause the cost of an operation to
// be MAX_SECTOR_LENGTH * 2 bytes allocated from stack.
*/
/*#define FAT_ALLOCATE_VOLUME_BUFFER*/
#define FAT_ALLOCATE_SHARED_BUFFER
/*#define FAT_ALLOCATE_DYNAMIC_BUFFERS*/	/* NOT SUPPORTED YET!!!! */
/*
// Defines that the library should be compiled with multi-thread support
*/
/*#define FAT_MULTI_THREADED */
/*
// Defines that the fat_format_volume function should be included in the library.
// If you don't need format support you can save a few lines of code by commenting
// this out.
*/
#define FAT_FORMAT_UTILITY

/*
// Defines that file buffers should be allocated with the file handle.
// This only means that the file handle structure (FAT_FILE) will include the
// buffer, it doesn't actually means that this library will do the allocation as
// the file handle should be allocated by the user. If this option is defined
// and you open the file with the FAT_FILE_FLAG_NO_BUFFERING a buffer would still
// be allocated as part of the file handle and it'll be wasted.
//
// If this option is not specified the user must call fat_file_set_buffer to
// set a buffer before calling any of the file IO functions (fat_file_read, fat_file_write,
// fat_file_flush, or fat_file_close).
//
// If this library is used through smlib and this option is not specified then
// smlib will allocate the file buffers from the heap unless SM_FILE_FLAG_NO_BUFFERING
// is specified.
*/
/* #define FAT_ALLOCATE_FILE_BUFFERS */

/*
// if this option is specified this library will be compiled only with the functions
// needed for read access.
*/
/* #define FAT_READ_ONLY */

/*
// this is the interval in sectors written at which an
// open file will be flushed 0x800 = 1 MiB with 512 bytes
// sectors
*/
#define FAT_FLUSH_INTERVAL				0x800
#define FAT_STREAMING_IO
#define FAT_OPTIMIZE_FOR_FLASH

/* #################################
// end compile options
// ################################# */

/*
// FAT file system types
*/
#define FAT_FS_TYPE_UNSPECIFIED			0x0		/* used for formatting */
#define FAT_FS_TYPE_FAT12				0x1
#define FAT_FS_TYPE_FAT16				0x2
#define FAT_FS_TYPE_FAT32				0x3

/*
// File Attributes
*/
#define FAT_ATTR_READ_ONLY				0x1
#define FAT_ATTR_HIDDEN					0x2
#define FAT_ATTR_SYSTEM					0x4
#define FAT_ATTR_VOLUME_ID				0x8
#define FAT_ATTR_DIRECTORY				0x10
#define FAT_ATTR_ARCHIVE				0x20 
#define FAT_ATTR_LONG_NAME				(FAT_ATTR_READ_ONLY |	\
											FAT_ATTR_HIDDEN |	\
											FAT_ATTR_SYSTEM |	\
											FAT_ATTR_VOLUME_ID)

/*
// file access flags
*/
#define FAT_FILE_ACCESS_READ					0x1
#define FAT_FILE_ACCESS_WRITE					0x2
#define FAT_FILE_ACCESS_APPEND					(0x4)
#define FAT_FILE_ACCESS_OVERWRITE				(0x8)
#define FAT_FILE_ACCESS_CREATE					(0x10) 
#define FAT_FILE_ACCESS_CREATE_OR_OVERWRITE		(FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_OVERWRITE)
#define FAT_FILE_ACCESS_CREATE_OR_APPEND		(FAT_FILE_ACCESS_CREATE | FAT_FILE_ACCESS_APPEND)
#define FAT_FILE_FLAG_NO_BUFFERING				(0x20)
#define FAT_FILE_FLAG_OPTIMIZE_FOR_FLASH		(0x40)

/*
// seek modes
*/
#define FAT_SEEK_START							0x1
#define FAT_SEEK_CURRENT						0x2
#define FAT_SEEK_END							0x3

/*
// streaming IO responses
*/
#define FAT_STREAMING_RESPONSE_STOP				STORAGE_MULTI_SECTOR_RESPONSE_STOP
#define FAT_STREAMING_RESPONSE_SKIP				STORAGE_MULTI_SECTOR_RESPONSE_SKIP
#define FAT_STREAMING_RESPONSE_READY			STORAGE_MULTI_SECTOR_RESPONSE_READY

/*
// return codes (first 32 codes are reserved)
*/
#define FAT_SUCCESS								( 0x0 )
#define FAT_UNKNOWN_ERROR						( 0x20 + 0x1 )
#define FAT_CANNOT_READ_MEDIA					( 0x20 + 0x2 )
#define FAT_CANNOT_WRITE_MEDIA					( 0x20 + 0x3 )
#define FAT_NOT_A_DIRECTORY						( 0x20 + 0x4 )
#define FAT_INVALID_FILENAME					( 0x20 + 0x5 )
#define FAT_FILENAME_ALREADY_EXISTS				( 0x20 + 0x6 )
#define FAT_INVALID_PATH						( 0x20 + 0x7 )
#define FAT_CORRUPTED_FILE						( 0x20 + 0x8 )
#define FAT_ILLEGAL_FILENAME					( 0x20 + 0x9 )
#define FAT_FILENAME_TOO_LONG					( 0x20 + 0xA )
#define FAT_NOT_A_FILE							( 0x20 + 0xB )
#define FAT_FILE_NOT_FOUND						( 0x20 + 0xC )
#define FAT_DIRECTORY_DOES_NOT_EXIST			( 0x20 + 0xD )
#define FAT_INSUFFICIENT_DISK_SPACE				( 0x20 + 0xE )
#define FAT_FEATURE_NOT_SUPPORTED				( 0x20 + 0xF )
#define FAT_OP_IN_PROGRESS						( 0x20 + 0x10 )
#define FAT_SECTOR_SIZE_NOT_SUPPORTED			( 0x20 + 0x11 )
#define FAT_LFN_GENERATED						( 0x20 + 0x12 )
#define FAT_SHORT_LFN_GENERATED					( 0x20 + 0x13 )
#define FAT_SEEK_FAILED							( 0x20 + 0x14 )
#define FAT_FILE_NOT_OPENED_FOR_WRITE_ACCESS	( 0x20 + 0x15 )
#define FAT_INVALID_HANDLE						( 0x20 + 0x16 )
#define FAT_INVALID_CLUSTER						( 0x20 + 0x17 )
#define FAT_INVALID_FAT_VOLUME					( 0x20 + 0x18 )
#define FAT_INVALID_VOLUME_LABEL				( 0x20 + 0x19 )
#define FAT_INVALID_FORMAT_PARAMETERS			( 0x20 + 0x1A )
#define FAT_ROOT_DIRECTORY_LIMIT_EXCEEDED		( 0x20 + 0x1B )
#define FAT_DIRECTORY_LIMIT_EXCEEDED			( 0x20 + 0x1C )
#define FAT_INVALID_PARAMETERS					( 0x20 + 0x1D )
#define FAT_FILE_HANDLE_IN_USE					( 0x20 + 0x1E )
#define FAT_FILE_BUFFER_NOT_SET					( 0x20 + 0x1F )
#define FAT_MISALIGNED_IO						( 0x20 + 0x20 )
#define FAT_AWAITING_DATA						( 0x20 + 0x21 )
#define FAT_BUFFER_TOO_BIG						( 0x20 + 0x22 )

/*
// these are the bits set on the reserved fields of a directory
// entry to indicate that a SFN entry is actually a LFN entry with
// a filename conforming to the 8.3 convention but with a lowercase
// base name or extension (or both). This library will display these
// entries correctly, but for compatibility reasons when creating
// entries an LFN entry will be created for this type of files.
*/
#define FAT_LOWERCASE_EXTENSION			0x10
#define FAT_LOWERCASE_BASENAME			0x8

/*
// misc
*/
#define FAT_MAX_PATH					260
#define FAT_FIRST_LFN_ENTRY				0x40
#if defined(FAT_DISABLE_LONG_FILENAMES)
#define FAT_MAX_FILENAME				12
#else
#define FAT_MAX_FILENAME				255
#endif

/*
// make sure that FAT_ALLOCATE_VOLUME_BUFFER and FAT_ALLOCATE_SHARED_BUFFER
// are not both defined
*/
#if defined(FAT_ALLOCATE_VOLUME_BUFFER) && defined(FAT_ALLOCATE_SHARED_BUFFER)
#line 99
#error FAT_ALLOCATE_VOLUME_BUFFER and FAT_ALLOCATE_SHARED_BUFFER cannot be defined at once.
#endif

/*!
 * <summary>Function pointer to get the system time.</summary>
*/
typedef time_t (*FAT_GET_SYSTEM_TIME)(void);

/*!
 * <summary>
 * This structure is the volume handle. All the fields in the structure are
 * reserved for internal use and should not be accessed directly by the
 * developer.
 * </summary>
*/
typedef struct FAT_VOLUME 
{
	uint32_t id;
	uint32_t fat_size;
	uint32_t root_cluster;
	uint32_t first_data_sector;
	uint32_t no_of_sectors;
	uint32_t no_of_data_sectors;
	uint32_t no_of_clusters;
	uint32_t no_of_reserved_sectors;
	uint32_t next_free_cluster;
	uint32_t total_free_clusters;
	uint32_t fsinfo_sector;
	uint16_t root_directory_sectors;
	uint16_t no_of_bytes_per_serctor;
	uint16_t no_of_sectors_per_cluster;
	char use_long_filenames;
	unsigned char fs_type;
	unsigned char no_of_fat_tables;
	char label[12];
	#if defined(FAT_ALLOCATE_VOLUME_BUFFER)
	#if defined(FAT_MULTI_THREADED)
	DEFINE_CRITICAL_SECTION(sector_buffer_lock);
	#endif
	ALIGN16 unsigned char sector_buffer[MAX_SECTOR_LENGTH];
	uint32_t sector_buffer_sector;
	#endif
	#if defined(FAT_MULTI_THREADED)
	DEFINE_CRITICAL_SECTION(write_lock);
	unsigned char readers_count;
	#endif
	STORAGE_DEVICE* device;
}	
FAT_VOLUME;

/*
// fat 32-byte directory entry structure
*/
#if defined(USE_PRAGMA_PACK)
#pragma pack(1)
#endif
BEGIN_PACKED_STRUCT
typedef struct FAT_RAW_DIRECTORY_ENTRY 
{
	union 
	{
		struct STD
		{
			PACKED unsigned char name[11];
			PACKED unsigned char attributes;
			PACKED unsigned char reserved;
			PACKED unsigned char create_time_tenth;
			PACKED uint16_t create_time;
			PACKED uint16_t create_date;
			PACKED uint16_t access_date;
			PACKED uint16_t first_cluster_hi;
			PACKED uint16_t modify_time;
			PACKED uint16_t modify_date;
			PACKED uint16_t first_cluster_lo;
			PACKED uint32_t size;
		} STD;
		struct LFN
		{
			PACKED unsigned char lfn_sequence;
			PACKED unsigned char lfn_chars_1[10];
			PACKED unsigned char lfn_attributes;
			PACKED unsigned char lfn_type;
			PACKED unsigned char lfn_checksum;
			PACKED unsigned char lfn_chars_2[12];
			PACKED uint16_t lfn_first_cluster;
			PACKED unsigned char lfn_chars_3[4];
		} LFN;
	} ENTRY;
}
FAT_RAW_DIRECTORY_ENTRY;
END_PACKED_STRUCT
#if defined(USE_PRAGMA_PACK)
#pragma pack()
#endif

/*!
 * <summary>
 * Stores information about directory entries.
 * </summary>
*/
typedef struct FAT_DIRECTORY_ENTRY 
{
	/*!
	 * <summary>The name of the file.</summary>
	 */
	unsigned char name[FAT_MAX_FILENAME + 1];
	/*!
	 * <summary>The list of file attributes ORed together.</summary>
	*/
	unsigned char attributes;
	/*!
	 * <summary>The creation timestamp of the file.</summary>
	*/
	time_t create_time;
	/*!
	 * <summary>The modification timestamp of the file.</summary>
	 */
	time_t modify_time;
	/*!
	 * <summary>The access timestamp of the file.</summary>
	*/
	time_t access_time;
	/*!
	 * <summary>The size of the file.</summary>
	*/
	uint32_t size;
	/*!
	 * <summary>Reserved for internal use.</summary>
	*/
	uint32_t sector_addr;
	/*!
	 * <summary>Reserved for internal use.</summary>
	*/
	uint16_t sector_offset;
	/*!
	 * <summary>Reserved for internal use.</summary>
	*/
	FAT_RAW_DIRECTORY_ENTRY raw;
}
FAT_DIRECTORY_ENTRY;

/*
// Holds the internal state of a directory query.
*/
typedef struct FAT_QUERY_STATE 
{
	unsigned char Attributes;
	uint16_t current_sector;
	uint32_t current_cluster;
	FAT_RAW_DIRECTORY_ENTRY* current_entry_raw;
	unsigned char* buffer;

	#if defined(NO_STRUCT_PACKING) || defined(BIG_ENDIAN)
	uint16_t current_entry_raw_offset;
	FAT_RAW_DIRECTORY_ENTRY current_entry_raw_mem;
	#else
	FAT_RAW_DIRECTORY_ENTRY* first_entry_raw;
	#endif
	/*
	// LFN support members
	*/
	#if !defined(FAT_DISABLE_LONG_FILENAMES)
	uint16_t long_filename[256];
	unsigned char lfn_sequence;
	unsigned char lfn_checksum;
	#endif
	/*
	// buffer (MUST ALWAYS BE LAST!!!)
	*/
	unsigned char buff[MAX_SECTOR_LENGTH];
}
FAT_QUERY_STATE;

/*!
 * <summary>Callback function for asynchronous IO.</summary>
*/
typedef void FAT_ASYNC_CALLBACK(void* context, uint16_t* state);

/*!
 * <summary>Callback function for asynchronous STREAMING IO.</summary>
 */
typedef void FAT_STREAM_CALLBACK(void* context, uint16_t* state, unsigned char** buffer, uint16_t* response);

/*
 * holds the state of a read or write operation
 */
typedef struct FAT_OP_STATE 
{
	uint32_t pos;
	uint16_t bytes_remaining;
	uint32_t sector_addr;
	uint16_t* async_state;
	uint32_t* bytes_read;
	uint16_t length;
	uint16_t storage_state;
	unsigned char* end_of_buffer;
	unsigned char* buffer;
	unsigned char internal_state;

	STORAGE_CALLBACK_INFO storage_callback_info;
	FAT_ASYNC_CALLBACK* callback;

	#if defined(FAT_STREAMING_IO)
	unsigned char* original_buffer;
	STORAGE_CALLBACK_INFO_EX storage_callback_info_ex;
	FAT_STREAM_CALLBACK* callback_ex;
	#endif

	void* callback_context;
}
FAT_OP_STATE;

/*!
 * <summary>
 * This is the file handle structure. All the fields in this structure
 * are reserved for internal use and should not be accessed directly by the
 * developer.
 * </summary>
 */
typedef struct FAT_FILE 
{
	/*!
		\internal
	*/
	FAT_VOLUME* volume;
	FAT_DIRECTORY_ENTRY directory_entry;
	uint32_t current_size;
	uint32_t current_clus_addr;
	uint32_t current_clus_idx;
	uint32_t current_sector_idx;
	uint32_t no_of_clusters_after_pos;
	uint16_t no_of_sequential_clusters;
	unsigned char* buffer_head;
	char buffer_dirty;
	char busy;
	unsigned char magic;
	unsigned char access_flags;
	FAT_OP_STATE op_state;
	unsigned char* buffer;
	#if defined(FAT_ALLOCATE_FILE_BUFFERS)
	unsigned char buffer_internal[MAX_SECTOR_LENGTH];	
	#endif
	/*!
		\endinternal
	*/
}	
FAT_FILE;

/*!
 * <summary>Holds the state of a directory query.</summary>
*/
typedef struct FILESYSTEM_QUERY 
{
	FAT_DIRECTORY_ENTRY current_entry;
	FAT_QUERY_STATE state;
}	
FAT_FILESYSTEM_QUERY;

/*
// declare shared buffer
*/
#if defined(FAT_ALLOCATE_SHARED_BUFFER)
#if defined(FAT_MULTI_THREADED)
DECLARE_CRITICAL_SECTION(fat_shared_buffer_lock);
#endif
extern unsigned char fat_shared_buffer[MAX_SECTOR_LENGTH];
extern uint32_t fat_shared_buffer_sector;
#endif

#if !defined(FAT_USE_SYSTEM_TIME)
/*!
// <summary>Registers the function that gets the system time.</summary>
*/
void fat_register_system_time_function(FAT_GET_SYSTEM_TIME system_time);
#endif

/*!
 * <summary>Initializes the FAT library.</summary>
 */
void fat_init();

/*!
 * <summary>
 * Mounts a FAT volume. 
 * </summary>
 * <param name="volume">A pointer to a volume handle structure (FAT_VOLUME).</param>
 * <param name="device">A pointer to the storage device driver STORAGE_DEVICE structure.</param>
 * <returns>One of the return codes defined in fat.h.</returns> 
 */
uint16_t fat_mount_volume
(
	FAT_VOLUME* volume, 
	STORAGE_DEVICE* device
);

/**
// dismounts a FAT volume
*/
uint16_t fat_dismount_volume
(
	FAT_VOLUME* volume
);

/**
 * <summary>Gets the sector size of a volume in bytes.</summary>
 * <param name="volume">A pointer to the volume handle.</param>
 */
uint16_t fat_get_sector_size
(
	FAT_VOLUME* volume
);

uint16_t fat_get_file_entry
(
	FAT_VOLUME* volume, 
	char* path, 
	FAT_DIRECTORY_ENTRY* entry
);

/**
 * <summary>
 * Finds the first entry in a directory.
 * </summary>
 * <param name="volume">A pointer to the volume handle (FAT_VOLUME).</param>
 * <param name="path">The path of the directory to query.</param>
 * <param name="attributes">An ORed list of file attributes to filter the query.</param>
 * <param name="dir_entry">
 * A pointer-to-pointer to a FAT_DIRECTORY_ENTRY structure. 
 * When this function returns the pointer will be set to to point to the directory entry.
 * </param>
 * <param name="query">A pointer to a FAT_FILESYSTEM_QUERY that will be initialized as the query handle.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
*/
uint16_t fat_find_first_entry
(
	FAT_VOLUME* volume, 
	char* path, 
	unsigned char attributes, 
	FAT_DIRECTORY_ENTRY** dir_entry,
	FAT_FILESYSTEM_QUERY* query
);

/**
 * <summary>
 * Finds the next entry in a directory.
 * </summary>
 * <param name="volume">A pointer to the volume handle (FAT_VOLUME).</param>
 * <param name="dir_entry">
 * A pointer-to-pointer to a FAT_DIRECTORY_ENTRY structure.
 * When this function returns the pointer will be set to point the the directory entry.
 * </param>
 * <param name="query">The file system query object.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
*/
uint16_t fat_find_next_entry
(
	FAT_VOLUME* volume, 
	FAT_DIRECTORY_ENTRY** dir_entry,
	FAT_FILESYSTEM_QUERY* query
);

/**
 * <summary>
 * Creates a new directory on the volume.
 * </summary>
 * <param name="volume">A pointer to the volume handle (FAT_VOLUME).</param>
 * <param name="filename">The full path to the new directory.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
*/
uint16_t fat_create_directory
(
	FAT_VOLUME* volume, 
	char* filename
);

/**
 * <summary>
 * Deletes a file.
 * </summary>
 * <param name="volume">A pointer to the volume handle (FAT_VOLUME structure).</param>
 * <param name="filename">The full path and filename of the file to delete.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
*/
uint16_t fat_file_delete
(
	FAT_VOLUME* volume, 
	char* filename
);

/**
 * <summary>
 * Renames a file.
 * </summary>
 * <param name="volume">A pointer to the volume handle (FAT_VOLUME structure).</param>
 * <param name="original_filename">The full path and original filename of the file to be renamed.</param>
 * <param name="new_filename">The full path and new filename for the file.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
*/
uint16_t fat_file_rename
(
	FAT_VOLUME* volume,
	char* original_filename,
	char* new_filename
);

/**
 * <summary>
 * Opens or create a file.
 * </summary>
 * <param name="volume">A pointer to the volume handle (FAT_VOLUME structure).</param>
 * <param name="filename">The full path and filename of the file to open.</param>
 * <param name="access_flags">An ORed list of one or more of the access flags defined in fat.h</param>
 * <param name="file">A pointer to a file handle FAT_FILE structure.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
*/
uint16_t fat_file_open
(
	FAT_VOLUME* volume,
	char* filename, 
	unsigned char access_flags, 
	FAT_FILE* file
);

/**
 * <summary>
 * Sets an external buffer for this file handle.
 * </summary>
*/
uint16_t fat_file_set_buffer
(
	FAT_FILE* file, 
	unsigned char* buffer
);

/**
 * <summary>
 * Gets the unique identifier of the file.
 * </summary>
 * <param name="file">An open file handle.</param>
 */
uint32_t fat_file_get_unique_id
(
	FAT_FILE* file
);

/**
 * <summary>
 * Allocates disk space to an open file.
 * </summary>
 * <param name="file">A pointer to a file handle FAT_FILE structure.</param>
 * <param name="bytes">The amount of disk space (in bytes) to be allocated.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
 * <remarks>
 * When writting large files in small chunks calling this function to pre-allocate 
 * drive space significantly improves write speed. Any space that is not used will be freed
 * when the file is closed.
 *
 * This function will allocate enough disk space to write the requested amount of
 * bytes after the current poisition of the file. So if you request 5K bytes and there's 
 * already 2K bytes allocated after the cursor position it will only allocate 3K bytes.
 * All disk allocations are done in multiples of the cluster size.
 * </remarks>
*/
uint16_t fat_file_alloc
(
	FAT_FILE* file,
	uint32_t bytes
);

/**
 * <summary>
 * Moves the file cursor to a new position within the file.
 * </summary>
 * <param name="file">A pointer to the file handle FAT_FILE structure.</param>
 * <param name="offset">The offset of the new cursor position relative to the position specified by the mode param.</param>
 * <param name="mode">One of the supported seek modes: FAT_SEEK_START, FAT_SEEK_CURRENT, or FAT_SEEK_END.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
 * <remarks>
 * If FAT_SEEK_END is specified the offset must be 0 or the function will fail.
 * </remarks>
*/
uint16_t fat_file_seek
(
	FAT_FILE* file,
	uint32_t offset,
	char mode
);

/**
 * <summary>
 * Writes the specified number of bytes to the current position on an opened file.
 * </summary>
 * <param name="file">A pointer to a file handle FAT_FILE structure.</param>
 * <param name="buffer">A buffer containing the bytes to be written.</param>
 * <param name="length">The amount of bytes to write.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
*/
uint16_t fat_file_write
(
	FAT_FILE* file, 
	unsigned char* buffer, 
	uint32_t length
);	

/**
 * <summary>
 * Writes the specified number of bytes to the current position on an opened file asynchronously.
 * </summary>
 * <param name="handle">A pointer to a file handle FAT_FILE structure.</param>
 * <param name="buffer">A buffer containing the bytes to be written.</param>
 * <param name="length">The amount of bytes to write.</param>
 * <param name="result">A pointer to a 16-bit unsigned integer where the result of the operation will be saved.</param>
 * <param name="callback">A callback function that will be called when the operation completes.</param>
 * <param name="callback_context">A pointer that will be passed to the callback function.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
 * <remarks>
 * If the operation is initialized successfully this function will return FAT_SUCCESS, however the operation
 * may still fail and the actual return code is stored on the 16 bit integer pointed to by the result parameter
 * when the operation completes.
 * </remarks>
*/
uint16_t fat_file_write_async
( 
	FAT_FILE* handle, 
	unsigned char* buffer, 
	uint32_t length, 
	uint16_t* result, 
	FAT_ASYNC_CALLBACK* callback,
	void* callback_context
);

/*!
 * <summary>Enables STREAMING IO to flash device</summary>
 */
#if defined(FAT_STREAMING_IO)
uint16_t fat_file_write_stream
(
	FAT_FILE* handle, 
	unsigned char* buff, 
	uint32_t length, 
	uint16_t* async_state, 
	FAT_STREAM_CALLBACK* callback, 
	void* callback_context
);
#endif

/**
 * <summary>
 * Reads the specified number of bytes from the current position on an opened file.
 * </summary>
 * <param name="handle">A pointer to a file handle FAT_FILE structure.</param>
 * <param name="buffer">A buffer where the bytes will be copied to.</param>
 * <param name="length">The amount of bytes to read.</param>
 * <param name="bytes_read">A pointer to a 32 bit integer where the amount of bytes read will be written to.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
*/
uint16_t fat_file_read
(
	FAT_FILE* handle, 
	unsigned char* buffer, 
	uint32_t length,
	uint32_t* bytes_read
);

/**
 * <summary>
 * Reads the specified number of bytes from the current position on an opened file asynchronously.
 * </summary>
 * <param name="handle">A pointer to a file handle FAT_FILE structure.</param>
 * <param name="buffer">A buffer where the bytes will be copied to.</param>
 * <param name="length">The amount of bytes to read.</param>
 * <param name="bytes_read">A pointer to a 32 bit integer where the amount of bytes read will be written to.</param>
 * <param name="result">A pointer to a 16-bit unsigned integer where the result of the operation will be saved.</param>
 * <param name="callback">A callback function that will be called when the operation completes.</param>
 * <param name="callback_context">A pointer that will be passed to the callback function.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
 * <remarks>
 * If the operation is initialized successfully this function will return FAT_SUCCESS, however the operation
 * may still fail and the actual return code is stored on the 16 bit integer pointed to by the result parameter
 * when the operation completes.
 * </remarks>
*/
uint16_t fat_file_read_async
( 
	FAT_FILE* handle, 
	unsigned char* buffer, 
	uint32_t length,
	uint32_t* bytes_read,
	uint16_t* result, 
	FAT_ASYNC_CALLBACK* callback,
	void* callback_context
);

/**
 * <summary>
 * Flushes file buffers and updates directory entry.
 * </summary>
 * <param name="file">A pointer to the file handle (FAT_FILE) structure.</param>
*/
uint16_t fat_file_flush
(
	FAT_FILE* file
);

/**
 * <summary>
 * Closes an open file.
 * </summary>
 * <param name="handle">A pointer to the file handle FAT_FILE structure.</param>
 * <returns>One of the return codes defined in fat.h.</returns>
*/
uint16_t fat_file_close
( 
	FAT_FILE* handle 
);

#endif
