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

#ifndef __STORAGE_DEVICE_H__
#define __STORAGE_DEVICE_H__

/*! \file storage_device.h
 * \brief This file defines the interface between this module
 * and the underlying storage device driver.
 */

#include <compiler.h>


/*
// return codes
*/
#define STORAGE_SUCCESS							0x0
#define STORAGE_VOLTAGE_NOT_SUPPORTED			0x0001
#define STORAGE_INVALID_MEDIA					0x0002
#define STORAGE_UNKNOWN_ERROR					0x0003
#define STORAGE_INVALID_PARAMETER				0x0004
#define STORAGE_ADDRESS_ERROR					0x0005
#define STORAGE_ERASE_SEQ_ERROR					0x0006
#define STORAGE_CRC_ERROR						0x0007
#define STORAGE_ILLEGAL_COMMAND					0x0008
#define STORAGE_ERASE_RESET						0x0009
#define STORAGE_COMMUNICATION_ERROR				0x000A
#define STORAGE_TIMEOUT							0x000B
#define STORAGE_IDLE							0x000C
#define STORAGE_OUT_OF_RANGE					0x000D
#define STORAGE_MEDIA_ECC_FAILED				0x000E
#define STORAGE_CC_ERROR						0x000F
#define STORAGE_OP_IN_PROGRESS					0x0010
#define STORAGE_MEDIUM_WRITE_PROTECTED			0x0011
#define STORAGE_OUT_OF_SPACE					0x0012
#define STORAGE_DEVICE_NOT_READY				0x0013
#define STORAGE_AWAITING_DATA					0x0014
#define STORAGE_INVALID_MULTI_BLOCK_RESPONSE	0x0015

#define STORAGE_MULTI_SECTOR_RESPONSE_STOP		0x0
#define STORAGE_MULTI_SECTOR_RESPONSE_SKIP		0x1
#define STORAGE_MULTI_SECTOR_RESPONSE_READY		0x2

/*
// interface definitions...
*/
typedef uint16_t (*STORAGE_DEVICE_GET_SECTOR_SIZE)( void* device );
typedef uint32_t (*STORAGE_DEVICE_GET_SECTOR_COUNT)( void* device );
typedef void (*STORAGE_CALLBACK)(void* context, uint16_t* state);
typedef void (*STORAGE_CALLBACK_EX)(void* context, uint16_t* state, unsigned char** buffer, uint16_t* action);

typedef uint16_t (*STORAGE_GET_DEVICE_ID)(void* device);
typedef uint32_t (*STORAGE_GET_PAGE_SIZE)(void* device);
typedef void (*STORAGE_MEDIA_CHANGED_CALLBACK)(uint16_t device_id, char media_state);
typedef void (*STORAGE_REGISTER_MEDIA_CHANGED_CALLBACK)(void* device, STORAGE_MEDIA_CHANGED_CALLBACK callback);

/*
// structure used to represent an asynchronous
// operation state
*/
typedef struct _STORAGE_CALLBACK_INFO 
{
	STORAGE_CALLBACK Callback;
	void* Context;
}
STORAGE_CALLBACK_INFO, *PSTORAGE_CALLBACK_INFO;	

/*
// structure used to represent an asynchronous
// multi-sector operation.
*/
typedef struct _STORAGE_CALLBACK_INFO_EX
{
	STORAGE_CALLBACK_EX Callback;
	void* Context;
}
STORAGE_CALLBACK_INFO_EX, *PSTORAGE_CALLBACK_INFO_EX;	

/*
// storage device read sector function
*/
typedef uint16_t (*STORAGE_DEVICE_READ)(void* device, uint32_t sector_address, unsigned char* buffer);
typedef uint16_t (*STORAGE_DEVICE_READ_ASYNC)(void* device, uint32_t sector_address, 
	unsigned char* buffer, uint16_t* async_state, PSTORAGE_CALLBACK_INFO callback_info);

typedef uint16_t (*STORAGE_DEVICE_ERASE_SECTORS)(void* device, uint32_t start_sector, uint32_t end_sector);

/*
 * storage device write sector function
 */
typedef uint16_t (*STORAGE_DEVICE_WRITE)(void* device, uint32_t sector_address, unsigned char* buffer);
typedef uint16_t (*STORAGE_DEVICE_WRITE_ASYNC)(void* device, uint32_t sector_address, 
	unsigned char* buffer, uint16_t* async_state, PSTORAGE_CALLBACK_INFO callback_info);
typedef uint16_t (*STORAGE_DEVICE_WRITE_MULTIPLE_SECTORS)(void* device, uint32_t sector_address, 
	unsigned char* buffer, uint16_t* async_state, PSTORAGE_CALLBACK_INFO_EX callback_info);
/*!
 * <summary>
 * Contains the interface between fat32lib and the
 * underlying storage device driver. 
 * </summary>
 */
typedef struct _STORAGE_DEVICE 
{
	void* Media;
	STORAGE_DEVICE_READ ReadSector;
	STORAGE_DEVICE_READ_ASYNC ReadSectorAsync;
	STORAGE_DEVICE_WRITE WriteSector;
	STORAGE_DEVICE_WRITE_ASYNC WriteSectorAsync;
	STORAGE_DEVICE_GET_SECTOR_SIZE GetSectorSize;
	STORAGE_DEVICE_GET_SECTOR_COUNT GetTotalSectors;
	STORAGE_REGISTER_MEDIA_CHANGED_CALLBACK register_media_changed_callback;
	STORAGE_GET_DEVICE_ID get_device_id;
	STORAGE_GET_PAGE_SIZE get_page_size;
	STORAGE_DEVICE_WRITE_MULTIPLE_SECTORS write_multiple_sectors;
	STORAGE_DEVICE_ERASE_SECTORS erase_sectors;
}	
STORAGE_DEVICE, *PSTORAGE_DEVICE;

#endif
