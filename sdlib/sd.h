/*
 * sdlib - SD card driver for fat32lib
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
 
#ifndef __SD_H__
#define __SD_H__

/*! \file sd.h
 * \brief This is the header file for the SD card driver.
 * It handles the SD card access through the SPI bus driver implemented
 * on the HAL library and provides a STORAGE_DEVICE interface to the
 * higher level drivers (fat32lib and smlib.)
 */

/*
 * NOTE!!: The defines wrapped in #if defined(__DOXYGEN__) are in effect
 * commented out. This was done in order to allow the doxygen to see
 * them. If you want to uncomment them just remove the #if defined(__DOXYGEN__).
 *
 */


#include <common.h>
#include <dma.h>
#include <spi.h>
#include "../fat32lib/storage_device.h"


/*!
 * <summary>
 * This is a compile-time option used the enable multi-threading
 * support. WARNING: At the present time multi-threading support has
 * not been fully tested.
 * </summary>
 */
#if defined(__DOXYGEN__)
#define SD_MULTI_THREADED
#endif

/*!
 * <summary>
 * This is a compile-time option to indicate that the driver
 * has it's own background processing thread, or more specifically
 * that none of the IO functions will ever be called by application code
 * from the same thread as sd_idle_processing. It only applies when
 * SD_MULTI_THREADED is defined.
 * </summary>
*/
#if defined(__DOXYGEN__)
#define SD_DRIVER_OWNS_THREAD
#endif

/*!
 * <summary>
 * This is a compile-time option that defines whether the driver
 * should enqueue asynchronous requests (including stream IO requests.)
 * If your application only uses 1 asynchronous IO concurrently you can
 * comment this line out in order to save space.
 * </summary>
 */
#define SD_QUEUE_ASYNC_REQUESTS

/*!
 * <summary>
 * This is a compile-time option that defines the number of asynchronous
 * requests that the driver can handle simultaneously. After this limit is
 * exceeded any asynchronous requests will block until one requests is completed.
 * </summary>
 */
#define SD_ASYNC_QUEUE_LIMIT		(3)

/*!
 * <summary>
 * This is a compile-time option that defines that the driver
 * should allocate memory for the request queue dynamically from
 * the heap. If memory is not available when an asynchronous request
 * is made it will block until the memory becomes available. This
 * option is not supported with SD_ENABLE_MULTI_BLOCK_WRITE.
 * </summary>
 */
#if defined(__DOXYGEN__)
#define SD_ALLOCATE_QUEUE_FROM_HEAP
#endif

/*!
 * <summary>
 * This is a compile-time option that indicates that the driver should be compile
 * with multi-sector write support. This option is required to support stream
 * IO. If your application does not use stream IO you may comment this option
 * out in order to save space.
 * </summary>
 */
#define SD_ENABLE_MULTI_BLOCK_WRITE

/* #define SD_PRINT_DEBUG_INFO */
/* #define SD_PRINT_MULTI_BLOCK_TIME_INFO */
/* #define SD_PRINT_INTER_BLOCK_DELAYS */
/* #define SD_PRINT_TRANSIT_TIME */
/* #define SD_PRINT_PROGRAMMING_DELAYS */

/*
// check for configuration errors.
*/
#if defined(SD_ENABLE_MULTI_BLOCK_WRITE)
#if !defined(SD_QUEUE_ASYNC_REQUESTS)
#error SD_QUEUE_ASYNC_REQUESTS must be defined for multi block transfers.
#endif
#if defined(SD_ALLOCATE_QUEUE_FROM_HEAP)
#error Multi block requests do not support heap allocation for queue.
#endif
#endif

/*!
 * <summary>
 * This is the macro for driver's 1st DMA channel interrupt. It must
 * be called from within that channel's ISR.
 * </summary>
 * <param name="driver">A pointer to the driver handle.</param>
 */
#define SD_DMA_CHANNEL_1_INTERRUPT(driver)								\
	(driver)->context.dma_transfer_completed = 1;						\
	DMA_CHANNEL_CLEAR_INTERRUPT_FLAG((driver)->context.dma_channel_1)
	
/*!
 * <summary>
 * This is the macro for driver's 2nd DMA channel interrupt. It must
 * be called from within that channel's ISR.
 * </summary>
 * <param name="driver">A pointer to the driver handle.</param>
 */
#define SD_DMA_CHANNEL_2_INTERRUPT(driver)								\
	(driver)->context.dma_transfer_completed = 1;						\
	DMA_CHANNEL_CLEAR_INTERRUPT_FLAG((driver)->context.dma_channel_2)



/*!
 * <summary>
 * This is the function pointer required by the file system driver for the 
 * asynchronous IO callback. The file system driver implements this functions 
 * and passes it's pointer to the asynchronous IO functions in order to receive 
 * callbacks.
 * </summary>
 * <param name="result">The result of the asynchronous IO operation.</param>
 * <param name="context">
 * A pointer passed by the file system driver to the storage device driver
 * that contains state information about the async operation.
 * </param>
 */ 
typedef void (*SD_ASYNC_CALLBACK)(uint16_t* result, void* context);

/*!
 * <summary>
 * This is the function pointer required by the volume manager (smlib) to
 * receive notifications when the device is mounted of dismounted. The Volume
 * Manager registers this function when the device is registered with the 
 * Volume Manager.
 * </summary>
 * <param name="device_id">
 * The id of the device. This is the value passed as the id parameter to sd_init .
 * </param>
 * <param name="media_ready">
 * A boolean value that is set to 1 to indicate that the device has been mounted.
 * </param>
 */
typedef void (*SD_MEDIA_STATE_CHANGED)(uint16_t device_id, char media_ready);

/*!
 * <summary>
 * This is the function pointer required by the file system driver for the 
 * stream IO callback. The file system driver implements this functions 
 * and passes it's pointer to the write_multiple_sectors functions in order to receive 
 * callbacks.
 * <summary>
 * <param name="result">The result of the asynchronous IO operation.</param>
 * <param name="context">
 * A pointer passed by the file system driver to the storage device driver
 * that contains state information about the async operation.
 * </param>
 * <param name="buffer">
 * A pointer-to-pointer to the buffer containing the data being read/written
 * to/from the device. During this callback the file system driver may change
 * the buffer or reload it with fresh data to continue the multiple sector transfer.
 * </param>
 * <param name="response">
 * A pointer to the response code. The file system driver may set this value to
 * STORAGE_MULTI_SECTOR_RESPONSE_STOP to signal that the multi sector operation is
 * completed, to STORAGE_MULTI_SECTOR_RESPONSE_SKIP to signal that the multi-sector 
 * operation is not completed but the data is not ready to be written. In this case
 * the driver should enqueue the request, process other pending request and callback
 * again. Or to STORAGE_MULTI_SECTOR_RESPONSE_READY to indicate that the buffer has
 * been changed or releaded with fresh data and that the multi-sector operation should
 * continue.
 * </param>
 */
typedef void (*SD_ASYNC_CALLBACK_EX)(uint16_t* result, void* context, unsigned char** buffer, uint16_t* response);

/*!
 * <summary>
 * This structure is used by the SD driver to store callback information
 * about a request. It is reserved for internal use and should not be accessed
 * directly by the application code.
 * </summary>
 */
typedef struct SD_CALLBACK_INFO 
{
	SD_ASYNC_CALLBACK callback;
	void* context;
}
SD_CALLBACK_INFO, *PSD_CALLBACK_INFO;

/*!
 * <summary>
 * This structure is used by the SD driver to store callback information
 * about a request. It is reserved for internal use and should not be accessed
 * directly by the application code.
 * </summary>
 */
typedef struct SD_CALLBACK_INFO_EX
{
	SD_ASYNC_CALLBACK_EX callback;
	void* context;
}
SD_CALLBACK_INFO_EX;

/*!
 * <summary>
 * This structure is used by the SD driver to store information about asynchronous
 * requests. It is reserved for internal use and should not be accessed directly by 
 * the application code.
 * </summary>
 */
typedef struct SD_ASYNC_REQUEST
{
	uint32_t address;
	unsigned char* buffer;
	uint16_t* async_state;
	SD_CALLBACK_INFO* callback_info;		/* todo: we need to keep a copy of this */
	SD_CALLBACK_INFO_EX* callback_info_ex; 	/* todo: unionize this */
	char mode;
	char needs_data;
	struct SD_ASYNC_REQUEST* next;
}
SD_ASYNC_REQUEST;

/*!
 * <summary>
 * This structure stores the state of the SD card driver. It is part of the
 * driver handle (SD_DRIVER) and as such it is reserved for internal use and should
 * not be accessed by the application directly.
 * </summary>
 */
typedef struct SD_DRIVER_CONTEXT 
{
	/*
	// driver state
	*/
	char busy;
	uint16_t last_media_ready;
	#if defined(SD_MULTI_THREADED)
	DEFINE_CRITICAL_SECTION(busy_lock);
	#endif
	/*
	// driver configuration
	*/
	uint16_t id;
	BIT_POINTER media_ready;
	BIT_POINTER cs_line;
	BIT_POINTER busy_signal;
	SPI_MODULE spi_module;
	DMA_CHANNEL* dma_channel_1;
	DMA_CHANNEL* dma_channel_2;
	char* dma_byte;
	SD_MEDIA_STATE_CHANGED media_changed_callback;
	unsigned char* async_buffer;
	uint32_t timeout;
	/*
	// active dma transfer
	*/
	char dma_transfer_state;
	char dma_transfer_completed;
	char dma_transfer_skip_callback;
	unsigned char* dma_transfer_buffer;
	#if defined(SD_ENABLE_MULTI_BLOCK_WRITE)
	uint16_t dma_transfer_blocks_remaining;
	uint16_t dma_transfer_response;
	uint16_t* dma_transfer_result;
	uint32_t dma_transfer_address;
	SD_ASYNC_CALLBACK dma_transfer_callback;
	SD_CALLBACK_INFO_EX* dma_transfer_callback_info_ex;
	SD_ASYNC_CALLBACK_EX dma_transfer_callback_ex; /* todo: unionize these */
	void* dma_transfer_callback_context;			/* todo: merge the last two and this into struct */
	/*
	// this are used only for debugging
	*/
	#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
	uint32_t dma_transfer_busy_count;
	uint32_t dma_transfer_block_count;
	uint16_t dma_transfer_program_time_s;
	uint16_t dma_transfer_program_time_ms;
	uint16_t dma_transfer_overhead_time_s;
	uint16_t dma_transfer_overhead_time_ms;
	uint32_t dma_transfer_total_blocks;
	uint32_t dma_transfer_time_s;
	uint32_t dma_transfer_time_ms;
	uint32_t dma_transfer_bus_time_s;
	uint32_t dma_transfer_bus_time_ms;
	uint32_t dma_transfer_program_timer_s;
	uint32_t dma_transfer_program_timer_ms;
	uint32_t dma_transfer_overhead_timer_s;
	uint32_t dma_transfer_overhead_timer_ms;
	#endif
	#endif
	/*
	// async request queue
	*/
	#if defined(SD_QUEUE_ASYNC_REQUESTS)
	SD_ASYNC_REQUEST* request_queue;
	#if !defined(SD_ALLOCATE_QUEUE_FROM_HEAP)
	#if defined(SD_ENABLE_MULTI_BLOCK_WRITE)
	SD_ASYNC_REQUEST request_queue_slots[SD_ASYNC_QUEUE_LIMIT + 1]; /* we need to guarantee a slot for multi-block transfers */
	#else
	SD_ASYNC_REQUEST request_queue_slots[SD_ASYNC_QUEUE_LIMIT];
	#endif
	#endif
	#if defined(SD_MULTI_THREADED)
	DEFINE_CRITICAL_SECTION(request_queue_lock);
	#endif
	#endif
}
SD_DRIVER_CONTEXT; 

/*!
 * <summary>
 * This structure stores information about the currently mounted
 * SD card. It is part of the device driver handle (SD_DRIVER structure) and
 * as such it is reserved for internal use and should not be accessed by
 * the application directly.
 * </summary>
 */
typedef struct SD_CARD
{
	unsigned char version;			/* SD card spec. version */
	unsigned char high_capacity;	/* value of 1 indicates SDHC or SDXC card */
	unsigned char taac;
	unsigned char nsac;
	unsigned char r2w_factor;	
	uint32_t capacity;				/* SD card capacity in blocks */	
	uint32_t au_size;				/* size of allocation units todo: make 16-bits */
	uint32_t ru_size;				/* size of recording units todo: make 16-bits */
	uint16_t block_length;			/* block length (always 512/make constant) */
}
SD_CARD;

/*!
 * <summary>
 * The SD card device driver handle. The members of this structure are
 * reserved for internal use and should not be accessed by the application
 * directly.
 * </summary>
 */
typedef struct SD_DRIVER
{
	SD_CARD card_info;
	SD_DRIVER_CONTEXT context;
} 
SD_DRIVER;	

/*!
 * <summary>Initializes the SD card driver.</summary>
 * <param name="driver">A pointer to the driver handle (SD_DRIVER structure)</param>
 * <param name="spi_module">The SPI module used to connect to the SD card.</param>
 * <param name="dma_channel_1">
 * The 1st DMA channel used by the driver. This is only required for asynchronous (and stream) IO.
 * If you don't use this features you can set this paramter to zero. Use the DMA_GET_CHANNEL() macro 
 * included in the HAL library to initialize a DMA channel instance.
 * </param>
 * <param name="dma_channel_2">
 * The 2nd DMA channel used by the driver. This is only required for asynchronous (and stream) IO.
 * If you don't use this features you can set this paramter to zero. Use the DMA_GET_CHANNEL() macro 
 * included in the HAL library to initialize a DMA channel instance.
 * </param>
 * <param name="dma_buffer">
 * DMA buffer used by the driver. This is only required for asynchronous (and stream) IO.
 * If your application doesn't use this features you can set this parameter to zero.
 * </param>
 * <param name="dma_byte">
 * A byte of DMA memory. This is only required by asynchronous IO in order to sustain
 * a DMA transfer through the SPI since since the SPI bus requires that reads and writes
 * be done simultaneously in order to sustain a transfer. If your application does not use
 * asynchronous IO you can set this parameter to zero.
 * </param>
 * <param name="cs_line">
 * A pointer to the pin that is connected to the CS line of the SD card.
 * Use the BP_INIT macro included in the HAL library to initialize a 
 * bit pointer.
 * </param>
 * <param name="busy_signal">
 * A pointer to a pin (or a bit of memory) that will be set HIGH by the driver
 * during disk activity.
 * </param>
 * <param name="id">
 * A unique device id. This is used by smlib to identify the device.
 * </param>
 * <returns>
 * If successful it will return SD_SUCCESS, otherwise it will return one of the
 * result codes defined in sd.h.
 * </returns>
 * <remarks>
 * The interrupts for dma_channel_1 and dma_channel_2 must by handled with the
 * SD_DMA_CHANNEL_1_INTERRUPT() and SD_DMA_CHANNEL_2_INTERRUPT() macros.
 * </remarks>
 */
uint16_t sd_init
( 
	SD_DRIVER* driver, 
	SPI_MODULE spi_module,
	DMA_CHANNEL* const dma_channel_1, 
	DMA_CHANNEL* const dma_channel_2, 
	unsigned char* dma_buffer,
	char* dma_byte,
	BIT_POINTER media_ready,
	BIT_POINTER cs_line,
	BIT_POINTER busy_signal,
	uint16_t id
);

/*!
 * <summary>
 * Initializes the STORAGE_DEVICE interface used by the fat32lib and smlib modules
 * to communicate with the device.
 * </summary>
 * <param name="driver">
 * A pointer to the initialized driver handle (SD_DRIVER structure).
 * </param>
 * <param name="driver_interface">
 * A pointer to a STORAGE_DEVICE structure to initialize.
 * </param>
 *
 */
void sd_get_storage_device_interface
( 
	SD_DRIVER* driver, 
	STORAGE_DEVICE* driver_interface 
);

/*!
 * <summary>
 * Performs the driver's background processing. This function should
 * be called from within your applications main loop.
 * </summary>
 * <param name="driver">A pointer to the driver handle (SD_DRIVER structure.)</param>
 */
void sd_idle_processing
( 
	SD_DRIVER* driver 
);

#endif
