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
 * and the underlying storage device driver. All storage device
 * driver must implement all the functions defined in this file in
 * order to fully support the File32Lib File System Stack.
 */

#include "../compiler/compiler.h"


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

/*!
 * <summary>
 * This is the media state changed callback function. It is called by the
 * device driver in order to notify the volume manager whenever the device is 
 * mounted or dismounted.
 * </summary>
 * <param name="device_id">A 16-bit unsigned integer that uniquely identifies the device.</param>
 * <param name="media_state">
 * A boolean value that is set to 1 to indicate that the device has been mounted and to 0
 * to indicate that it has been dismounted.
 * </param>
 */
typedef void (*STORAGE_MEDIA_CHANGED_CALLBACK)(uint16_t device_id, char media_state);

/*!
 * <summary>
 * This is the function used to register a callback function to receive notifications
 * when the device is mounted or dismounted. It is called by the volume manager driver to
 * register a STORAGE_MEDIA_CHANGED_CALLBACK function.
 * </summary>
 * <param name="device">A pointer to the device driver handle.</param>
 * <param name="callback">A pointer to the function that will be called when the device is mounted or dismounted.</param>
 */
typedef void (*STORAGE_REGISTER_MEDIA_CHANGED_CALLBACK)(void* device, STORAGE_MEDIA_CHANGED_CALLBACK callback);

/*!
 * <summary>
 * This is the callback function for an asynchronous operation.
 * </summary>
 * <param name="context">The context pointer of the asynchronous operation.</param>
 * <param name="result">A pointer to the result of the asynchronous operation.</param>
 */
typedef void (*STORAGE_CALLBACK)(void* context, uint16_t* result);

/*!
 * <summary>
 * This is the callback function for multi-sector operations.
 * </summary>
 * <param name="context">The context of the multi-sector operation.</param>
 * <param name="result">A pointer to the result or current status of the multi-sector operation.</param>
 * <param name="buffer">
 * A pointer to the multi-sector operation buffer.
 * </param>
 * <param name="response">
 * A pointer that the file system driver must set in order to signal how the
 * storage device driver should proceed with the asynchronous operation.
 * </param>
 * <seealso cref="STORAGE_DEVICE_WRITE_MULTIPLE_SECTORS" />
 */
typedef void (*STORAGE_CALLBACK_EX)(void* context, uint16_t* result, unsigned char** buffer, uint16_t* response);

/*!
 * <summary>
 * This structure holds the callback function pointer and 
 * the callback context and is passed by the file system driver
 * as a parameter to the driver's asynchronous IO functions.
 * </summary>
 */
typedef struct _STORAGE_CALLBACK_INFO 
{
	/*!
	 * <summary>The callback function for an asynchronous IO function.</summary>
	 */
	STORAGE_CALLBACK Callback;
	/*!
	 * <summary>The callback context for an asynchronous IO function.</summary>
	 */
	void* Context;
}
STORAGE_CALLBACK_INFO, *PSTORAGE_CALLBACK_INFO;	

/*!
 * <summary>
 * This structure holds the callback function pointer and
 * the callback context and is passed by the file system driver
 * as a parameter to the driver's multiple sector functions.
 * </summary>
 */
typedef struct _STORAGE_CALLBACK_INFO_EX
{
	/*!
	 * <summary>The callback function for a multiple sector function.</summary>
	 */
	STORAGE_CALLBACK_EX Callback;
	/*!
	 * <summary>The callback context pointer for a multiple sector function.</summary>
	 */
	void* Context;
}
STORAGE_CALLBACK_INFO_EX, *PSTORAGE_CALLBACK_INFO_EX;	

/*!
 * <summary>
 * This is the function pointer to the driver function that gets the sector size.
 * </summary>
 * <param name="device">A pointer to the device driver handle.</param>
 * <returns>
 * A 16-bit unsigned integer that represents the sector size usually ()though not 
 * necessarily always) 512 bytes.
 * </returns>
 */
typedef uint16_t (*STORAGE_DEVICE_GET_SECTOR_SIZE)(void* device);

/*!
 * <summary>
 * This is the function pointer to the driver function that gets the number of
 * sectors on the storage device.
 * </summary>
 * <param name="device">A pointer to the device driver handle.</param>
 * <returns>
 * A 32-bit unsigned integer that represents the number of sectors on the storage device.
 * </returns>
 */
typedef uint32_t (*STORAGE_DEVICE_GET_SECTOR_COUNT)(void* device);

/*!
 * <summary>
 * This is the function pointer to the driver function that gets the device id.
 * </summary>
 * <param name="device">A pointer to the device driver handle.</param>
 * <returns>
 * A 16-bit unsigned integer that uniquely identifies the device.
 * </summary>
 */
typedef uint16_t (*STORAGE_GET_DEVICE_ID)(void* device);

/*!
 * <summary>
 * This is the function pointer to the driver function that gets the size of flash pages.
 * This function should return 1 for non-flash devices.
 * </summary>
 * <param name="device">A pointer to the device driver handle.</param>
 * <returns>
 * A 32-bit unsigned integer that represents the size of the flash page in sectors.
 * </returns>
 */
typedef uint32_t (*STORAGE_GET_PAGE_SIZE)(void* device);

/*!
 * <summary>
 * A function pointer to the driver function used to read a sector.
 * </summary>
 * <param name="device">A pointer to the device driver handle.</param>
 * <param name="sector_address">A 32-bit unsigned integer representing the address of the sector to be read</param>
 * <param name="buffer">A sector sized buffer where the sector data will be copied to.</param>
 * <returns>One of the result codes defined in storage_device.h.</returns>
 */
typedef uint16_t (*STORAGE_DEVICE_READ)(void* device, uint32_t sector_address, unsigned char* buffer);

/*!
 * <summary>
 * A function pointer to the driver function used to read a sector asynchronously.
 * </summary>
 * <param name="device">A pointer to the device driver handle.</param>
 * <param name="sector_address">A 32-bit unsigned integer representing the address of the sector to be read.</param>
 * <param name="buffer">A sector-sized buffer where the sector data will be copied to.</param>
 * <param name="result">
 * A pointer to a 16-bit unsigned integer where the result of the asynchronous operation will be stored.
 * </param>
 * <param name="callback_info">
 * A pointer to a STORAGE_CALLBACK_INFO structure that holds the callback function pointer
 * and a context pointer that will be passed back to the callback function.
 * </param>
 * <returns>
 * If successful it should return STORAGE_OP_IN_PROGRESS, otherwise it should
 * return one of the result codes defined in storage_device.h
 * </returns> 
 */
typedef uint16_t (*STORAGE_DEVICE_READ_ASYNC)(void* device, uint32_t sector_address, 
			unsigned char* buffer, uint16_t* result, STORAGE_CALLBACK_INFO* callback_info);

/*!
 * <summary>
 * A function pointer to the driver function used to erase flash pages.
 * For non-flash devices this function should always return STORAGE_SUCCESS without doing
 * any work.
 * </summary>
 * <param name="device">A pointer to the device driver handle.</param>
 * <param name="start_sector_address">A 32-bit unsigned integer representing the address of the 1st sector to erase.</param>
 * <param name="end_sector_address">A 32-bit unsigned integer representing the address of the last sector to erase.</param>
 * <returns>One of the result codes defined in storage_device.h</returns>
 */
typedef uint16_t (*STORAGE_DEVICE_ERASE_SECTORS)(void* device, uint32_t start_sector_address, uint32_t end_sector_address);

/*!
 * <summary>
 * A function pointer to the driver function used to write a sector to the device.
 * </summary>
 * <param name="device">A pointer to the device driver handle.</param>
 * <param name="sector_address">A 32-bit unsigned integer representing the address of the sector to be written.</param>
 * <param name="buffer">A sector-sized buffer containing the data to be written.</param>
 * <returns>One of the result codes defined in storage_device.h</returns>
 */
typedef uint16_t (*STORAGE_DEVICE_WRITE)(void* device, uint32_t sector_address, unsigned char* buffer);

/*!
 * <summary>
 * A function pointer to the driver function used to write a sector to the device asynchronously.
 * </summary>
 * <param name="device">A pointer to the device driver handle.</param>
 * <param name="sector_address">A 32-bit unsigned integer representing the address of the sector to be written.</param>
 * <param name="buffer">A sector-sized buffer containing the data to be written.</param>
 * <param name="result">
 * A pointer to a 16-bit integer where the result of the asynchronous operation
 * will be written to.
 * </param>
 * <param name="callback_info">
 * A pointer to a STORAGE_CALLBACK_INFO structure that holds the callback function pointer
 * and a context pointer that will be passed back to the callback function.
 * </param>
 * <returns>
 * If successful it should return STORAGE_OP_IN_PROGRESS, otherwise it should
 * return one of the result codes defined in storage_device.h
 * </returns> 
 */
typedef uint16_t (*STORAGE_DEVICE_WRITE_ASYNC)(void* device, uint32_t sector_address, 
			unsigned char* buffer, uint16_t* result, PSTORAGE_CALLBACK_INFO callback_info);
	
/*!
 * <summary>
 * A function pointer to the driver function used to write multiple sectors to the device asynchronously.
 * This function actually writes one sector at a time but calls back as soon as the sector is written (or while
 * it is still programming in the case of flash memory) to reload the data and continue writing. This
 * allows us to use the multiple block write feature of flash devices without having to allocate large
 * buffers to store the data that is being written, so we get the performance advantage of using
 * multiple sector writes without the expense of large buffers.
 * </summary>
 * <param name="device">A pointer to the device driver handle.</param>
 * <param name="sector_address">
 * A 32-bit unsigned integer representing the address of the 1st sector
 * to be written.
 * </param>
 * <param name="buffer">A sector-sized buffer containing the 1st sector of data to be written.</param>
 * <param name="result">
 * A pointer to a 16-bit unsigned integer where the result of the multiple-sector write operation
 * will be stored once the operation completes.
 * </param>
 * <param name="callback_info">
 * A pointer to a STORAGE_CALLBACK_INFO_EX structure that holds the callback function pointer
 * and a context pointer that will be passed back to the callback function.
 * </param>
 * <returns>
 * If successful it should return STORAGE_OP_IN_PROGRESS, otherwise one of the result codes
 * defined in storage_device.h
 * </returns>
 */
typedef uint16_t (*STORAGE_DEVICE_WRITE_MULTIPLE_SECTORS)(void* device, uint32_t sector_address, 
				unsigned char* buffer, uint16_t* result, STORAGE_CALLBACK_INFO_EX* callback_info);


/*!
 * <summary>
 * Contains the interface between fat32lib and the
 * underlying storage device driver. 
 * </summary>
 */
typedef struct _STORAGE_DEVICE 
{
	/*!
	 * <summary>A pointer to the device driver handle.</summary>
	 */
	void* driver;
	/*!
	 * <summary>A pointer to the driver's STORAGE_DEVICE_READ function.</summary>
	 */
	STORAGE_DEVICE_READ read_sector;
	/*!
	 * <summary>A pointer to the driver's STORAGE_DEVICE_READ_ASYNC function.</summary>
	 */
	STORAGE_DEVICE_READ_ASYNC read_sector_async;
	/*!
	 * <summary>A pointer to the driver's STORAGE_DEVICE_WRITE function.</summary>
	 */
	STORAGE_DEVICE_WRITE write_sector;
	/*!
	 * <summary>A pointer to the driver's STORAGE_DEVICE_WRITE_ASYNC function.</summary>
	 */
	STORAGE_DEVICE_WRITE_ASYNC write_sector_async;
	/*!
	 * <summary>A pointer to the driver's STORAGE_DEVICE_GET_SECTOR_SIZEfunction.</summary>
	 */
	STORAGE_DEVICE_GET_SECTOR_SIZE get_sector_size;
	/*!
	 * <summary>A pointer to the driver's STORAGE_DEVICE_GET_SECTOR_COUNT function.</summary>
	 */
	STORAGE_DEVICE_GET_SECTOR_COUNT get_total_sectors;
	/*!
	 * <summary>A pointer to the driver's STORAGE_REGISTER_MEDIA_CHANGED_CALLBACK function.</summary>
	 */
	STORAGE_REGISTER_MEDIA_CHANGED_CALLBACK register_media_changed_callback;
	/*!
	 * <summary>A pointer to the driver's STORAGE_GET_DEVICE_ID function.</summary>
	 */
	STORAGE_GET_DEVICE_ID get_device_id;
	/*!
	 * <summary>A pointer to the driver's STORAGE_GET_PAGE_SIZE function.</summary>
	 */
	STORAGE_GET_PAGE_SIZE get_page_size;
	/*!
	 * <summary>A pointer to the driver's STORAGE_DEVICE_WRITE_MULTIPLE_SECTORS function.</summary>
	 */
	STORAGE_DEVICE_WRITE_MULTIPLE_SECTORS write_multiple_sectors;
	/*!
	 * <summary>A pointer to the driver's STORAGE_DEVICE_ERASE_SECTORS function.</summary>
	 */
	STORAGE_DEVICE_ERASE_SECTORS erase_sectors;
}	
STORAGE_DEVICE, *PSTORAGE_DEVICE;

#endif
