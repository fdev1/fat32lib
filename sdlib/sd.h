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

#include <common.h>
#include <dma.h>
#include <spi.h>
#include "..\fat32lib\storage_device.h"

/*
// define SD_MULTI_THREADED to compile with support
// for multi-threaded systems. Note: if you call any
// of the IO functions from an interrupt handler (which
// I strongly recommend against) then you must compile
// with this option.
*/
/* #define SD_MULTI_THREADED */

/*
// define SD_DRIVER_OWNS_THREAD only if the driver
// has it's own bakground processing thread, or more 
// specifically, if none of the IO functions is ever
// called by user code from the same thread that calls
// sd_idle_processing.
*/
/* #define SD_DRIVER_OWNS_THREAD */

/*
// async request queue options
// if you do more than 1 concurrent async request
// you must define SD_QUEUE_ASYNC_REQUESTS and
// SD_ASYNC_QUEUE_LIMIT. The recommended value
// for SD_ASYNC_QUEUE_LIMIT is (x - 1) where x is
// the number of concurrent async operations that
// your application may perform.
*/
#define SD_QUEUE_ASYNC_REQUESTS
#define SD_ASYNC_QUEUE_LIMIT		(3)

/*
// if SD_ALLOCATE_QUEUE_FROM_HEAP is defined this
// driver will allocate memory for the queue dynamically
// from the heap. Be careful when using this option because
// if you run out of memory and attemp an asynchronous
// request it will block until the memory becomes
// available
*/
/* #define SD_ALLOCATE_QUEUE_FROM_HEAP */

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

/*
// macros for the DMA interrupts
*/
#define SD_DMA_CHANNEL_1_INTERRUPT(sd)								\
	(sd)->context.dma_transfer_completed = 1;						\
	DMA_CHANNEL_CLEAR_INTERRUPT_FLAG((sd)->context.dma_channel_1)
	
#define SD_DMA_CHANNEL_2_INTERRUPT(sd)								\
	(sd)->context.dma_transfer_completed = 1;						\
	DMA_CHANNEL_CLEAR_INTERRUPT_FLAG((sd)->context.dma_channel_2)



/*
// SD driver callback functions
*/ 
typedef void (*SD_ASYNC_CALLBACK)(uint16_t* State, void* context);
typedef void (*SD_MEDIA_STATE_CHANGED)(uint16_t device_id, char media_ready);
typedef void (*SD_ASYNC_CALLBACK_EX)(uint16_t*state, void* context, unsigned char** buffer, uint16_t* action);

/*
// SD callback info structure
*/
typedef struct SD_CALLBACK_INFO 
{
	SD_ASYNC_CALLBACK callback;
	void* context;
}
SD_CALLBACK_INFO, *PSD_CALLBACK_INFO;

typedef struct SD_CALLBACK_INFO_EX
{
	SD_ASYNC_CALLBACK_EX callback;
	void* context;
}
SD_CALLBACK_INFO_EX;

/*
// async request
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

/*
// SD card driver context
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
	DMA_CHANNEL* dma_channel_1;
	DMA_CHANNEL* dma_channel_2;
	unsigned char dma_spi_bus_irq;
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

/*
// SD card info structure
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

/*
// SD card driver handle
*/
typedef struct SD_DRIVER
{
	SD_CARD card_info;
	SD_DRIVER_CONTEXT context;
} 
SD_DRIVER;	

/*
// initializes the SD card driver
*/
uint16_t sd_init
( 
	SD_DRIVER* driver, 
	DMA_CHANNEL* const channel1, 
	DMA_CHANNEL* const channel2, 
	const unsigned char bus_dma_irq,
	unsigned char* async_buffer,
	char* dma_byte,
	BIT_POINTER media_ready,
	BIT_POINTER cs_line,
	BIT_POINTER busy_signal,
	uint16_t id
);

/*
// gets the storage device interface for
// this driver
*/
void sd_get_storage_device_interface
( 
	SD_DRIVER* card_info, 
	STORAGE_DEVICE* device 
);

/*
// performs driver background processing
*/
void sd_idle_processing
( 
	SD_DRIVER* sd 
);

#endif
