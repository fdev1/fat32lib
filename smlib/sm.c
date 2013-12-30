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

#include "sm.h"
#include <stdlib.h>
#include <string.h>

/*
// structure used for keeping track of registered devices
*/
typedef struct SM_DEVICE
{
	uint16_t id;
	STORAGE_DEVICE device;
	char label[SM_MAX_DRIVE_LABEL_LENGTH + 1];
	struct SM_DEVICE* next;
}
SM_DEVICE;

/*
// structure for keeping track of registered filesystems
*/
typedef struct SM_FILESYSTEM
{
	FILESYSTEM filesystem;
	struct SM_FILESYSTEM* next;
}
SM_FILESYSTEM;

/*
// structure for keeping track of mounted volumes
*/
typedef struct SM_VOLUME
{
	char drive_label[SM_MAX_DRIVE_LABEL_LENGTH + 1];
	void* volume;
	FILESYSTEM* filesystem;
	struct SM_VOLUME* next;
}
SM_VOLUME;

/*
// open file structure
*/
typedef struct SM_OPEN_FILE
{
	uint32_t id;
	SM_FILE* handle;
	struct SM_OPEN_FILE* next;
}
SM_OPEN_FILE;

/*
// gobal variables
*/
static SM_FILESYSTEM* registered_filesystems = 0;						/* pointer to list of registered filesystems */
static SM_DEVICE* registered_devices = 0;								/* pointer to list of registered devices */
static SM_VOLUME* mounted_volumes = 0;									/* pointer to list of mounted volumes */
static SM_VOLUME_MOUNTED_CALLBACK volume_mounted_callback = 0;			/* callback function for notification of new mounted volumes */
static SM_VOLUME_DISMOUNTED_CALLBACK volume_dismounted_callback = 0;	/* callback function for notification of dismounted volumes */

#if defined(SM_SUPPORT_FILE_LOCKS)
static SM_OPEN_FILE* open_files = 0;									/* pointer to list of open files */
#if defined(SM_MULTI_THREADED)
DEFINE_CRITICAL_SECTION(open_files_lock);
#endif
#endif

/*
// static function prototypes
*/
static uint16_t sm_resolve_path(char* path, SM_VOLUME** volume, char** volume_path);
static void sm_media_changed_callback(uint16_t device_id, char media_ready);

/*
// registers a filesystem with smlib
*/
uint16_t sm_register_filesystem(FILESYSTEM* filesystem)
{
	SM_FILESYSTEM** last_filesystem;
	/*
	// set pointer to 1st filesystem registered
	*/
	last_filesystem = &registered_filesystems;
	/*
	// check that the filesystem is not already registered
	*/
	while ((*last_filesystem))
	{
		if (!strcmp((*last_filesystem)->filesystem.name, filesystem->name))
		{
			return SM_FILESYSTEM_ALREADY_REGISTERED;
		}
		/*
		// set pointer to the next filesystem on the list
		*/
		last_filesystem = &((*last_filesystem)->next);
	}
	/*
	// set pointer to 1st filesystem registered
	*/
	last_filesystem = &registered_filesystems;
	/*
	// set pointer to the last filesystem on the list
	*/
	while ((*last_filesystem))
		last_filesystem = &((*last_filesystem)->next);
	/*
	// allocate memory to store the filesystem interface
	*/
	(*last_filesystem) = malloc(sizeof(SM_FILESYSTEM));
	if (!(*last_filesystem))
		return SM_INSUFFICIENT_MEMORY;
	/*
	// copy the filesystem interface
	*/
	(*last_filesystem)->filesystem = *filesystem;
	(*last_filesystem)->next = 0;
	/*
	// return success.
	*/
	return SM_SUCCESS;
}

/*
// this associates a storage device with a drive label and causes
// smlib to subscribe for notifications when media is inserted or removed
// from the device and automatically mount the volume with the registered
// drive label
*/
uint16_t sm_register_storage_device(STORAGE_DEVICE* device, char* label)
{
	SM_DEVICE** last_device;

	if (!strlen(label) || strlen(label) > SM_MAX_DRIVE_LABEL_LENGTH)
		return SM_INVALID_DRIVE_LABEL;
	/*
	// set pointer to 1st device registered
	*/
	last_device = &registered_devices;
	/*
	// check that the same device is not registered and that
	// another device is not registered with the same drive label
	*/
	while (*last_device)
	{
		if (!strcmp((*last_device)->label, label))
		{
			return SM_DRIVE_LABEL_ALREADY_IN_USE;
		}
		if ((*last_device)->id == device->get_device_id(device->driver))
		{
			return SM_DEVICE_ALREADY_REGISTERED;
		}
		last_device = &((*last_device)->next);
	}
	/*
	// set pointer to 1st device registered
	*/
	last_device = &registered_devices;
	/*
	// set pointer to the last device on the list
	*/
	while (*last_device)
		last_device = &((*last_device)->next);
	/*
	// allocate memory for this device
	*/
	(*last_device) = malloc(sizeof(SM_DEVICE));
	if (!(*last_device))
		return SM_INSUFFICIENT_MEMORY;
	/*
	// copy device interface and label
	*/
	strcpy((*last_device)->label, label);
	(*last_device)->id = device->get_device_id(device->driver);
	(*last_device)->device = *device;
	(*last_device)->next = 0;
	/*
	// register the callback
	*/
	device->register_media_changed_callback(device->driver, 
		(STORAGE_MEDIA_CHANGED_CALLBACK) &sm_media_changed_callback);
	/*
	// SUCCESS!
	*/
	return SM_SUCCESS;
}

/*
// registers a callback function that will be called everytime
// a new volume is mounted.
*/
void sm_register_volume_mounted_callback(SM_VOLUME_MOUNTED_CALLBACK callback)
{
	volume_mounted_callback = callback;
}

/*
// registers a callback function to receive notification when a 
// volume is dismounted.
*/
void sm_register_volume_dismounted_callback(SM_VOLUME_DISMOUNTED_CALLBACK callback)
{
	volume_dismounted_callback = callback;
}

/*
// mounts a volume
*/
uint16_t sm_mount_volume(char* label, void* storage_device)
{
	void* vol = 0;
	uint16_t ret;
	SM_VOLUME** last_volume;
	SM_FILESYSTEM** current_filesystem;
	/*
	// check that the drive label is valid
	*/
	if (!strlen(label) || strlen(label) > SM_MAX_DRIVE_LABEL_LENGTH)
		return SM_INVALID_DRIVE_LABEL;
	/*
	// set pointer to the 1st volume on the list
	*/
	last_volume = &mounted_volumes;
	/*
	// check that the drive label is available
	*/
	while ((*last_volume))
	{
		if (!strcmp((*last_volume)->drive_label, label))
			return SM_DRIVE_LABEL_ALREADY_IN_USE;
		/*
		// set pointer to the next volume on the list
		*/
		last_volume = &((*last_volume)->next);
	}
	/*
	// set pointer to 1st filesystem on the list
	*/
	current_filesystem = &registered_filesystems;
	/*
	// try to mount the volume with all registered
	// filesystems.
	*/
	while ((*current_filesystem))
	{
		/*
		// allocate memory for the volume handle for the
		// current filesystem
		*/
		vol = malloc((*current_filesystem)->filesystem.volume_handle_size);
		if (!vol)
			return SM_INSUFFICIENT_MEMORY;
		/*
		// call on the filesystem to mount the volume, if it works
		// then break out of this loop, otherwise free the volume handle
		*/
		ret = (*current_filesystem)->filesystem.mount_volume(vol, storage_device);
		if (ret == FILESYSTEM_SUCCESS)
		{
			break;
		}
		else
		{
			free(vol);
			vol = 0;
		}
		/*
		// set pointer to the next filesystem on the list
		*/
		current_filesystem = &((*current_filesystem)->next);
	}
	/*
	// if the volume was mounted successfully
	*/
	if (vol)
	{
		/*
		// allocate memory for the volume list entry
		*/
		(*last_volume) = malloc(sizeof(SM_VOLUME));
		if (!(*last_volume))
		{
			/*
			// free the volume handle
			*/
			free(vol);
			/*
			// return insufficient memory error code
			*/
			return SM_INSUFFICIENT_MEMORY;
		}
		/*
		// copy the newly mounted volume info to the list
		*/
		strcpy((*last_volume)->drive_label, label);
		(*last_volume)->volume = vol;
		(*last_volume)->filesystem = &((*current_filesystem)->filesystem);
		(*last_volume)->next = 0;
		/*
		// if notification of mounted volumes has been requested
		// call the notification callback function
		*/
		if (volume_mounted_callback)
			volume_mounted_callback(label);
		/*
		// return success code
		*/
		return SM_SUCCESS;
	}
	/*
	// if we got this far it means we were unable
	// to mount this volume
	*/
	return SM_COULD_NOT_MOUNT_VOLUME;
}

/*
// unmounts a volume
*/
uint16_t sm_dismount_volume(char* label)
{
	SM_VOLUME** current_volume;
	SM_VOLUME* tmp;
	/*
	// set pointer to the 1st item of the list
	*/
	current_volume = &mounted_volumes;
	/*
	// look for the volume to be freed, if we find it
	// free the memory used by it's handle
	*/
	while ((*current_volume))
	{
		if (!strcmp(label, (*current_volume)->drive_label))
		{
			if ((*current_volume)->volume)
			{
				/*
				// call the filesystem's dismount volume function
				*/
				(*current_volume)->filesystem->dismount_volume((*current_volume)->volume);
				/*
				// free the volume handle
				*/
				free((*current_volume)->volume);
				/*
				// if a callback function for volume dismounts has been
				// registered call it
				*/
				if (volume_dismounted_callback)
					volume_dismounted_callback(label);
			}
			break;
		}
		current_volume = &((*current_volume)->next);
	}
	/*
	// if the volume was found remove it from the list
	// and free the memory used by the list entry
	*/
	if ((*current_volume))
	{
		/*
		// save a copy of the current volume pointer
		*/
		tmp = (*current_volume);
		/*
		// remove the current volume from the list
		*/
		(*current_volume) = (*current_volume)->next;
		/*
		// free the volume entry
		*/
		free(tmp);
	}
	/*
	// success
	*/
	return SM_SUCCESS;
}

/*
// gets the sector size of a mounted volume.
*/
uint32_t sm_get_volume_sector_size(char* label)
{
	SM_VOLUME** current_volume = &mounted_volumes;

	while ((*current_volume))
	{
		/*
		// if this is the volume in question get it's sector size
		// and return it
		*/
		if (!strcmp((*current_volume)->drive_label, label))
		{
			return (*current_volume)->filesystem->get_sector_size((*current_volume)->volume);
		}
		/*
		// move on to the next volume
		*/
		(*current_volume) = (*current_volume)->next;
	}
	/*
	// if we can't find the volume return 0
	*/
	return 0;
}

/*
// creates a directory
*/
uint16_t sm_create_directory(char* pathname)
{
	SM_VOLUME* volume;
	char* volume_path;
	uint16_t ret;
	/*
	// resolve the pathname
	*/
	ret = sm_resolve_path(pathname, &volume, &volume_path);
	if (ret != SM_SUCCESS)
		return ret;
	/*
	// call on the appropriate filesystem to perform the operation
	*/
	return volume->filesystem->create_directory(volume->volume, volume_path);
}

/*
// deletes a file
*/
uint16_t sm_file_delete(char* filename)
{
	uint16_t ret;
	SM_VOLUME* volume;
	char* volume_path;
	/*
	// resolve the file path
	*/
	ret = sm_resolve_path(filename, &volume, &volume_path);
	if (ret != SM_SUCCESS)
		return ret;
	/*
	// call on the filesystem to perform the operation
	*/
	return volume->filesystem->file_delete(volume->volume, volume_path);
}

/*
// renames a file
*/
uint16_t sm_file_rename(char* original_filename, char* new_filename)
{
	uint16_t ret;
	SM_VOLUME* volume;
	SM_VOLUME* volume_1;
	char* original_volume_path;
	char* new_volume_path;
	/*
	// resolve paths
	*/
	ret = sm_resolve_path(original_filename, &volume, &original_volume_path);
	if (ret != SM_SUCCESS)
		return ret;
	ret = sm_resolve_path(new_filename, &volume_1, &new_volume_path);
	if (ret != SM_SUCCESS)
		return ret;
	/*
	// make sure that both paths are on the same volume
	*/
	if (volume != volume_1)
		return SM_OPERATION_NOT_SUPPORTED_ACCROSS_VOLUMES;
	/*
	// call on the filesystem to perform the operation
	*/
	return volume->filesystem->file_rename(volume->volume, original_volume_path, new_volume_path);
}

/*
// opens a file
*/
uint16_t sm_file_open(SM_FILE* file, char* filename, unsigned char access_flags)
{
	uint16_t ret;
	SM_VOLUME* volume;
	char* volume_path;
	/*
	// resolve path
	*/
	ret = sm_resolve_path(filename, &volume, &volume_path);
	if (ret != SM_SUCCESS)
		return ret;
	/*
	// allocate filesystem file handle
	*/
	file->filesystem_file_handle = malloc(volume->filesystem->file_handle_size);
	if (!file->filesystem_file_handle)
		return SM_INSUFFICIENT_MEMORY;
	/*
	// store pointer to filesystem in file handle
	*/
	file->filesystem = volume->filesystem;
	/*
	// call on filesystem to perform operation
	*/
	ret = volume->filesystem->file_open(volume->volume, volume_path, access_flags, file->filesystem_file_handle);
	if (ret != FILESYSTEM_SUCCESS)
	{
		free(file->filesystem_file_handle);
		file->filesystem_file_handle = 0;
		return ret;
	}
	#if defined(SM_SUPPORT_FILE_LOCKS)
	/*
	// set implicit access flags
	*/
	access_flags |= SM_FILE_ACCESS_READ;
	if (access_flags & (SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_APPEND | SM_FILE_ACCESS_OVERWRITE))
		access_flags |= SM_FILE_ACCESS_WRITE;
	/*
	// if the file was open for write access then lock it
	*/
	if (access_flags & SM_FILE_ACCESS_WRITE)
	{
		uint32_t file_id;
		SM_OPEN_FILE** open_file;
		file_id = volume->filesystem->file_get_unique_id(file->filesystem_file_handle);
		_ASSERT(file_id);
		/*
		// lock the list of locked files
		*/
		#if defined(SM_MULTI_THREADED)
		ENTER_CRITICAL_SECTION(open_files_lock);
		#endif
		/*
		// go through the list of locked files and if the file is
		// already locked return SM_CANNOT_LOCK_FILE code
		*/
		open_file = &open_files;
		while ((*open_file))
		{
			if ((*open_file)->id == file_id)
			{
				free(file->filesystem_file_handle);
				file->filesystem_file_handle = 0;
				#if defined(SM_MULTI_THREADED)
				LEAVE_CRITICAL_SECTION(open_files_lock);
				#endif
				return SM_CANNOT_LOCK_FILE;
			}
			open_file = &(*open_file)->next;
		}
		/*
		// lock the file by adding an entry to the locked files list
		*/
		(*open_file) = malloc(sizeof(SM_OPEN_FILE));
		if (!(*open_file))
		{
			free(file->filesystem_file_handle);
			file->filesystem_file_handle = 0;
			#if defined(SM_MULTI_THREADED)
			LEAVE_CRITICAL_SECTION(open_files_lock);
			#endif
			return SM_INSUFFICIENT_MEMORY;
		}
		(*open_file)->id = file_id;
		(*open_file)->handle = file;
		(*open_file)->next = 0;
		/*
		// unlock open files list
		*/
		#if defined(SM_MULTI_THREADED)
		LEAVE_CRITICAL_SECTION(open_files_lock);
		#endif
	}
	#endif
	/*
	// allocate a buffer for the file
	*/
	if (!(access_flags & SM_FILE_FLAG_NO_BUFFERING) &&
			volume->filesystem->file_buffer_allocation_required)
	{
		file->managed_buffer = malloc(volume->filesystem->get_sector_size(volume->volume));
		if (!file->managed_buffer)
		{
			free(file->filesystem_file_handle);
			file->filesystem_file_handle = 0;
			return SM_INSUFFICIENT_MEMORY;
		}
		/*
		// set the buffer
		*/
		volume->filesystem->file_set_buffer(file->filesystem_file_handle, file->managed_buffer);
	}
	else
	{
		file->managed_buffer = 0;
	}
	/*
	// set the magic char
	*/
	file->magic = SM_FILE_HANDLE_MAGIC;
	/*
	// return success
	*/
	return SM_SUCCESS;
}

/*
// allocates disk space for a file
*/
uint16_t sm_file_alloc(SM_FILE* file, uint32_t bytes)
{
	/*
	// check that we got a valid file handle
	*/
	if (!file)
		return SM_INVALID_FILE_HANDLE;
	if (file->magic != SM_FILE_HANDLE_MAGIC)
		return SM_INVALID_FILE_HANDLE;
	/*
	// call on the filesystem to perform the operation
	*/
	return file->filesystem->file_alloc(file->filesystem_file_handle, bytes);
}

uint16_t sm_file_set_buffer(SM_FILE* file, unsigned char* buffer)
{
	/*
	// check that we got a valid file handle
	*/
	if (!file)
		return SM_INVALID_FILE_HANDLE;
	if (file->magic != SM_FILE_HANDLE_MAGIC)
		return SM_INVALID_FILE_HANDLE;
	/*
	// if the file has a managed buffer free it
	*/
	if (file->managed_buffer)
	{
		free(file->managed_buffer);
		file->managed_buffer = 0;
	}
	/*
	// call on the filesystem to perform the operation
	*/
	return file->filesystem->file_set_buffer(file->filesystem_file_handle, buffer);
}

/*
// performs a seek operation on a file.
*/
uint16_t sm_file_seek(SM_FILE* file, uint32_t offset, char mode)
{
	/*
	// check that we got a valid file handle
	*/
	if (!file)
		return SM_INVALID_FILE_HANDLE;
	if (file->magic != SM_FILE_HANDLE_MAGIC)
		return SM_INVALID_FILE_HANDLE;
	/*
	// call on the filesystem to perform the operation
	*/
	return file->filesystem->file_seek(file->filesystem_file_handle, offset, mode);
}

/*
// writes data to a file.
*/
uint16_t sm_file_write(SM_FILE* file, unsigned char* buffer, uint32_t length)
{
	/*
	// check that we got a valid file handle
	*/
	if (!file)
		return SM_INVALID_FILE_HANDLE;
	if (file->magic != SM_FILE_HANDLE_MAGIC)
		return SM_INVALID_FILE_HANDLE;
	/*
	// call on the filesystem to perform the operation
	*/
	return file->filesystem->file_write(file->filesystem_file_handle, buffer, length);
}

/*
// writes data to a file asynchronously
*/
uint16_t sm_file_write_async
(
	SM_FILE* file, 
	unsigned char* buffer, 
	uint32_t length, 
	uint16_t* result, 
	SM_ASYNC_CALLBACK callback, 
	void* callback_context
)
{
	/*
	// check that we got a valid file handle
	*/
	if (!file)
		return SM_INVALID_FILE_HANDLE;
	if (file->magic != SM_FILE_HANDLE_MAGIC)
		return SM_INVALID_FILE_HANDLE;
	/*
	// call on the filesystem to perform the operation
	*/
	return file->filesystem->file_write_async(file->filesystem_file_handle, buffer, length, result, (FILESYSTEM_ASYNC_CALLBACK) callback, callback_context);
}

uint16_t sm_file_write_stream
(
	SM_FILE* file, 
	unsigned char* buffer, 
	uint32_t length, 
	uint16_t* result, 
	SM_STREAM_CALLBACK callback, 
	void* callback_context
)
{
	/*
	// check that we got a valid file handle
	*/
	if (!file)
		return SM_INVALID_FILE_HANDLE;
	if (file->magic != SM_FILE_HANDLE_MAGIC)
		return SM_INVALID_FILE_HANDLE;
	/*
	// call on the filesystem to perform the operation
	*/
	return file->filesystem->file_write_stream(file->filesystem_file_handle, buffer, length, result, (SM_STREAM_CALLBACK) callback, callback_context);
}


/*
// reads data from a file
*/
uint16_t sm_file_read(SM_FILE* file, unsigned char* buffer, uint32_t length, uint32_t* bytes_read)
{
	/*
	// check that we got a valid file handle
	*/
	if (!file)
		return SM_INVALID_FILE_HANDLE;
	if (file->magic != SM_FILE_HANDLE_MAGIC)
		return SM_INVALID_FILE_HANDLE;
	/*
	// call on the filesystem to perform the operation
	*/
	return file->filesystem->file_read(file->filesystem_file_handle, buffer, length, bytes_read);
}

/*
// reads data from a file asynchronously
*/
uint16_t sm_file_read_async
(
	SM_FILE* file, 
	unsigned char* buffer, 
	uint32_t length, 
	uint32_t* bytes_read, 
	uint16_t* result, 
	SM_ASYNC_CALLBACK callback, 
	void* callback_context
)
{
	/*
	// check that we got a valid file handle
	*/
	if (!file)
		return SM_INVALID_FILE_HANDLE;
	if (file->magic != SM_FILE_HANDLE_MAGIC)
		return SM_INVALID_FILE_HANDLE;
	/*
	// call on the filesystem to perform the operation
	*/
	return file->filesystem->file_read_async(
		file->filesystem_file_handle, buffer, length, bytes_read, result, (FILESYSTEM_ASYNC_CALLBACK) callback, callback_context);
}

/*
// flushes file buffers
*/
uint16_t sm_file_flush(SM_FILE* file)
{
	uint16_t ret;
	/*
	// check that we got a valid file handle
	*/
	if (!file)
		return SM_INVALID_FILE_HANDLE;
	if (file->magic != SM_FILE_HANDLE_MAGIC)
		return SM_INVALID_FILE_HANDLE;
	/*
	// call on the filesystem to perform the operation
	*/
	ret = file->filesystem->file_close(file->filesystem_file_handle);
	if (ret != FILESYSTEM_SUCCESS)
		return ret;
	/*
	// return success;
	*/
	return SM_SUCCESS;
}

/*
// closes a file
*/
uint16_t sm_file_close(SM_FILE* file)
{
	uint16_t ret;
	uint32_t file_id;
	SM_OPEN_FILE** open_file;
	/*
	// check that we got a valid file handle
	*/
	if (!file)
		return SM_INVALID_FILE_HANDLE;
	if (file->magic != SM_FILE_HANDLE_MAGIC)
		return SM_INVALID_FILE_HANDLE;
	/*
	// get the file id
	*/
	file_id = file->filesystem->file_get_unique_id(file->filesystem_file_handle);
	/*
	// call on the filesystem to perform the operation
	*/
	ret = file->filesystem->file_close(file->filesystem_file_handle);
	if (ret != FILESYSTEM_SUCCESS)
		return ret;
	/*
	// free the filesystem handle
	*/
	free(file->filesystem_file_handle);
	file->magic = 0;
	/*
	// if the file has a managed buffer free it
	*/
	if (file->managed_buffer)
	{
		free(file->managed_buffer);
		file->managed_buffer = 0;
	}
	/*
	// remove the file from the locked files list
	*/
	#if defined(SM_SUPPORT_FILE_LOCKS)
	_ASSERT(file_id);
	/*
	// try to find the file in the locked files list
	*/
	open_file = &open_files;
	while ((*open_file))
	{
		if ((*open_file)->id == file_id)
		{
			break;
		}
		open_file = &(*open_file)->next;
	}
	/*
	// if the file was locked removed from the locked files list
	*/
	if ((*open_file))
	{
		SM_OPEN_FILE* the_file = (*open_file);
		(*open_file) = (*open_file)->next;
		free(the_file);
	}
	#endif
	/*
	// return success;
	*/
	return SM_SUCCESS;
}

/*
// gets the directory entry for a file.
*/
uint16_t sm_get_file_entry(char* filename, SM_DIRECTORY_ENTRY* entry)
{
	uint16_t ret;
	SM_VOLUME* volume;
	char* volume_path;
	void* dir_entry;
	FILESYSTEM* fs;
	/*
	// resolve path
	*/
	ret = sm_resolve_path(filename, &volume, &volume_path);
	if (ret != SM_SUCCESS)
		return ret;
	/*
	// allocate memory for filesystem entry
	*/
	dir_entry = malloc(volume->filesystem->dir_entry_size);
	if (!dir_entry)
		return SM_INSUFFICIENT_MEMORY;
	/*
	// filesystem pointer
	*/
	fs = volume->filesystem;
	/*
	// get the entry from filesystem driver
	*/
	ret = fs->get_file_entry(volume->volume, volume_path, dir_entry);
	if (ret != FILESYSTEM_SUCCESS)
	{
		free(dir_entry);
		return ret;
	}
	/*
	// copy the directory entry
	*/
	memcpy(entry->name, (void*)((uintptr_t) dir_entry + fs->dir_entry_name_offset), fs->dir_entry_name_size);
	entry->size = *((uint32_t*) ((uintptr_t) dir_entry + fs->dir_entry_size_offset));
	entry->create_time = *((time_t*) ((uintptr_t) dir_entry + fs->dir_entry_create_time_offset));
	entry->modify_time = *((time_t*) ((uintptr_t) dir_entry + fs->dir_entry_modify_time_offset));
	entry->access_time = *((time_t*) ((uintptr_t) dir_entry + fs->dir_entry_access_time_offset));
	entry->attributes = *((unsigned char*) ((uintptr_t) dir_entry + fs->dir_entry_attributes_offset));
	/*
	// free the filesystem entry
	*/
	free(dir_entry);
	/*
	// return success code
	*/
	return SM_SUCCESS;

}

/*
// initializes a directory query and finds the 1st entry
*/
uint16_t sm_find_first_entry(char* path, unsigned char attributes, SM_DIRECTORY_ENTRY* entry, SM_QUERY* query)
{
	uint16_t ret;
	SM_VOLUME* volume;
	char* volume_path;
	void* dir_entry;
	FILESYSTEM* fs;
	/*
	// resolve path
	*/
	ret = sm_resolve_path(path, &volume, &volume_path);
	if (ret != SM_SUCCESS)
		return ret;
	/*
	// get handle to filesystem
	*/
	fs = volume->filesystem;
	/*
	// initialize query
	*/
	query->volume = (void*) volume;
	query->query_handle = malloc(fs->query_handle_size);
	if (!query->query_handle)
		return SM_INSUFFICIENT_MEMORY;
	/*
	// zero-out the filesystem query handle
	*/
	memset(query->query_handle, 0, fs->query_handle_size);
	/*
	// call on the filesystem to perform op
	*/
	ret = fs->find_first_entry(volume->volume, volume_path, attributes, &dir_entry, query->query_handle);
	if (ret != FILESYSTEM_SUCCESS)
		return ret;
	/*
	// copy the directory entry
	*/
	memcpy(entry->name, (void*)((uintptr_t) dir_entry + fs->dir_entry_name_offset), fs->dir_entry_name_size);
	entry->size = *((uint32_t*) ((uintptr_t) dir_entry + fs->dir_entry_size_offset));
	entry->create_time = *((time_t*) ((uintptr_t) dir_entry + fs->dir_entry_create_time_offset));
	entry->modify_time = *((time_t*) ((uintptr_t) dir_entry + fs->dir_entry_modify_time_offset));
	entry->access_time = *((time_t*) ((uintptr_t) dir_entry + fs->dir_entry_access_time_offset));
	entry->attributes = *((unsigned char*) ((uintptr_t) dir_entry + fs->dir_entry_attributes_offset));
	/*
	// return success
	*/
	return SM_SUCCESS;
}

/*
// finds the next entry in a directory query.
*/
uint16_t sm_find_next_entry(SM_DIRECTORY_ENTRY* entry, SM_QUERY* query)
{
	uint16_t ret;
	void* dir_entry;
	FILESYSTEM* fs;
	/*
	// get a handle to the filesystem that's processing the query
	*/
	fs = ((SM_VOLUME*) query->volume)->filesystem;
	/*
	// call on filesystem to perform op
	*/
	ret = fs->find_next_entry(((SM_VOLUME*) query->volume)->volume, &dir_entry, query->query_handle);
	if (ret != FILESYSTEM_SUCCESS)
		return ret;
	/*
	// copy the directory entry
	*/
	memcpy(entry->name, (void*)((uintptr_t) dir_entry + fs->dir_entry_name_offset), fs->dir_entry_name_size);
	entry->size = *((uint32_t*) ((uintptr_t) dir_entry + fs->dir_entry_size_offset));
	entry->create_time = *((time_t*) ((uintptr_t) dir_entry + fs->dir_entry_create_time_offset));
	entry->modify_time = *((time_t*) ((uintptr_t) dir_entry + fs->dir_entry_modify_time_offset));
	entry->access_time = *((time_t*) ((uintptr_t) dir_entry + fs->dir_entry_access_time_offset));
	entry->attributes = *((unsigned char*) ((uintptr_t) dir_entry + fs->dir_entry_attributes_offset));
	/*
	// return success
	*/
	return SM_SUCCESS;
}

/*
// closes a directory query
*/
uint16_t sm_find_close(SM_QUERY* query)
{
	if (query->query_handle)
		free(query->query_handle);
	return SM_SUCCESS;
}

/*
// if a storage device interface has been associated with a drive letter
// the storage device driver will call this function when the media
// changes (ie. an SD card is inserted or removed).
*/
static void sm_media_changed_callback(uint16_t device_id, char media_ready)
{
	SM_DEVICE** device = &registered_devices;

	while ((*device))
	{
		if ((*device)->id == device_id)
		{
			sm_dismount_volume((*device)->label);

			if (media_ready)
			{
				sm_mount_volume((*device)->label, &(*device)->device);
			}
			break;
		}
		device = &((*device)->next);
	}
}

/*
// resolves a path (ie. gets the index of the volume that contains the file
// on the mounted_volumes array and the path of the file on the volume).
*/
static uint16_t sm_resolve_path(char* path, SM_VOLUME** vol, char** volume_path)
{
	int i;
	size_t path_len;
	char label_part[SM_MAX_DRIVE_LABEL_LENGTH + 1];
	SM_VOLUME** volume;
	/*
	// if the path begins with a separator skip it
	*/
	if (*path == SM_PATH_SEPARATOR)
		path++;
	/*
	// find the location of the 1st path separator
	*/
	path_len = strlen(path);
	for (i = 0; (unsigned int) i < path_len; i++)
	{
		if (path[i] == SM_PATH_SEPARATOR)
			break;
	}
	/*
	// check that this is a valid path so far
	*/
	if (i == 0 /*|| i == path_len - 1*/)
		return SM_INVALID_PATH;
	if (i >= SM_MAX_DRIVE_LABEL_LENGTH)
		return SM_INVALID_DRIVE_LABEL;
	/*
	// copy the drive label part to our temp buffer and set
	// *volume_path to point to the beginning of the path part
	*/
	memcpy(label_part, path, i);
	label_part[i] = 0;
	*volume_path = path + i;
	/*
	// set volume to thhe 1st volume on the list
	*/
	volume = &mounted_volumes;
	/*
	// find the volume that matches the path part
	*/
	while ((*volume))
	{
		if (!strcmp(label_part, (*volume)->drive_label))
			break;
		volume = &((*volume)->next);
	}
	/*
	// if no volume was found return error
	*/
	if (!(*volume))
		return SM_INVALID_DRIVE_LABEL;
	/*
	// set the volume pointer
	*/
	*vol = *volume;
	/*
	// return success.
	*/
	return SM_SUCCESS;
}
