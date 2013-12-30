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

#ifndef SMLIB_SMLIB_H
#define SMLIB_SMLIB_H

/*! 
 * \file sm.h
 * \brief This header file contains all the functions needed
 * to mount and dismount drives, register file system drivers, and
 * perform file I/O operations.
 *
 */

/*
	@manonly */

#include <compiler.h>
#include "filesystem.h"
#include "..\fat32lib\storage_device.h"
#include <time.h>

/*
// Result codes.
*/
#define SM_SUCCESS									0x0
#define SM_FILESYSTEM_ALREADY_REGISTERED			0x1
#define SM_INSUFFICIENT_MEMORY						0x2
#define SM_DRIVE_LABEL_ALREADY_IN_USE				0x3
#define SM_INVALID_DRIVE_LABEL						0x4
#define SM_INVALID_PATH								0x5
#define SM_OPERATION_NOT_SUPPORTED_ACCROSS_VOLUMES	0x6
#define SM_INVALID_FILE_HANDLE						0x7
#define SM_COULD_NOT_MOUNT_VOLUME					0x8
#define SM_DEVICE_ALREADY_REGISTERED				0x9
#define SM_CANNOT_LOCK_FILE							0xA

/*
// Misc options.
*/
#define SM_MAX_DRIVE_LABEL_LENGTH				8
#define SM_PATH_SEPARATOR						'\\'	/* at this time backkslash is the only path separator supported */
#define SM_FILE_HANDLE_MAGIC					0x1e

/* #define SM_MULTI_THREADED */
#define SM_SUPPORT_FILE_LOCKS

/*
	@endmanonly */

/*!
 * <summary>Specifies that the file is read-only.</summary>
 */
#define SM_ATTR_READ_ONLY				0x1
/*!
 * <summary>Specifies that the file is hidden.</summary>
 */
#define SM_ATTR_HIDDEN					0x2
/*!
 * <summary>Specifies that the file is a system file.</summary>
 */
#define SM_ATTR_SYSTEM					0x4
/*!
 * <summary>Specifies that the file entry is the volume ID.</summary>
 */
#define SM_ATTR_VOLUME_ID				0x8
/*!
 * <summary>Specifies that the entry is a directory.</summary>
 */
#define SM_ATTR_DIRECTORY				0x10
/*!
 * <summary>Specifies that the file is to be archived.</summary>
 */
#define SM_ATTR_ARCHIVE					0x20 

/*
// file access flags
// =================
*/

/*!
 * <summary>Specifies that the file should be opened for read access.</summary>
 */
#define SM_FILE_ACCESS_READ						0x1
/*!
 * <summary>Specifies that the file should be opened for write access.</summary>
 */
#define SM_FILE_ACCESS_WRITE					0x2
/*!
 * <summary>Specifies that if the file exists it should be opened in append mode.</summary>
 */
#define SM_FILE_ACCESS_APPEND					(0x4)
/*!
 * <summary>Specifies that if the file exists it should be overwritten.</summary>
 */
#define SM_FILE_ACCESS_OVERWRITE				(0x8)
/*!
 * <summary>Specifies that if the file does not exists it should be created.</summary>
 */
#define SM_FILE_ACCESS_CREATE					(0x10) 
/*!
 * <summary>Specifies that the file should be opened in unbuffered mode.</summary>
 */
#define SM_FILE_FLAG_NO_BUFFERING				(0x20)
/*!
 * <summary>Specifies that the file should be optimized for flash stream writes.</summary>
 */
#define SM_FILE_FLAG_OPTIMIZE_FOR_FLASH			(0x40)

/*
// seek modes
// ==========
*/

/*!
 * <summary>Specifies that the seek operation should start at the beginning of the file.</summary>
 */
#define SM_SEEK_START							0x1
/*!
 * <summary>Specifies that the seek operation should start at the current position.</summary>
 */
#define SM_SEEK_CURRENT						0x2
/*!
 * <summary>Specified that the seek operation should set the file pointer at the end of the file.</summary>
 */
#define SM_SEEK_END							0x3

/*
 * stream responses
 */
#define SM_STREAM_RESPONSE_READY			STORAGE_MULTI_SECTOR_RESPONSE_READY
#define SM_STREAM_RESPONSE_SKIP				STORAGE_MULTI_SECTOR_RESPONSE_SKIP
#define SM_STREAM_RESPONSE_STOP				STORAGE_MULTI_SECTOR_RESPONSE_STOP

/*!
 * <summary>
 * This is the handle of a directory query. All the fields in this structure
 * are reserved for internal use and should not be accessed directly by the
 * developer.
 * </summary>
 * <seealso cref="sm_find_first_entry" />
 * <seealso cref="sm_find_next_entry" />
 * <seealso cref="sm_get_file_entry" />
 */
typedef struct SM_QUERY
{
	void* query_handle;
	void* volume;
}
SM_QUERY;

/*!
 * <summary>
 * This is the file handle structure. All fields in this structure are
 * reserved for internal use and should not be used directly by the developer.
 * </summary>
 */
typedef struct SM_FILE
{
	/*!
	 * \internal
	 */
	void* filesystem_file_handle;
	FILESYSTEM* filesystem;
	unsigned char magic;
	void* managed_buffer;
	/*!
	 * \endinternal
	 */
}
SM_FILE;

/*!
 * <summary>
 * This structure used to store information about a directory entry. It is
 * usually populated by sm_get_file_entry, sm_find_first_entry, and
 * sm_find_next_entry.
 * </summary>
 * \see sm_get_file_entry
 * \see sm_find_first_entry
 * \see sm_find_next_entry
 */
typedef struct SM_DIRECTORY_ENTRY
{
	/*!
	 * \brief The size of the file in bytes.
	 */
	uint32_t size;
	/*!
	 * \brief The timestamp for the creation of the file.
	 */
	time_t create_time;
	/*!
	 * \brief The timestamp for the last time that the file was modified.
	 */
	time_t modify_time;
	/*!
	 * \brief The timestamp of the last time that the file was accessed.
	 */
	time_t access_time;
	/*!
	 * \brief The file attributes ORed together.
	 */
	unsigned char attributes;
	/*!
	 * \brief The filename represented as a null terminated ASCII string.
	 */
	char name[256];
}
SM_DIRECTORY_ENTRY;

/*!
 * <summary>
 * This function is pointer is used to register for notifications
 * when a new volume is mounted. Once you register this function with
 * sm_register_volume_mounted_callback it will be called by this module
 * every time a new volume is mounted.
 * </summary>
 * <param name="label">The drive label of the mounted volume.</param>
 * <seealso cref="sm_register_volume_mounted_callback" />
 */
typedef void (*SM_VOLUME_MOUNTED_CALLBACK)(char* label);

/*!
 * <summary>
 * This function is pointer is used to register for notifications
 * when a new volume is dimounted. Once you register this function with
 * sm_register_volume_dismounted_callback it will be called by this module
 * every time a new volume is dismounted.
 * </summary>
 * <param name="label">The drive label of the mounted volume.</param>
 * <seealso cref="sm_register_volume_dismounted_callback" />
 */
typedef void (*SM_VOLUME_DISMOUNTED_CALLBACK)(char* label);

/*!
 * <summary>
 * This is the callback function for asynchronous IO. You can pass a pointer
 * of this type of any of the asynchronous IO functions and it will be called
 * when the operation completes.
 * </summary>
 * <param name="context">
 * The pointer that was passed to the asynchronous function through
 * the callback_context parameter.
 * </param>
 * <param name="result">The result of the asynchronous operation.</param>
 * <seealso cref="sm_file_write_async" />
 * <seealso cref="sm_file_read_async" />
 */
typedef void (*SM_ASYNC_CALLBACK)(void* context, uint16_t* result);

/*!
 * <summary>
 * This is the callback function for asynchronous STREAM IO. You MUST
 * pass a pointer of this type to sm_file_write_stream. When it finished
 * writting your buffer it will call this function.
 *
 * When smlib calls this function you must do one of the following:
 *
 * 1.	If you have more data to be written reload the buffer with
 *		it and set response to SM_STREAM_RESPONSE_READY.
 * 2.	If you want to write more data but the data is not yet ready
 *		set response to SM_STREAM_RESPONSE_SKIP. The file system will
 *		release the IO device and once it becomes available again it will
 *		call back so you can reload the buffer.
 * 3.	If you're done writing to the device set response to
 *		SM_STREAM_RESPONSE_STOP.
 * </summary>
 */
typedef void (*SM_STREAM_CALLBACK)(void* context, uint16_t* result, unsigned char** buffer, uint16_t* response);

/*!
 * <summary>
 * Registers a callback function that will be called everytime a
 * volume is mounted.
 * </summary>
 * <param name="callback">
 * A function pointer to the callback function.
 * </param>
 */
void sm_register_volume_mounted_callback
(
	SM_VOLUME_MOUNTED_CALLBACK callback
);

/*!
 * <summary>
 * Registers a callback function that will be called everytime a
 * volume is dismounted.
 * </summary>
 * <param name="callback">
 * A function pointer to the callback function.
 * </param>
*/
void sm_register_volume_dismounted_callback
(
	SM_VOLUME_DISMOUNTED_CALLBACK callback
);

/*!
 * <summary>
 * Registers a filesystem with smlib. This function needs to be
 * called before a drive can be mounted.
 * </summary>
 * <param name="filesystem">
 * A pointer to the FILESYSTEM structure of the file system driver.
 * </param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
*/
uint16_t sm_register_filesystem
(
	FILESYSTEM* filesystem
);

/*!
 * <summary>
 * Registers a device (such as an SD card slot) with smlib to be
 * mounted automatically whenever the device is inserted/connected.
 * This will cause smlib to request notifications from the device driver
 * when the device is connected/disconnected and it will mount the device
 * automatically with the specified drive label.
 * </summary>
 * <param name="device">
 * A pointer to the STORAGE_DEVICE structure of the device driver.
 * </param>
 * <param name="label">A drive label for the volume.</param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
 * <remarks>
 * You may call this function several times for the same device with 
 * different drive labels and if the device has multiple partitions it 
 * will mount one in each drive label.
 * </remarks>
*/
uint16_t sm_register_storage_device
(
	STORAGE_DEVICE* device, 
	char* label
);

/*!
 * <summary>
 * Mounts a volume.
 * </summary>
 * <param name="label">A drive label for the volume.</param>
 * <param name="storage_device">
 * A pointer to the STORAGE_DEVICE structure of the device driver.
 * </param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
*/
uint16_t sm_mount_volume
(
	char* label, 
	void* storage_device
);

/*!
 * <summary>
 * Dismounts a mounted volume.
 * </summary>
 * <param name="label">The drive label of the volume to dismount.</param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
 * <remarks>
 * If this function is called for a volume that is not actually
 * mounted it will return FILESYSTEM_SUCCESS without doing any actual
 * work.
 * </remarks>
*/
uint16_t sm_dismount_volume
(
	char* label
);

/*!
 * <summary>
 * Gets the sector size of a mounted volume.
 * </summary>
 * <param name="label">The drive label of the volume.</param>
 *
 */
uint32_t sm_get_volume_sector_size
(
	char* label
);

/*!
 * <summary>Gets the directory entry information for a file.</summary>
 * <param name="filename">The full path of the file.</param>
 * <param name="entry">
 * A pointer to an SM_DIRECTORY_ENTRY structure that will be populated
 * with the file information.
 * </param>
*/
uint16_t sm_get_file_entry
(
	char* filename,
	SM_DIRECTORY_ENTRY* entry
);

/*!
 * <summary>
 * Initializes a directory query and finds the 1st entry
 * in the specified directory.
 * </summary>
 * <param name="path">The full path of the directory to query.</param>
 * <param name="attributes">
 * A list of file attributes ORed together to be used as a query filter.
 * </param>
 * <param name="entry">
 * A pointer to a SM_DIRECTORY_ENTRY structure that will be
 * populated with the information about the next directory entry.
 * </param>
 * <param name="query">
 * A pointer to a directory query structure (SM_QUERY).
 * </param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
*/
uint16_t sm_find_first_entry
(
	char* path, 
	unsigned char attributes, 
	SM_DIRECTORY_ENTRY* entry, 
	SM_QUERY* query
);

/*!
 * <summary>
 * Finds the next entry in a directory.
 * </summary>
 * <param name="entry">
 * A pointer to a SM_DIRECTORY_ENTRY structure that will be
 * populated with the information about the next directory entry.
 * </param>
 * <param name="query">
 * A pointer to a directory query structure (SM_QUERY) that has been
 * initialized by sm_find_first_entry.
 * </param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
*/
uint16_t sm_find_next_entry
(
	SM_DIRECTORY_ENTRY* entry, 
	SM_QUERY* query
);

/*!
 * <summary>
 * Closes and frees all resources used by a directory query.
 * </summary>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
*/
uint16_t sm_find_close
(
	SM_QUERY* query
);

/*!
 * <summary>
 * Creates a directory.
 * </summary>
 * <param name="directory_name">The full path of the directory to be created.</param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
*/
uint16_t sm_create_directory
(
	char* directory_name
);

/*!
 * <summary>
 * Deletes a file.
 * </summary>
 * <param name="filename">The full path of the file to be deleted.</param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
 */
uint16_t sm_file_delete
(
	char* filename
);

/*!
 * <summary>
 * Renames a file.
 * </summary>
 * <param name="original_filename">The full path to the file to be renamed.</param>
 * <param name="new_filename">The full path to the new filename.</param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
 * <remarks>
 * Both filenames need to be on the same volume. If they're in different directories
 * the file will be moved. Moving files accross different volumes is not currently
 * supported.
 * </remarks>
*/
uint16_t sm_file_rename
(
	char* original_filename, 
	char* new_filename
);

/*!
 * <summary>
 * Opens or creates a file for read and/or write access.
 * </summary>
 * <param name="file">A pointer to a SM_FILE structure.</param>
 * <param name="filename">The full path of the file to open.</param>
 * <param name="access_flags">
 * One or more of the access flags defined in sm.h ORed together.
 * </param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
 * <remarks>
 * If the SM_FILE_FLAG_NO_BUFFERING access flag is used all IO operations
 * must be done on a sector boundry. You can call sm_get_volume_sector_size to
 * get the sector size of a mounted volume.
 * </remarks>
 * <seealso cref="sm_get_volume_sector_size" />
 */
uint16_t sm_file_open
(
	SM_FILE* file, 
	char* filename, 
	unsigned char access_flags
);

/*!
 * <summary>
 * Allocates enough clusters to a file as needed to write the
 * specified number of bytes beyond the current file pointer position.
 * </summary>
 * <param name="file">An open file handle.</param>
 * <param name="bytes">The number of bytes to allocate.</param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
 * <remarks>
 * The purpose of this function is to speed up file writes. It simply
 * allocates the clusters needed, it does not actually expand the file
 * (it's file size remains the same and you still cannot seek beyond the
 * last byte that has already been written. If you call this function before
 * writing to a file the write operation will be much faster, that's all
 * it does. If not all the clusters are used when the file is closed they
 * will be freed. Calling this function also helps to avoid fragmentation.
 * </remarks>
*/
uint16_t sm_file_alloc
(
	SM_FILE* file, 
	uint32_t bytes
);

/*!
 * <summary>
 * Moves the file pointer to the specified position.
 * </summary>
 * <param name="file">An open file handle.</param>
 * <param name="offset">The new position of the specified position within the file.</param>
 * <param name="mode">
 * One of the seek modes listed bellow.
 * </param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of
 * the return codes defined in filesystem.h and sm.h
 * </returns>
 * \see SM_SEEK_START
 * \see SM_SEEK_CURRENT
 * \see SM_SEEK_END
 * <remarks>
 * If an offset other than 0 is specified along with the SM_SEEK_END mode
 * this call will fail.
 * </remarks>
*/
uint16_t sm_file_seek
(
	SM_FILE* file, 
	uint32_t offset, 
	char mode
);

/*!
 * <summary>
 * Writes the specified number of bytes to a file synchronously
 * </summary>
 * <param name="file">An open file handle.</param>
 * <param name="buffer">The buffer containing the data to be written.</param>
 * <param name="bytes_to_write">The number of bytes to write</param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise it will return
 * one of the result codes defined in filesystem.h or sm.h.
 * </returns>
 * <seealso cref="sm_file_alloc" />
 * <seealso cref="sm_file_write_async" />
 */
uint16_t sm_file_write
(
	SM_FILE* file, 
	unsigned char* buffer, 
	uint32_t bytes_to_write
);

/*!
 * <summary>
 * Writes the specified number of bytes to a file asynchronously.
 * </summary>
 * <param name="file">An open file handle.</param>
 * <param name="buffer">The buffer containing the data to be written.</param>
 * <param name="bytes_to_write">The number of bytes to write</param>
 * <param name="result">
 * A pointer to a 16-bit integer where the result of the asynchronous write will
 * be stored once the operation completes.
 * </param>
 * <param name="callback">
 * An optional function pointer to a callback function to be called when the
 * asynchronous operation completes.
 * </param>
 * <param name="callback_context">
 * A pointer that will be passed to the callback function when the operation completes.
 * </param>
 * <returns>
 * If successful it will return FILESYSTEM_OP_IN_PROGRESS, otherwise it will return
 * one of the result codes defined in filesystem.h or sm.h.
 * </returns>
 * <seealso cref="sm_file_alloc" />
*/
uint16_t sm_file_write_async
(
	SM_FILE* file, 
	unsigned char* buffer, 
	uint32_t bytes_to_write, 
	uint16_t* result, 
	SM_ASYNC_CALLBACK callback, 
	void* callback_context
);

/*!
 * <summary>
 * Initiates a stream write. This function is used to write data continuosly
 * to the file system. When used with SD cards or other flash devices it will
 * write multiple sectors at once thus increasing performance. It accomplish this
 * without requiring you to allocate large buffers to hold multiple sectors. This
 * function supports both unbuffered and buffered IO but it is recommended that you
 * use unbuffered IO for better performance. Once you initiate a stream write with
 * this function the data you provided will be written to the device, and as soon
 * as the transfer is complete (even while the device is still writing the data)
 * the callback function provided through the callback parameter will be invoked.
 * In this function you must either reload the buffer or replace it with a different
 * one (a pointer to the buffer is passed to the function for this) and set the
 * response argument to SM_STREAM_RESPONSE_READY. As long as you do this everytime
 * and there are no other requests pending the driver will use a single command
 * to write multiple sectors to the device. If you don't have the data ready when
 * the callback function is called you can set the response pointer to SM_STREAM_RESPONSE_SKIP.
 * In that case the driver will complete the multiple sector transfer and release
 * the device for other requests to use, once the device becomes available again the
 * callback function will be called. This process is repeated until you either supply
 * the data and set the response pointer to SM_STREAM_RESPONSE_READY or end the
 * transfer by setting the response pointer to SM_STREAM_RESPONSE_STOP. Once you
 * set the response pointer to SM_STREAM_RESPONSE_STOP the callback function will
 * be called once more to notify you of the result. If an error occurs before that
 * transaction is complete the result pointer will be set to the error code. When
 * the transaction is completed successfully the result pointer is set to SM_SUCCESS,
 * otherwise if the file system is waiting for you to reload the buffer the result
 * pointer is set to SM_AWAITING_DATA. Please see the examples included for more
 * details.
 * </summary>
 * <param name="file">An open file handle.</param>
 * <param name="buffer">The buffer containing the data to be written.</param>
 * <param name="bytes_to_write">The number of bytes to write</param>
 * <param name="result">
 * A pointer to a 16-bit integer where the result of the asynchronous write will
 * be stored once the operation completes.
 * </param>
 * <param name="callback">
 * An optional function pointer to a callback function to be called when the
 * asynchronous operation completes.
 * </param>
 * <param name="callback_context">
 * A pointer that will be passed to the callback function when the operation completes.
 * </param>
 * <returns>
 * If successful it will return FILESYSTEM_OP_IN_PROGRESS, otherwise it will return
 * one of the result codes defined in filesystem.h or sm.h.
 * </returns>
 * <seealso cref="sm_file_alloc" />
 */
uint16_t sm_file_write_stream
(
	SM_FILE* file, 
	unsigned char* buffer, 
	uint32_t bytes_to_write, 
	uint16_t* result, 
	SM_STREAM_CALLBACK callback, 
	void* callback_context
);

/*!
 * <summary>
 * Reads the specified number of bytes from the file.
 * </summary>
 * <param name="file">An open file handle.</param>
 * <param name="buffer">The buffer where the data will be written to.</param>
 * <param name="bytes_to_read">The number of bytes to be written.</param>
 * <param name="bytes_read">
 * A pointer to a 16-bit integer where the number of bytes read will be stored 
 * when the operation completes.
 * </param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of the
 * error codes defined in filesystem.h or sm.h
 * </returns>
 * <seealso cref="sm_file_read_async" />
 * <seealso cref="sm_file_open" />
*/
uint16_t sm_file_read
(
	SM_FILE* file, 
	unsigned char* buffer, 
	uint32_t bytes_to_read, 
	uint32_t* bytes_read
);

/*!
 * <summary>
 * Reads the specified number of bytes from the file asynchronously.
 * </summary>
 * <param name="file">An open file handle.</param>
 * <param name="buffer">The buffer where the data will be written to.</param>
 * <param name="bytes_to_read">The number of bytes to be written.</param>
 * <param name="bytes_read">
 * A pointer to a 32-bit integer where the number of bytes read will be stored 
 * when the operation completes.
 * </param>
 * <param name="result">
 * A pointer to a 16-bit integer where the result of the asynchronous operation
 * will be stored once it completes.
 * </param>
 * <param name="callback">
 * An optional function pointer to a callback function to be called when the
 * asynchronous operation completes.
 * </param>
 * <param name="callback_context">
 * A pointer that will be passed to the callback function when the operation completes.
 * </param>
 * <returns>
 * If successful it will return FILESYSTEM_OP_IN_PROGRESS, otherwise one of the
 * error codes defined in filesystem.h or sm.h
 * </returns>
*/
uint16_t sm_file_read_async
(
	SM_FILE* file, 
	unsigned char* buffer, 
	uint32_t bytes_to_read, 
	uint32_t* bytes_read, 
	uint16_t* result, 
	SM_ASYNC_CALLBACK callback, 
	void* callback_context
);

/*!
 * <summary>
 * Flushes the file buffer to the drive and updates the file size and timestamps.
 * </summary>
 * <param name="file">An open file handle.</param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of the result
 * codes defined on filesystem.h and sm.h.
 * </returns>
*/
uint16_t sm_file_flush
(
	SM_FILE* file
);

/*!
 * <summary>
 * Flushes the file buffer to the drive, updates the file size and timestamps,
 * and closes the file handle.
 * </summary>
 * <param name="file">An open file handle.</param>
 * <returns>
 * If successful it will return FILESYSTEM_SUCCESS, otherwise one of the result
 * codes defined on filesystem.h and sm.h.
 * </returns>
*/
uint16_t sm_file_close
(
	SM_FILE* file
);

/*!
 * <summary>
 * Sets the file buffer. This function is useful if the device
 * driver requires you to use memory from a specific location for
 * asynchronous operations (ie. DMA memory). Or if you want more control
 * about memory allocation. You can open the file with the 
 * SM_FILE_FLAG_NO_BUFFERING flag and immediately call this function
 * to set the buffer. That will switch the file handle back to buffered
 * mode.
 * </summary>
 * <param name="file">An open file handle.</param>
 * <param name="buffer">The new file buffer.</param>
*/
uint16_t sm_file_set_buffer
(
	SM_FILE* file, 
	unsigned char* buffer
);

#endif
