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

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include "win32io.h"

typedef struct 
{
	void* physical_drive;
	uint32_t bytes_per_sector;
	uint32_t total_sectors;
}
WIN32IO_DEVICE;

//
// struct for passing arameters of async calls to new thread
//
typedef struct 
{
	void* device;
	uint32_t sector_address;
	unsigned char* buffer;
	uint16_t* async_state;
	PSTORAGE_CALLBACK_INFO callback_info;
	char write;
}
WIN32IO_ASYNC_PARAMS;

HANDLE h;
HANDLE hAsyncEvent;
HANDLE hAsyncThread;
DWORD last_sector = 0;
WIN32IO_ASYNC_PARAMS async_args;
char runAsync = 1;

typedef struct WIN32IO_MULTI_BLOCK_CONTEXT
{
	void* device;
	uint32_t sector_address;
	unsigned char* buffer;
	uint16_t* async_state;
	STORAGE_CALLBACK_INFO_EX callback_info;
	STORAGE_CALLBACK_INFO cinfo;

}
WIN32IO_MULTI_BLOCK_CONTEXT;

static void win32io_write_multiple_blocks_callback(WIN32IO_MULTI_BLOCK_CONTEXT* context, uint16_t* result);
static uint16_t win32io_write_multiple_blocks(
	void* device, uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, STORAGE_CALLBACK_INFO_EX* callback_info);

//
// STORAGE_DEVICE interface functions
//
static uint16_t win32io_read_sector(void* device, uint32_t sector_address, unsigned char* buffer);
static uint16_t win32io_read_sector_async(void* device, uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, PSTORAGE_CALLBACK_INFO callback_info);
static uint16_t win32io_write_sector(void* device, uint32_t sector_address, unsigned char* buffer);
static uint16_t win32io_write_sector_async(void* device, uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, PSTORAGE_CALLBACK_INFO callback_info);
static uint16_t win32io_get_sector_size(void* device);
static uint32_t win32io_get_sector_count(void* device);

//
// async call worker
//
static void win32io_async_worker();

//
// gets the STORAGE_DEVICE interface used by the filesystem
//
void win32io_get_storage_device(TCHAR* physical_drive, STORAGE_DEVICE* device)
{
	DWORD ioctl_result;
	WIN32IO_DEVICE* dev;
	DISK_GEOMETRY geo;
	DWORD bytes_returned;

	dev = malloc(sizeof(WIN32IO_DEVICE));
	dev->physical_drive = physical_drive;
	
	device->Media = dev;
	device->ReadSector = (STORAGE_DEVICE_READ) &win32io_read_sector;
	device->WriteSector = (STORAGE_DEVICE_WRITE) &win32io_write_sector;
	device->GetSectorSize = (STORAGE_DEVICE_GET_SECTOR_SIZE) &win32io_get_sector_size;
	device->ReadSectorAsync = (STORAGE_DEVICE_READ_ASYNC) &win32io_read_sector_async;
	device->WriteSectorAsync = (STORAGE_DEVICE_WRITE_ASYNC) &win32io_write_sector_async;
	device->GetTotalSectors = (STORAGE_DEVICE_GET_SECTOR_COUNT) &win32io_get_sector_count;
	device->write_multiple_sectors = (STORAGE_DEVICE_WRITE_MULTIPLE_SECTORS) &win32io_write_multiple_blocks;

	h = CreateFile((TCHAR*) physical_drive, GENERIC_READ | GENERIC_WRITE, 
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	DeviceIoControl(
	  h,              // handle to device
	  IOCTL_DISK_GET_DRIVE_GEOMETRY, // dwIoControlCode
	  NULL,                          // lpInBuffer
	  0,                             // nInBufferSize
	  (LPVOID) &geo,          // output buffer
	  (DWORD) sizeof(DISK_GEOMETRY),        // size of output buffer
	  (LPDWORD) &bytes_returned,     // number of bytes returned
	  NULL
	);
	//CloseHandle(h);
	dev->bytes_per_sector = geo.BytesPerSector;
	dev->total_sectors = geo.SectorsPerTrack * geo.TracksPerCylinder * geo.Cylinders.LowPart;
	if (!DeviceIoControl(h, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &ioctl_result, NULL))
	{
		printf("win32io: could not lock drive.\r");
	}
	if (!DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &ioctl_result, NULL))
	{
		printf("Could not dismount volume.\r");
	}
	if (!DeviceIoControl(
	  (HANDLE) h,              // handle to device
	  FSCTL_ALLOW_EXTENDED_DASD_IO,  // dwIoControlCode
	  NULL,                          // lpInBuffer
	  0,                             // nInBufferSize
	  NULL,                          // lpOutBuffer
	  0,                             // nOutBufferSize
	  &ioctl_result,     // number of bytes returned
	  NULL    // OVERLAPPED structure
	))
	{
		printf("FSCTL_ALL_EXTENDED_DASD_IO failed.\r");

	}

	runAsync = 1;
	hAsyncEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	hAsyncThread = CreateThread(
						NULL, 
						0, 
						(LPTHREAD_START_ROUTINE) &win32io_async_worker, 
						(LPVOID) NULL, 
						(DWORD) NULL, 
						(LPDWORD) NULL);
}

void win32io_release_storage_device()
{
	DWORD ioctl_result;
	DeviceIoControl(h, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &ioctl_result, NULL);
	//DeviceIoControl(h, FSCTL_MOUNT_VOLUME, NULL, 0, NULL, 0, &ioctl_result, NULL);
	CloseHandle(h);

	runAsync = 0;
	SetEvent(hAsyncEvent);
	CloseHandle(hAsyncThread);
}

//
// reads a sector from the storage device using Win32 raw I/O
//
static uint16_t win32io_read_sector(void* device, uint32_t sector_address, unsigned char* buffer)
{
	DWORD bytes_read;
	WIN32IO_DEVICE* dev = device;
	DWORD sector = sector_address * win32io_get_sector_size(device);
	DWORD bytes_to_read = win32io_get_sector_size(device);

	if (sector != (last_sector + win32io_get_sector_size(device)))
	{
		SetFilePointer(h, sector, NULL, FILE_BEGIN);
	}
	last_sector = sector;

	if (!ReadFile(h, buffer, bytes_to_read, &bytes_read, NULL))
		return STORAGE_COMMUNICATION_ERROR;

	if (bytes_read < win32io_get_sector_size(device))
		return STORAGE_COMMUNICATION_ERROR;
	return STORAGE_SUCCESS;
}

//
// writes a sector to the storage device using Win32 I/O
//
static uint16_t win32io_write_sector(void* device, uint32_t sector_address, unsigned char* buffer)
{
	DWORD bytes_written = 0;
	DWORD ioctl_result;
	WIN32IO_DEVICE* dev = device;
	DWORD sector = sector_address * win32io_get_sector_size(device);

	if (sector != (last_sector + win32io_get_sector_size(device)))
	{
		SetFilePointer(h, sector, NULL, FILE_BEGIN);
	}
	last_sector = sector;
	
	WriteFile(h, buffer, win32io_get_sector_size(device), &bytes_written, NULL);

	if (bytes_written < win32io_get_sector_size(device))
	{
		printf("win32io: Write operation to sector %x failed!\n", sector_address);
		return STORAGE_COMMUNICATION_ERROR;
	}

	return STORAGE_SUCCESS;
}

//
// fires a new thread to call win32io_read_sector.
// this emulates the hardware driver asynchronous IO support.
//
static uint16_t win32io_read_sector_async(
	void* device, uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, PSTORAGE_CALLBACK_INFO callback_info)
{
	async_args.device = device;
	async_args.sector_address = sector_address; 
	async_args.buffer = buffer;
	async_args.async_state = async_state;
	async_args.callback_info = callback_info;
	async_args.write = 0;
	/*
	// start async op
	*/
	SetEvent(hAsyncEvent);

	return STORAGE_OP_IN_PROGRESS;
}

//
// fires a new thread to call win32io_read_sector.
// this emulates the hardware driver asynchronous IO support.
//
static uint16_t win32io_write_sector_async(
	void* device, uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, PSTORAGE_CALLBACK_INFO callback_info)
{
	async_args.device = device;
	async_args.sector_address = sector_address;
	async_args.buffer = buffer;
	async_args.async_state = async_state;
	async_args.callback_info = callback_info;
	async_args.write = 1;
	//
	// start async op
	//
	SetEvent(hAsyncEvent);

	return STORAGE_OP_IN_PROGRESS;
}

/*
// emulates the write_multiple_blocks function of the SD driver.
// In windows it won't make a difference in performance, it simply
// emulates the interface of the SD driver to allows us to test our
// code in windows.
*/
static uint16_t win32io_write_multiple_blocks(
	void* device, uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, STORAGE_CALLBACK_INFO_EX* callback_info)
{
	WIN32IO_MULTI_BLOCK_CONTEXT* context = GlobalAlloc(GPTR, sizeof(WIN32IO_MULTI_BLOCK_CONTEXT));

	context->device = device;
	context->sector_address = sector_address + 1;
	context->buffer = buffer;
	context->async_state = async_state;
	context->callback_info = *callback_info;


	context->cinfo.Callback = &win32io_write_multiple_blocks_callback;
	context->cinfo.Context = context;

	return win32io_write_sector_async(device, sector_address, buffer, async_state, &context->cinfo);




}

static void win32io_write_multiple_blocks_callback(WIN32IO_MULTI_BLOCK_CONTEXT* context, uint16_t* result)
{
	uint16_t response;
	
do_it_again:
	response = STORAGE_MULTI_SECTOR_RESPONSE_STOP;
	*context->async_state = STORAGE_AWAITING_DATA;

	if (context->callback_info.Callback)
		context->callback_info.Callback(context->callback_info.Context, context->async_state, &(context->buffer), &response);

	switch  (response)
	{
		case STORAGE_MULTI_SECTOR_RESPONSE_READY:
			/*
			// this is being called from the working thread so in effect
			// the thread is signaling itself therefore with this driver we
			// must be careful not to start multiple async request as it may
			// mess things up since we're not queueing requests.
			*/
			win32io_write_sector_async(context->device, context->sector_address++, context->buffer, context->async_state, &context->cinfo);

			break;
		case STORAGE_MULTI_SECTOR_RESPONSE_SKIP:
			Sleep(500);
			goto do_it_again;
		case STORAGE_MULTI_SECTOR_RESPONSE_STOP:
		default:
			*context->async_state = STORAGE_SUCCESS;
			/*
			// call back after stopping
			*/
			if (context->callback_info.Callback)
				context->callback_info.Callback(context->callback_info.Context, context->async_state, &(context->buffer), &response);

			GlobalFree(context);
			break;
	}
}

	
	
//
// gets the sector size of the storage device
//
static uint16_t win32io_get_sector_size(void* device)
{

	WIN32IO_DEVICE* dev = device;
	return dev->bytes_per_sector;
}

/*
// gets the sector count of the storage device
*/
static uint32_t win32io_get_sector_count(void* device)
{
	WIN32IO_DEVICE* dev = device;
	return dev->total_sectors;
}

/*
// asynchronous IO worker thread
*/
static void win32io_async_worker()
{
	DWORD bytes_written;

	WaitForSingleObject(hAsyncEvent, INFINITE);

	while (runAsync)
	{
		if (async_args.write)
		{
			*async_args.async_state = win32io_write_sector(async_args.device, async_args.sector_address, async_args.buffer);
		}
		else
		{
			*async_args.async_state = win32io_read_sector(async_args.device, async_args.sector_address, async_args.buffer);
			
		}
		if (async_args.callback_info->Callback)
			async_args.callback_info->Callback(async_args.callback_info->Context, async_args.async_state);

		WaitForSingleObject(hAsyncEvent, INFINITE);
	}
}
