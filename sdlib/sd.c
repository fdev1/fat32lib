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

#include "sd.h"
#include <stdlib.h>
#include <stdio.h>

/*
// SD version
*/
#define SD_VERSION_1_0							0x10
#define SD_VERSION_2_0							0x20

/*
// speed classes
*/
#define SD_SPEED_CLASS0							0
#define SD_SPEED_CLASS2							1
#define SD_SPEED_CLASS4 						2
#define SD_SPEED_CLASS6							3
#define SD_SPEED_CLASS10						4

/*
// SD R1 response fields
*/
#define SD_RSP_INVALID_PARAMETER				0x40
#define SD_RSP_ADDRESS_ERROR					0x20
#define SD_RSP_ERASE_SEQ_ERROR					0x10
#define SD_RSP_CRC_ERROR						0x08
#define SD_RSP_ILLEGAL_COMMAND					0x04
#define SD_RSP_ERASE_RESET						0x02
#define SD_RSP_IDLE								0x01

/*
// misc
*/
#define SD_BLOCK_START_TOKEN					0xFE
#define SD_BLOCK_START_TOKEN_MULT				0xFC
#define SD_BLOCK_STOP_TOKEN_MULT				0xFD
#define SD_WAIT_TOKEN							0x00
#define SD_DATA_ERROR_UNKNOWN					0x01
#define SD_DATA_RESPONSE_MASK					0x1F
#define SD_DATA_RESPONSE_ACCEPTED				0x05
#define SD_DATA_RESPONSE_REJECTED_CRC_ERROR		0x0B
#define SD_DATA_RESPONSE_REJECTED_WRITE_ERROR	0x0D
#define SD_DATA_ERROR_CC_ERROR					0x02
#define SD_DATA_ERROR_ECC_FAILED				0x04
#define SD_DATA_ERROR_OUT_OF_RANGE				0x08
#define SD_SPI_TIMEOUT							0xFFFF/*0xFDE8*/

/*
// SD commands
*/
#define CMD_MASK								(0x40)
#define GO_IDLE_STATE							(CMD_MASK | 0x00)
#define SEND_IF_COND							(CMD_MASK | 0x08)
#define SEND_CSD								(CMD_MASK | 0x09)
#define SEND_BLOCK_LEN							(CMD_MASK | 0x10)
#define SEND_STATUS								(CMD_MASK | 0x0D)
#define READ_SINGLE_BLOCK						(CMD_MASK | 0x11)
#define WRITE_SINGLE_BLOCK						(CMD_MASK | 0x18)
#define WRITE_MULTIPLE_BLOCK					(CMD_MASK | 0X19)
#define SD_APP_OP_COND							(CMD_MASK | 0x29)
#define APP_CMD									(CMD_MASK | 0x37)
#define READ_OCR								(CMD_MASK | 0x3A)
#define ERASE_WR_BLK_START						(CMD_MASK | 32)
#define ERASE_WR_BLK_END						(CMD_MASK | 33)
#define ERASE									(CMD_MASK | 38)


/*
// return codes
*/
#define SD_SUCCESS								(STORAGE_SUCCESS)
#define SD_VOLTAGE_NOT_SUPPORTED				(STORAGE_VOLTAGE_NOT_SUPPORTED)
#define SD_INVALID_CARD							(STORAGE_INVALID_MEDIA)
#define SD_UNKNOWN_ERROR						(STORAGE_UNKNOWN_ERROR)
#define SD_INVALID_PARAMETER					(STORAGE_INVALID_PARAMETER)
#define SD_ADDRESS_ERROR						(STORAGE_ADDRESS_ERROR)
#define SD_ERASE_SEQ_ERROR						(STORAGE_ERASE_SEQ_ERROR)
#define SD_CRC_ERROR							(STORAGE_CRC_ERROR)
#define SD_ILLEGAL_COMMAND						(STORAGE_ILLEGAL_COMMAND)
#define SD_ERASE_RESET							(STORAGE_ERASE_RESET)
#define SD_COMMUNICATION_ERROR					(STORAGE_COMMUNICATION_ERROR)
#define SD_TIMEOUT								(STORAGE_TIMEOUT)
#define SD_IDLE									(STORAGE_IDLE)
#define SD_OUT_OF_RANGE							(STORAGE_OUT_OF_RANGE)
#define SD_CARD_ECC_FAILED						(STORAGE_MEDIA_ECC_FAILED)
#define SD_CC_ERROR								(STORAGE_CC_ERROR)
#define SD_PROCESSING							(STORAGE_OP_IN_PROGRESS)
#define SD_CARD_NOT_READY						(STORAGE_DEVICE_NOT_READY)
#define SD_AWAITING_DATA						(STORAGE_AWAITING_DATA)
#define SD_INVALID_MULTI_BLOCK_RESPONSE			(STORAGE_INVALID_MULTI_BLOCK_RESPONSE)

/*
// possible responses to a multi-block write callback
*/
#define SD_MULTI_BLOCK_RESPONSE_STOP			(STORAGE_MULTI_SECTOR_RESPONSE_STOP)
#define SD_MULTI_BLOCK_RESPONSE_SKIP			(STORAGE_MULTI_SECTOR_RESPONSE_SKIP)
#define SD_MULTI_BLOCK_RESPONSE_READY			(STORAGE_MULTI_SECTOR_RESPONSE_READY)

/*
// the default block length supported
// by SD cards ( and the only block length
// used by this implementation
*/
#define SD_BLOCK_LENGTH							(0x200)
#define SD_SPI_MODULE							(1)

/*
// used for marking queue entries
*/
#define SD_FREE_REQUEST							(0x0)
#define SD_READ_REQUEST							(0x1)
#define SD_WRITE_REQUEST						(0x2)
#define SD_MULTI_BLOCK_WRITE_REQUEST			(0x3)

/*
// dma transfer states
*/
#define SD_BLOCK_READ_IN_PROGRESS				0
#define SD_BLOCK_WRITE_IN_PROGRESS				1
#define SD_BLOCK_WRITE_PROGRAMMING				2
#define SD_MULTI_BLOCK_WRITE_IN_PROGRESS		3
#define SD_MULTI_BLOCK_WRITE_PROGRAMMING		4
#define SD_MULTI_BLOCK_WRITE_COMPLETING			5


/*
// convenience macros
*/
#define ASSERT_CS(sd)							BP_CLR(sd->context.cs_line)
#define DEASSERT_CS(sd)							BP_SET(sd->context.cs_line)
#define IS_CARD_LOCKED()						(0x0)

/*
// send a command to the SD card with
// all-zeroes arguments
*/
#define SEND_COMMAND(spi_module, cmd)			\
{											\
	spi_write(spi_module, cmd);			\
	spi_write(spi_module, 0x00);			\
	spi_write(spi_module, 0x00);			\
	spi_write(spi_module, 0x00);			\
	spi_write(spi_module, 0x00);			\
	spi_write(spi_module, 0x95);			\
}

/*
// send a command /w args to the SD card
*/
#define SEND_COMMAND_WITH_ARGS(spi_module, cmd, arg0, arg1, arg2, arg3, crc)	\
{												\
	spi_write(spi_module, cmd);						\
	spi_write(spi_module, arg0);						\
	spi_write(spi_module, arg1);						\
	spi_write(spi_module, arg2);						\
	spi_write(spi_module, arg3);						\
	spi_write(spi_module, crc | 1);						\
}

/*
// sends an IO command to the SD card
*/
#define SEND_IO_COMMAND(spi_module, cmd, address)				\
{													\
	sd_send_io_command(spi_module, cmd, address);				\
}

/*
// wait for a response to an SD command
*/
#define WAIT_FOR_RESPONSE(driver, rsp) 										\
{																			\
	sd_wait_for_response(driver, &rsp);										\
}	

/*
// wait for the data response token
*/
#define WAIT_FOR_DATA_RESPONSE_TOKEN(driver, token)					\
{																	\
	driver->context.timeout = SD_SPI_TIMEOUT;						\
	do 																\
	{																\
		token = spi_read(driver->context.spi_module);						\
		token &= SD_DATA_RESPONSE_MASK;								\
		driver->context.timeout--;									\
	}																\
	while (token != SD_DATA_RESPONSE_ACCEPTED &&					\
		token != SD_DATA_RESPONSE_REJECTED_CRC_ERROR &&				\
		token != SD_DATA_RESPONSE_REJECTED_WRITE_ERROR && 			\
		driver->context.timeout);									\
	driver->context.timeout = (driver->context.timeout) ? 0 : 1;	\
}			

/*
// translate the response
*/
#define TRANSLATE_RESPONSE(rsp)									\
	sd_translate_response(rsp)
	
/*
// waits for incoming data
*/
#define WAIT_FOR_DATA(driver)									\
{																\
	uint16_t temp;												\
	temp = sd_wait_for_data(driver);							\
	if (temp != SD_SUCCESS)										\
		return temp;											\
}		


/*
// wait while the SD card is busy
*/
#define WAIT_WHILE_CARD_BUSY(spi_module)		\
{									\
	unsigned char __temp__;			\
	do 								\
	{ 								\
		__temp__ = spi_read(spi_module); 	\
	}								\
	while (__temp__ == 0x0);		\
}	

/*
// prototypes
*/
uint16_t sd_init_internal(SD_DRIVER* driver);
void sd_init_dma(SD_DRIVER* sd );
unsigned char calc_crc7(unsigned char value, unsigned char crc);
uint16_t sd_read(SD_DRIVER*, uint32_t address, unsigned char* buffer, uint16_t* async_state, SD_CALLBACK_INFO* callback_info, char from_queue);
uint16_t sd_write(SD_DRIVER* driver, uint32_t address, unsigned char* buffer, uint16_t* async_state, SD_CALLBACK_INFO* callback_info, char from_queue);
uint16_t sd_erase(SD_DRIVER* driver, uint32_t start_address, uint32_t end_address, char from_queue);
static uint16_t sd_wait_for_data(SD_DRIVER* driver);
static void sd_wait_for_response(SD_DRIVER* driver, unsigned char* data);
static unsigned char sd_translate_response(unsigned char response);
static void sd_send_io_command(SPI_MODULE spi_module, unsigned char cmd, uint32_t address);

#if defined(SD_ENABLE_MULTI_BLOCK_WRITE)
uint16_t sd_write_multiple_blocks
( 
	SD_DRIVER* driver,	
	unsigned long address, 
	unsigned char* buffer, 
	uint16_t* async_state, 
	SD_CALLBACK_INFO_EX* callback_info,
	char from_queue,
	char needs_data 
);
#endif

/*
// storage device interface
*/
uint16_t sd_get_sector_size(SD_DRIVER* driver);
uint16_t sd_read_sector(SD_DRIVER* driver, uint32_t sector_address, unsigned char* buffer);
uint16_t sd_write_sector(SD_DRIVER* driver, uint32_t sector_address, unsigned char* buffer);
uint16_t sd_read_sector_async(SD_DRIVER* driver, uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, PSD_CALLBACK_INFO callback_info);
uint16_t sd_write_sector_async(SD_DRIVER* driver, uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, PSD_CALLBACK_INFO callback_info);
uint16_t sd_write_multiple_sectors(SD_DRIVER* driver, uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, SD_CALLBACK_INFO_EX* callback_info);
uint16_t sd_erase_sectors(SD_DRIVER* driver, uint32_t start_address, uint32_t end_address);
uint32_t sd_get_total_sectors(SD_DRIVER* driver);
uint32_t sd_get_device_id(SD_DRIVER* driver);
uint32_t sd_get_page_size(SD_DRIVER* driver);
void sd_register_media_changed_callback(SD_DRIVER* driver, SD_MEDIA_STATE_CHANGED callback);

/*
// gets the storage device interface
*/
void sd_get_storage_device_interface(SD_DRIVER* driver, STORAGE_DEVICE* device) 
{
	device->driver = driver;
	device->read_sector 					= (STORAGE_DEVICE_READ) &sd_read_sector;
	device->write_sector 					= (STORAGE_DEVICE_WRITE) &sd_write_sector;
	device->get_sector_size 				= (STORAGE_DEVICE_GET_SECTOR_SIZE) &sd_get_sector_size;
	device->read_sector_async 				= (STORAGE_DEVICE_READ_ASYNC) &sd_read_sector_async;
	device->write_sector_async 				= (STORAGE_DEVICE_WRITE_ASYNC) &sd_write_sector_async;
	device->get_total_sectors 				= (STORAGE_DEVICE_GET_SECTOR_COUNT) &sd_get_total_sectors;
	device->get_device_id 					= (STORAGE_GET_DEVICE_ID) &sd_get_device_id;
	device->get_page_size 					= (STORAGE_GET_PAGE_SIZE) &sd_get_page_size;
	device->register_media_changed_callback = (STORAGE_REGISTER_MEDIA_CHANGED_CALLBACK) &sd_register_media_changed_callback;
	device->erase_sectors 					= (STORAGE_DEVICE_ERASE_SECTORS) &sd_erase_sectors;
	#if defined(SD_ENABLE_MULTI_BLOCK_WRITE)
	device->write_multiple_sectors 			= (STORAGE_DEVICE_WRITE_MULTIPLE_SECTORS) &sd_write_multiple_sectors;
	#endif
	
}

/*
// gets the device id
*/
uint32_t sd_get_device_id(SD_DRIVER* driver)
{
	return driver->context.id;
}
	
/*
// registers a callback function to receive notifications
// when a card is inserted or removed from the slot
*/
void sd_register_media_changed_callback(SD_DRIVER* driver, SD_MEDIA_STATE_CHANGED callback)
{
	driver->context.media_changed_callback = callback;
}

/*
// gets the total capacity of the SD card
// in bytes
*/
uint32_t sd_get_total_sectors(SD_DRIVER* driver) 
{
	/* return (driver->card_info.high_capacity) ? 
		driver->card_info.capacity : (driver->card_info.capacity / driver->card_info.block_length);*/
	return driver->card_info.capacity;
}	

/*
// gets the size of the card's allocation untis in sectors/blocks
*/
uint32_t sd_get_page_size(SD_DRIVER* driver)
{
	return driver->card_info.au_size;
}

/*
// gets the sector length
*/
uint16_t sd_get_sector_size(SD_DRIVER* driver) 
{
	return SD_BLOCK_LENGTH;
}

/*
// erase sectors
*/
uint16_t sd_erase_sectors(SD_DRIVER* driver, uint32_t start_address, uint32_t end_address)
{
	if (driver->card_info.high_capacity)
	{
		return sd_erase(driver, start_address, end_address, 0);
	}
	else
	{
		return sd_erase(driver, start_address * SD_BLOCK_LENGTH, end_address * SD_BLOCK_LENGTH, 0);
	}	
}

/*
// read sector synchronously
*/
uint16_t sd_read_sector(SD_DRIVER* driver, uint32_t sector_address, unsigned char* buffer) 
{
	uint32_t address = (driver->card_info.high_capacity) ? 
		sector_address : sector_address * driver->card_info.block_length;
	return sd_read(driver, address, buffer, 0, 0, 0);
}

/*
// read sector asynchronously
*/
uint16_t sd_read_sector_async(SD_DRIVER* driver, uint32_t sector_address, 
	unsigned char* buffer, uint16_t* async_state, SD_CALLBACK_INFO* callback) 
{
	uint32_t address = ( driver->card_info.high_capacity ) ? 
		sector_address : sector_address * driver->card_info.block_length;
	return sd_read(driver, address, buffer, async_state, callback, 0);
}	

/*
// write sector synchronously
*/
uint16_t sd_write_sector(SD_DRIVER* driver, uint32_t sector_address, unsigned char* buffer) 
{
	uint32_t address = (driver->card_info.high_capacity) ? 
		sector_address : sector_address * driver->card_info.block_length;
	return sd_write(driver, address, buffer, 0, 0, 0);	
}	

/*
// write sector asynchronously
*/
uint16_t sd_write_sector_async(SD_DRIVER* driver, 
	uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, SD_CALLBACK_INFO* callback) 
{
	uint32_t address = (driver->card_info.high_capacity) ? 
		sector_address : sector_address * driver->card_info.block_length;
	return sd_write(driver, address, buffer, async_state, callback, 0);
}	

#if defined(SD_ENABLE_MULTI_BLOCK_WRITE)
uint16_t sd_write_multiple_sectors(SD_DRIVER* driver, uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, SD_CALLBACK_INFO_EX* callback)
{
	uint32_t address = (driver->card_info.high_capacity) ? 
		sector_address : sector_address * driver->card_info.block_length;
	return sd_write_multiple_blocks(driver, address, buffer, async_state, callback, 0, 0);

}
#endif

/*
// sd card driver initialization routine
*/
uint16_t sd_init
(
	SD_DRIVER* driver, 
	SPI_MODULE spi_module,
	DMA_CHANNEL* const dma_channel1, 
	DMA_CHANNEL* const dma_channel2, 
	unsigned char* async_buffer,
	char* dma_byte,
	BIT_POINTER media_ready,
	BIT_POINTER cs_line,
	BIT_POINTER busy_signal,
	uint16_t id
) 
{
	/*
	// initialize request queue
	*/
	#if defined(SD_QUEUE_ASYNC_REQUESTS)
	#if !defined(SD_ALLOCATE_QUEUE_FROM_HEAP)
	int i;
	for (i = 0; i < SD_ASYNC_QUEUE_LIMIT; i++)
	{
		driver->context.request_queue_slots[i].mode = SD_FREE_REQUEST;
	}
	#endif
	driver->context.request_queue = 0;
	#if defined(SD_MULTI_THREADED)
	INITIALIZE_CRITICAL_SECTION(driver->context.request_queue_lock);
	#endif
	#endif
	/*
	// initialize driver busy critical section
	*/
	#if defined(SD_MULTI_THREADED)
	INITIALIZE_CRITICAL_SECTION(driver->context.busy_lock);
	#endif
	/*
	// initialize the driver's context
	*/
	driver->context.busy = 0;
	driver->context.dma_transfer_completed = 0;
	driver->context.last_media_ready = 1;
	driver->context.media_ready = media_ready;
	driver->context.cs_line = cs_line;
	driver->context.busy_signal = busy_signal;
	driver->context.media_changed_callback = 0;
	driver->context.id = id;
	driver->context.async_buffer = async_buffer;
	driver->context.spi_module = spi_module;
	/*
	// save copy of the DMA channel pointers
	*/
	driver->context.dma_channel_1 = dma_channel1;
	driver->context.dma_channel_2 = dma_channel2;
	driver->context.dma_byte = dma_byte;
	/*
	// configure the dma channels
	*/
	sd_init_dma(driver);
	/*
	// make sure CS line is not asserted and
	// the busy signal is clear
	*/
	DEASSERT_CS(driver);
	BP_CLR(driver->context.busy_signal);
	/*
	// return success code
	*/
	/* return SD_SUCCESS; */
	return sd_init_internal(driver);
}

/*
// initializes the DMA channels used
// by this driver
*/
void sd_init_dma(SD_DRIVER* sd) 
{
	/*
	// configure the dma channel 0 to transfer a single
	// byte from the SPI receive buffer one time for every
	// byte that is being transfered by channel 1. This is
	// needed in order to sustain the SPI transfer since
	// SPI does not support one-way communication
	*/
	DMA_CHANNEL_DISABLE( sd->context.dma_channel_1 );
	DMA_SET_CHANNEL_WIDTH( sd->context.dma_channel_1, DMA_CHANNEL_WIDTH_BYTE );
	DMA_SET_CHANNEL_DIRECTION( sd->context.dma_channel_1, DMA_DIR_PERIPHERAL_TO_RAM );
	DMA_SET_INTERRUPT_MODE( sd->context.dma_channel_1, DMA_INTERRUPT_MODE_FULL );
	DMA_SET_NULL_DATA_WRITE_MODE( sd->context.dma_channel_1, 0 );
	DMA_SET_ADDRESSING_MODE( sd->context.dma_channel_1, DMA_ADDR_REG_IND_NO_INCREMENT );
	DMA_SET_PERIPHERAL( sd->context.dma_channel_1, SPI_GET_DMA_REQ(sd->context.spi_module));
	DMA_SET_PERIPHERAL_ADDRESS( sd->context.dma_channel_1, SPI_BUFFER_ADDRESS(sd->context.spi_module));
	DMA_SET_BUFFER_A(sd->context.dma_channel_1, sd->context.dma_byte);
	DMA_SET_BUFFER_B(sd->context.dma_channel_1, sd->context.dma_byte);
	DMA_SET_MODE( sd->context.dma_channel_1, DMA_MODE_ONE_SHOT );
	DMA_SET_TRANSFER_LENGTH( sd->context.dma_channel_1, SD_BLOCK_LENGTH - 1);
	DMA_CHANNEL_CLEAR_INTERRUPT_FLAG( sd->context.dma_channel_1 );
	DMA_CHANNEL_DISABLE_INTERRUPT( sd->context.dma_channel_1 );
	/*
	// configure dma channel 1 to transfer a buffer
	// from system memory to the SPI buffer
	*/
	DMA_CHANNEL_DISABLE(sd->context.dma_channel_2);
	DMA_SET_CHANNEL_WIDTH(sd->context.dma_channel_2, DMA_CHANNEL_WIDTH_BYTE);
	DMA_SET_CHANNEL_DIRECTION(sd->context.dma_channel_2, DMA_DIR_RAM_TO_PERIPHERAL);
	DMA_SET_INTERRUPT_MODE(sd->context.dma_channel_2, DMA_INTERRUPT_MODE_FULL);
	DMA_SET_NULL_DATA_WRITE_MODE(sd->context.dma_channel_2, 0);
	DMA_SET_ADDRESSING_MODE(sd->context.dma_channel_2, DMA_ADDR_REG_IND_W_POST_INCREMENT);
	DMA_SET_PERIPHERAL(sd->context.dma_channel_2, SPI_GET_DMA_REQ(sd->context.spi_module));
	DMA_SET_PERIPHERAL_ADDRESS(sd->context.dma_channel_2, SPI_BUFFER_ADDRESS(sd->context.spi_module));
	DMA_SET_MODE(sd->context.dma_channel_2, DMA_MODE_ONE_SHOT);
	DMA_SET_TRANSFER_LENGTH(sd->context.dma_channel_2, SD_BLOCK_LENGTH - 1);
	DMA_CHANNEL_CLEAR_INTERRUPT_FLAG(sd->context.dma_channel_2);
	DMA_CHANNEL_DISABLE_INTERRUPT(sd->context.dma_channel_2);
}

/*
// initialize SD card
*/
uint16_t sd_init_internal(SD_DRIVER* driver)
{
	unsigned char retries = 0;	/* retries count */
	unsigned char tmp;			/* temp byte for data received from SPI */
	unsigned char read_bl_len;	/* read_bl_len of the CSD (legacy cards) */
	unsigned char c_size_mult;	/* c_size_mult of the CSD (legacy cards) */
	unsigned char sector_size;	/* flash page size (legacy cards) */
	unsigned char speed_class;	/* speed class of version 2 cards */
	SPI_MODULE spi_module;
	uint32_t block_nr;			/* block number (legacy cards) */
	uint32_t c_size = 0;		/* c_size field of the CSD */
	uint16_t mult;				/* card size multiplier (legacy cards) */
	uint16_t err = 0;			/* temp variable */
	
	spi_module = driver->context.spi_module;

init_retry:
	/*
	// deassert cs_line
	*/
	DEASSERT_CS(driver);
	/*
	// initialize the sp module @ < 400 khz
	*/
	spi_init(spi_module);	/* 310.5 khz */
	spi_set_clock(spi_module, 400000L);
	/*
	// if the card is not ready return error
	*/
	if (BP_GET(driver->context.media_ready))
	{
		return SD_CARD_NOT_READY;
	}
	/*
	// set busy signal
	*/
	BP_SET(driver->context.busy_signal);
	/*
	// initialize all card_info fields.
	*/	
	driver->card_info.version = 0;
	driver->card_info.capacity = 0;
	driver->card_info.high_capacity = 0;
	driver->card_info.block_length = 0;
	driver->card_info.taac = 0;
	driver->card_info.nsac = 0;
	driver->card_info.r2w_factor = 0;
	driver->card_info.au_size = 0;
	driver->card_info.ru_size = 0;
	/*
	// assert the CS line of the SD card
	*/
	ASSERT_CS(driver);
	/*
	// send at least 74 clock pulses to the card for it
	// to initialize (80 is as close as we can get).
	*/
	spi_write(spi_module, 0xFF);	
	spi_write(spi_module, 0xFF);	
	spi_write(spi_module, 0xFF);	
	spi_write(spi_module, 0xFF);	
	spi_write(spi_module, 0xFF);	
	spi_write(spi_module, 0xFF);	
	spi_write(spi_module, 0xFF);	
	spi_write(spi_module, 0xFF);	
	spi_write(spi_module, 0xFF);	
	spi_write(spi_module, 0xFF);	
	/*
	// send 16 clock pulses to the card
	*/
	spi_write(spi_module, 0xFF);	
	spi_write(spi_module, 0xFF);	
	/*
	// send the reset command
	*/
	SEND_COMMAND(spi_module, GO_IDLE_STATE);
	/*
	// wait for the response
	*/
	WAIT_FOR_RESPONSE(driver, tmp);
	/*
	// check for timeout condition
	*/
	if (driver->context.timeout) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		return SD_TIMEOUT;
	}
	/*
	// translate the response
	*/	
	/* tmp = TRANSLATE_RESPONSE(tmp); */
	err = SD_SPI_TIMEOUT;
	/*
	// send 8 clock pulses
	*/ 
	/* spi_write(spi_module, 0xFF); */
	while (tmp != 0xFF && tmp & SD_RSP_IDLE)
	{
		if (!err--)
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			return tmp;
		}
		tmp = spi_read(spi_module);
	}
	err = 0;
	/*
	// if an error was returned, then
	// deassert the CS line and return
	// the error code
	*/
	if (tmp && tmp != 0xFF /*!= SD_IDLE */) 
	{
		if (retries++ < 3)
		{
			goto init_retry;
		}	
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		return tmp;
	}
	/*
	// send the command 8
	*/
	SEND_COMMAND_WITH_ARGS(spi_module, SEND_IF_COND, 0x00, 0x00, 0x01, 0xAA, 0x86);
	/*
	// discard the 1st 4 bytes of the response
	*/	
	WAIT_FOR_RESPONSE(driver, tmp);
	/*
	// check for timeout condition
	*/
	if (driver->context.timeout) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		return SD_TIMEOUT;
	}
	/*
	// translate the response
	*/ 
	tmp = TRANSLATE_RESPONSE(tmp);
	/*
	// if the command is not supported then this is either a 
	// standard capacity card, or a legacy SD card or an MMC card
	*/
	if (tmp == SD_ILLEGAL_COMMAND) 
	{
		/*
		// for now we just assume that it is a legacy card, later down 
		// the line we'll know if it's not an SD card when we get the same 
		// response to other commands. If the card is a standard capacity 
		// SD version 2 card we'll treat it just as a legacy card
		*/
		driver->card_info.version = SD_VERSION_1_0;
		/*
		// send 8 clock pulses to the card
		*/	
		spi_write(spi_module, 0xFF);
		/*
		// skip the version 2-specific steps of the
		// initialization process
		*/
		goto __legacy_card;
	}
	/*
	// if we're here, then this must be version 2 card
	*/
	driver->card_info.version = SD_VERSION_2_0;
	/*
	// discard the next 2 bytes of the SEND_IF_COND response
	*/
	tmp = spi_read(spi_module);
	tmp = spi_read(spi_module);
	/*
	// read the next byte which is the supported 
	// operating voltage of the card
	*/
	tmp = spi_read(spi_module);
	/*
	// check that the card supports our operating
	// voltage
	*/
	if ((tmp & 0xF) != 0x1)
	{
		/*
		// give it a minute for voltage to rise before retrying
		*/
		for (err = 0; err < 0xFFFF; err++);
		err = SD_VOLTAGE_NOT_SUPPORTED;
	}	
	/*
	// read the 5th byte of the SEND_IF_COND command response. 
	// This should be an echo of our 4th parameter
	*/
	tmp = spi_read(spi_module);
	/*
	// make sure we understand each other
	*/
	if (tmp != 0xAA) 
	{
		if (retries++ < 5)
		{
			goto init_retry;
		}	
		DEASSERT_CS(driver);
		err = SD_COMMUNICATION_ERROR;
	}
	/*
	// read and discard the last byte of the
	// SEND_IF_COND command response (crc check)
	*/
	tmp = spi_read(spi_module);
	/*
	// send 8 clock pulses to the card
	*/
	spi_write(spi_module, 0xFF);
	/*
	// if an error occurred return the error code
	*/
	if (err != 0) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		return err;
	}
__legacy_card:
	/*
	// we'll now send the SD_APP_OP_COND command until
	// the card becomes ready
	*/
	do 
	{
		/*
		// send an APP_CMD (55) command to indicate that the next
		// command is an application specific command
		*/
		SEND_COMMAND(spi_module, APP_CMD);
		/*
		// read the response
		*/
		WAIT_FOR_RESPONSE(driver, tmp);
		/*
		// check for timeout condition
		*/
		if (driver->context.timeout) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			return SD_TIMEOUT;
		}
		/*
		// send 8 clock pulses to the card
		*/
		spi_write(spi_module, 0xFF);
		/*
		// if we got an invalid command response then the card is 
		// not a valid SD card, it must be an MMC card
		*/ 
		if (tmp & SD_RSP_ILLEGAL_COMMAND) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			return SD_INVALID_CARD;
		}
		/*
		// send the SD_APP_OP_COND (41) command to find out if the card 
		// has completed it's initialization process
		*/
		SEND_COMMAND_WITH_ARGS(spi_module, SD_APP_OP_COND, 0x50, 0x0, 0x0, 0x0, 0x95);
		/*
		// discard the 1st byte of the response
		*/
		WAIT_FOR_RESPONSE(driver, tmp);
		/*
		// check for timeout condition
		*/
		if (driver->context.timeout) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			return SD_TIMEOUT;	
		}
		/*
		// if we got an invalid command response
		// this is not a valid SD card ( mmc? )
		*/
		if (tmp & SD_RSP_ILLEGAL_COMMAND)
			err = SD_INVALID_CARD;
		/*
		// send 8 clock cycles
		*/
		spi_write(spi_module, 0xFF);
		/*
		// if we received an error from the
		// card then return the error code
		*/		
		if (err != 0) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			return err;
		}
	}
	while (tmp & SD_RSP_IDLE);
	/*
	// send READ_OCR (58) command to get the OCR 
	// register in order to check that the card supports our
	// supply volate
	*/
	SEND_COMMAND(spi_module, READ_OCR);
	/*
	// read the response
	*/
	WAIT_FOR_RESPONSE(driver, tmp);	/* R1 */
	/*
	// check for timeout condition
	*/
	if (driver->context.timeout) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		return SD_TIMEOUT;	
	}
	/*
	// translate the response
	*/
	tmp = TRANSLATE_RESPONSE(tmp);
	/*
	// if an error was returned by the
	// card fail the initialization
	*/
	if (tmp) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		return tmp;
	}
	/*
	// read the 1st byte of the OCR
	*/
	tmp = spi_read(spi_module);			/* OCR1 */
	/*
	// save the card capacity status bit
	*/
	driver->card_info.high_capacity = (tmp & 0x40) != 0;
	/*
	// read the 2nd byte of the OCR
	*/
	tmp = spi_read(spi_module);			/* OCR2 */
	/*
	// if neither bit 20 or 21 of the OCR register is set 
	// then the card does not support our supply volage
	*/
	if (!(tmp & 0x30)) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		return SD_VOLTAGE_NOT_SUPPORTED;
	}
	/*
	// read and discard the rest of the OCR
	*/
	tmp = spi_read(spi_module);
	tmp = spi_read(spi_module);
	/*
	// send 8 clock pulses to the card
	*/
	spi_write(spi_module, 0xFF);
	/*
	// send the SEND_CSD command
	// NOTE: Version 2 of the CSD is only applied to SDHC and 
	// SDXC cards, so if the card is Version 2 and standard capacity
	// we'll still use version 1 of this register, that is why bellow
	// we check for driver->card_info.high_capacity instead of the
	// version.
	*/
	SEND_COMMAND(spi_module, SEND_CSD);
	/*
	// wait for the response
	*/
	WAIT_FOR_RESPONSE(driver, tmp);
	/*
	// check for timeout condition
	*/
	if (driver->context.timeout) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		return SD_TIMEOUT;	
	}
	/*
	// translate the response
	*/
	tmp = TRANSLATE_RESPONSE(tmp);
	/*
	// check for an error response
	*/
	if (tmp) 
	{
		spi_write(spi_module, 0xFF);
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		return tmp;		
	}
	/*
	// wait for the data to arrive
	*/ 
	WAIT_FOR_DATA(driver);
	/*
	// test for timeout condition
	*/
	if (driver->context.timeout) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		return SD_TIMEOUT;
	}
	/*
	// read and discard the 1st byte 
	// ofthe CSD register
	*/
	tmp = spi_read(spi_module);
	/*
	// read the 2nd byte of the CSD register into the TAAC 
	// field of the driver structure
	*/
	driver->card_info.taac = spi_read(spi_module);
	/*
	// read the 3rd byte of the CSD register into the NSAC 
	// field of the driver structure
	*/
	driver->card_info.nsac = spi_read(spi_module);
	/*
	// discard bytes 4 and 5 of CSD
	*/
	tmp = spi_read(spi_module);
	tmp = spi_read(spi_module);
	/*
	// read byte 6 of CSD
	*/
	tmp = spi_read(spi_module);
	/*
	// the lower 4 bits of the 6th byte of
	// the CSD is the READ_BL_LEN value
	*/
	read_bl_len = (tmp & 0xF);
	/*
	// read the 7th byte of the CSD register
	*/
	tmp = spi_read(spi_module);
	/*
	// VERSION 1:
	// bits 1-0 of the 7th byte of the CSD contains bits
	// 11-10 of the csize field
	*/
	if (!driver->card_info.high_capacity)
	{
		c_size = ((uint16_t) (tmp & 0x3)) << 10;
	}
	/*
	// read the 8th byte
	*/
	tmp = spi_read(spi_module);
	/*
	// version 1:
	// the 8th byte of the CSD contains bits 9-2 of the c_size field
	// version 2:
	// bits 0-5 of 8th byte are bits 16-21 of c_size field
	*/
	if (!driver->card_info.high_capacity)
	{
		c_size |= ((uint16_t) tmp) << 2;
	}
	else 
	{
		tmp &= 0x3F;
		c_size |= ((uint32_t) tmp) << 16;
	}
	/*
	// read the 9th byte of the CSD register
	*/
	tmp = spi_read(spi_module);
	/*
	// copy bits 4-2 of the 9th byte of the
	// CSD register to the R2W_FACTOR field of
	// the driver structure
	*/
	if (!driver->card_info.high_capacity) 
	{
		driver->card_info.r2w_factor = (tmp & 0x1C) >> 2;
	}
	/*
	// version 1:
	// bits 7-6 of the 9th byte are bits 1-0 of the c_size field
	// bits 1-0 of the 9th byte are bits 2-1 of the C_SIZE_MULT field
	// version 2:
	// 9th byte is byte 1 of c_size field
	*/ 
	if (!driver->card_info.high_capacity) 
	{
		c_size |= ((uint16_t) (tmp & 0xC0)) >> 6;
		c_size_mult = ((uint16_t) (tmp & 0x3)) << 1;	
	}	
	else
	{
		c_size |= ((uint32_t) tmp) << 8;
	}
	/*
	// read the 10th byte of the CSD register
	*/
	tmp = spi_read(spi_module);
	/*
	// version 1:
	// bits 7 of the 10th byte of the CSD is bit 0 of C_SIZE_MULT field
	// version 2:
	// 10th byte of CSD is byte 0 of c_size field
	*/
	if (!driver->card_info.high_capacity)
	{
		c_size_mult |= (tmp & 0x3) << 1;
	}
	else
	{
		c_size |= (uint32_t) tmp;
	}
	/*
	// read the 11th byte of the CSD register
	*/
	tmp = spi_read(spi_module);
	/*
	// version 1: 11th byte of the CSD register
	// bit 7 contains bit 0 of the C_SIZE_MULT field
	// bit 6 contains the ERASE_BLK_EN bit field
	// bits 5-0 contains bits 6-1 of the 7-bit SECTOR_SIZE field
	// 
	*/
	if (!driver->card_info.high_capacity)
	{
		c_size_mult |= ((uint16_t) (tmp & 0x80)) >> 7;
		sector_size = (tmp & 0x3F) << 1;
	}
	/*
	// read the 12th byte
	*/
	tmp = spi_read(spi_module);
	/*
	// version 1:
	// bit 7 of 12th byte contains bit 0 0f SECTOR_SIZE field
	*/
	if (!driver->card_info.high_capacity)
	{
		sector_size |= (tmp >> 7);
		
	}
	/*
	// read and discard bytes 13-16 of the CSD register
	*/
	tmp = spi_read(spi_module);
	tmp = spi_read(spi_module);
	tmp = spi_read(spi_module);
	tmp = spi_read(spi_module);
	/*
	// read and discard the 16 bit crc
	*/
	tmp = spi_read(spi_module);
	tmp = spi_read(spi_module);	
	/*
	// send 8 clock pulses to card
	*/
	spi_write(spi_module, 0xFF);
	/*
	// de-assert the CS line of the SD card
	*/
	DEASSERT_CS(driver);
	/*
	// set the clock speed at 25 MB/s or whatever device supports
	*/
	spi_set_clock(spi_module, 25000000L);	/* 10 mhz @ 40 mips */
	/*
	// assert the CS line of the SD card
	*/
	ASSERT_CS(driver);
	/*
	// send 16 clock pulses to the card
	*/
	spi_write(spi_module, 0xFF);	
	spi_write(spi_module, 0xFF);	
	/*
	// calculate the SD card's capacity
	*/
	if (!driver->card_info.high_capacity) 
	{		
		/*
		// BLOCK_LEN 	= 	2 ^ READ_BL_LEN
		// MULT 		= 	2 ^ (C_SIZE_MULT + 2)
		// BLOCK_NR 	= 	(C_SIZE + 1) * MULT
		// CAPACITY 	= 	BLOCK_NR 
		*/
		driver->card_info.block_length 	= 0x2 << (read_bl_len - 0x1);
		mult 							= 0x2 << (c_size_mult + 0x1);
		block_nr 						= (c_size + 0x1) * mult;
		driver->card_info.capacity		= block_nr; /* * driver->card_info.block_length;	*/
		/*
		// todo: for max compatibility with legacy cards we need
		// get au_size from WRITE_BL_LEN also we need to check that
		// they are 512 and if not then we need to configure the card
		// for partial block writes of 512 bytes because the rest of the
		// driver assumes a 512 bytes block length
		*/
		driver->card_info.au_size 		= (sector_size + 1);
		driver->card_info.ru_size 	    = driver->card_info.au_size;
	}
	else /*if (driver->card_info.version == SD_VERSION_2_0) */
	{
		/*
		// CAPACITY = (C_SIZE + 1) * 512
		*/
		driver->card_info.capacity		= (c_size + 0x1) * 1024;
		driver->card_info.block_length 	= 0x200;
	}
	/*
	// ###
	// Now we need to get the SD_STATUS register
	// on version 2 cards
	// ###
	*/
	if (driver->card_info.version == SD_VERSION_2_0)
	{
		/*
		// send an APP_CMD (55) command to indicate that the next
		// command is an application specific command
		*/
		SEND_COMMAND(spi_module, APP_CMD);
		/*
		// read the response
		*/
		WAIT_FOR_RESPONSE(driver, tmp);
		/*
		// check for timeout condition
		*/
		if (driver->context.timeout) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			return SD_TIMEOUT;
		}
		/*
		// send 8 clock pulses to the card
		*/
		spi_write(spi_module, 0xFF);
		/*
		// if we got an invalid command response then the card is 
		// not a valid SD card, it must be an MMC card. This should never
		// happen as we've already determined that it IS an SD Card.
		*/ 
		#if !defined(NDEBUG)
		if (tmp & SD_RSP_ILLEGAL_COMMAND) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			return SD_INVALID_CARD;
		}
		#endif
		/*
		// send the SEND_STATUS command
		*/
		SEND_COMMAND(spi_module, SEND_STATUS);
		/*
		// wait for the response
		*/
		WAIT_FOR_RESPONSE(driver, tmp);
		/*
		// check for timeout condition
		*/
		if (driver->context.timeout) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			return SD_TIMEOUT;	
		}
		/*
		// translate the response
		*/
		tmp = TRANSLATE_RESPONSE(tmp);
		/*
		// check for an error response
		*/
		if (tmp) 
		{
			spi_write(spi_module, 0xFF);
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			return tmp;		
		}
		/*
		// wait for the data to arrive
		*/ 
		WAIT_FOR_DATA(driver);
		/*
		// test for timeout condition
		*/
		if (driver->context.timeout) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			return SD_TIMEOUT;
		}
		/*
		// SD_STATUS is 64 bytes wide
		// Lets read and discard the 1st 8 bytes
		*/
		tmp = spi_read(spi_module);
		tmp = spi_read(spi_module);	
		tmp = spi_read(spi_module);
		tmp = spi_read(spi_module);	
		tmp = spi_read(spi_module);
		tmp = spi_read(spi_module);	
		tmp = spi_read(spi_module);
		tmp = spi_read(spi_module);	
		/*
		// read the speed class
		*/
		speed_class = spi_read(spi_module);
		/*
		// read and discard the 10th byte
		*/
		tmp = spi_read(spi_module);	
		/*
		// read byte 11 which contains the AU_SIZE field
		// in the upper 4 bits
		*/
		tmp = spi_read(spi_module);	
		/*
		// shift in order to get the upper 4 bits
		*/		
		tmp >>= 4;
		/*
		// the actual AU size can be calculated as
		// 4096 * (2 ^ AU_SIZE)
		*/
		if (tmp)
		{
			driver->card_info.au_size = ((uint32_t) 4096) * (2 << (uint16_t) tmp);
			driver->card_info.au_size /= 0x200;	/* in terms of block length */
			/*
			// the RU size is determined from card capacity 
			// and speed class
			*/
			switch (speed_class)
			{
				case SD_SPEED_CLASS0:
					/*
					// i don't think this should even happen
					// for now we'll assume it doesn't
					*/
					while(1);
					break;
					
				case SD_SPEED_CLASS2:
				case SD_SPEED_CLASS4:
				
					if (driver->card_info.capacity <= 0x200000) /* cards <= 1GB */
					{
						driver->card_info.ru_size = 0x20; /* 16KB */
					}
					else
					{	
						driver->card_info.ru_size = 0x40; /* 32KB */
					}	
					break;
					
				case SD_SPEED_CLASS6:
				
					driver->card_info.ru_size = 0x80;	/* 64KB */
					break;
					
				case SD_SPEED_CLASS10:
				
					driver->card_info.ru_size = 0x400;	/* 512KB */
					break;
					
				default:
					/*
					// this card is too classy for use so this
					// is just a guess, actually RU size is probably
					// larger
					*/
					driver->card_info.ru_size = 0x400;	/* 512KB */
					break;				
			}
			
		}	
		/*
		// read byte 12 which contains the upper byte of the
		// ERASE_SIZE field
		*/
		tmp = spi_read(spi_module);
		/*
		// we'll store it in the card's ru field for now
		*/
		/* driver->card_info.ru_size = ((uint32_t) tmp) << 8; */
		/*
		// now we read byte 13 which contains the lower byte
		*/
		tmp = spi_read(spi_module);
		/*
		// read and discard the next 51 bytes
		*/
		for (err = 0; err < 51; err++)
		{
			tmp = spi_read(spi_module);
		}	
		/*
		// read and discard the 16 bit crc
		*/
		tmp = spi_read(spi_module);
		tmp = spi_read(spi_module);	
		/*
		// send 8 clock pulses to card
		*/
		spi_write(spi_module, 0xFF);
	}	
	/*
	// de-assert the CS line
	*/
	DEASSERT_CS(driver);
	/*
	// clear busy signal
	*/
	BP_CLR(driver->context.busy_signal);
	/*
	// return success code
	*/ 
	return SD_SUCCESS;
}

/*
// reads a block of data from an SD card
*/
uint16_t sd_read
( 
	SD_DRIVER* driver, 
	uint32_t address, 
	unsigned char* buffer, 
	uint16_t* async_state, 
	SD_CALLBACK_INFO* callback_info, 
	char from_queue
) 
{
	unsigned char tmp;
	uint16_t i;
	/*
	// if the card is not ready return error
	*/
	if (BP_GET(driver->context.media_ready))
	{
		if (from_queue)
		{
			driver->context.busy = 0;
		}
		return SD_CARD_NOT_READY;
	}
	/*
	// if this is an asynchronous read...
	*/
	if (async_state != 0) 
	{	
		/*
		// if this is an async request and it doesn't come
		// from the queue we'll just enqueue it and return
		// for all other requests we'll wait for the device
		// to become available and continue
		*/
		#if defined(SD_QUEUE_ASYNC_REQUESTS)
		if (!from_queue)
		{
			while (1) 
			{
				SD_ASYNC_REQUEST** req;
				/*
				// lock the queue
				*/				
				#if defined(SD_MULTI_THREADED)
				ENTER_CRITICAL_SECTION(driver->context.request_queue_lock);
				#endif
				/*
				// find the end of the queue
				*/
				i = 0;
				req = &driver->context.request_queue;
				while ((*req))
				{
					i++;
					req = &(*req)->next;
				}
				/*
				// if the queue is full we must wait for an
				// open slot
				*/
				if (i >= SD_ASYNC_QUEUE_LIMIT)
				{
					#if defined(SD_MULTI_THREADED)
					LEAVE_CRITICAL_SECTION(driver->context.request_queue_lock);
					#endif
					sd_idle_processing(driver);
					continue;
				}
				/*
				// allocate memory for the request
				*/
				#if defined(SD_ALLOCATE_QUEUE_FROM_HEAP)
				(*req) = malloc(sizeof(SD_ASYNC_REQUEST));
				if (!(*req))
				{
					#if defined(SD_MULTI_THREADED)
					LEAVE_CRITICAL_SECTION(driver->context.request_queue_lock);
					#endif
					sd_idle_processing(driver);
					continue;
				}
				#else
				for (i = 0; i < SD_ASYNC_QUEUE_LIMIT; i++)
				{
					if (driver->context.request_queue_slots[i].mode == SD_FREE_REQUEST)
					{
						*req = &driver->context.request_queue_slots[i];
						break;
					}
				}
				#endif
				/*
				// copy request parameters to queue
				*/
				(*req)->address = address;
				(*req)->buffer = buffer;
				(*req)->async_state = async_state;
				(*req)->callback_info = callback_info;
				(*req)->mode = SD_READ_REQUEST;
				(*req)->next = 0;
				/*
				// unlock the queue
				*/
				#if defined(SD_MULTI_THREADED)
				LEAVE_CRITICAL_SECTION(driver->context.request_queue_lock);
				#endif
				/*
				// set the state to SD_PROCESSING
				*/
				*async_state = SD_PROCESSING;
				/*
				// return control to caller
				*/
				return *async_state;
			}
		}
		#endif
		/*
		// if this request doesn't come from the queue we need
		// to take ownership of the device.
		*/
		if (!from_queue)
		{		
			#if defined(SD_MULTI_THREADED)
			ENTER_CRITICAL_SECTION(driver->context.busy_lock);
			#endif
			while (driver->context.busy)
			{
				#if defined(SD_MULTI_THREADED)
				LEAVE_CRITICAL_SECTION(driver->context.busy_lock);
				#endif
				sd_idle_processing(driver);
				#if defined(SD_MULTI_THREADED)
				ENTER_CRITICAL_SECTION(driver->context.busy_lock);
				#endif
			}	
			/*
			// mark the driver as busy
			*/
			driver->context.busy = 1;
			#if defined(SD_MULTI_THREADED)
			LEAVE_CRITICAL_SECTION(driver->context.busy_lock);
			#endif
		}	
		#if defined(SD_PRINT_DEBUG_INFO)
		printf("SDDRIVER: Async Single Block Read @ Address: 0x%lx\r\n", address);
		#endif
		/*
		// set busy signal
		*/
		BP_SET(driver->context.busy_signal);
		/*
		// assert the CS line
		*/
		ASSERT_CS(driver);
		/*
		// send the read command
		*/	
		SEND_IO_COMMAND(driver->context.spi_module, READ_SINGLE_BLOCK, address);
		/*
		// read the response
		*/
		WAIT_FOR_RESPONSE(driver, tmp);
		/*
		// check for timeout condition
		*/
		if (driver->context.timeout) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			driver->context.busy = 0;
			return SD_TIMEOUT;	
		}
		/*
		// translate the response
		*/
		tmp = TRANSLATE_RESPONSE(tmp);
		/*
		// check for an error response
		*/
		if (tmp != 0) 
		{
			spi_write(driver->context.spi_module, 0xFF);
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			driver->context.busy = 0;
			return tmp;			
		}
		/*
		// wait for the start token
		*/
		WAIT_FOR_DATA(driver);
		/*
		// check for timeout condition
		*/
		if (driver->context.timeout) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			driver->context.busy = 0;
			return SD_TIMEOUT;	
		}
		/*
		// setup up the dma transfer
		*/
		driver->context.dma_transfer_state = SD_BLOCK_READ_IN_PROGRESS;
		driver->context.dma_transfer_completed = 0;
		driver->context.dma_transfer_result = async_state;
		driver->context.dma_transfer_callback = callback_info->callback;
		driver->context.dma_transfer_callback_context = callback_info->context;
		/*
		// set dma buffer
		*/
		if (driver->context.async_buffer)
		{
			driver->context.dma_transfer_buffer = buffer;
			DMA_SET_BUFFER_A(driver->context.dma_channel_2, driver->context.async_buffer);
			DMA_SET_BUFFER_B(driver->context.dma_channel_2, driver->context.async_buffer);
		}
		else
		{
			driver->context.dma_transfer_buffer = 0;
			DMA_SET_BUFFER_A(driver->context.dma_channel_2, buffer);
			DMA_SET_BUFFER_B(driver->context.dma_channel_2, buffer);
		}
		/*
		// enable the dma channels
		*/
		DMA_SET_CHANNEL_DIRECTION(driver->context.dma_channel_1, DMA_DIR_RAM_TO_PERIPHERAL);
		DMA_SET_CHANNEL_DIRECTION(driver->context.dma_channel_2, DMA_DIR_PERIPHERAL_TO_RAM);
		DMA_CHANNEL_DISABLE_INTERRUPT(driver->context.dma_channel_1);
		DMA_CHANNEL_ENABLE_INTERRUPT(driver->context.dma_channel_2);
		DMA_CHANNEL_ENABLE(driver->context.dma_channel_1);
		DMA_CHANNEL_ENABLE(driver->context.dma_channel_2);
		/*
		// force the 1st byte transfer
		*/
		DMA_CHANNEL_FORCE_TRANSFER(driver->context.dma_channel_1);
		/*
		// set the state to SD_PROCESSING
		*/
		*async_state = SD_PROCESSING;
		/*
		// return the current state ( processing )
		*/
		return *async_state;
	}
	/*
	// if this is a synchronous read
	*/
	else 
	{
		#if defined(SD_PRINT_DEBUG_INFO)
		printf("SDDRIVER: Single Block Read @ Address: 0x%lx\r\n", address);
		#endif
		/*
		// take ownership of device
		*/
		#if defined(SD_MULTI_THREADED)
		ENTER_CRITICAL_SECTION(driver->context.busy_lock);
		#endif
		while (driver->context.busy)
		{
			#if defined(SD_MULTI_THREADED)
			LEAVE_CRITICAL_SECTION(driver->context.busy_lock);
			#endif
			sd_idle_processing(driver);
			#if defined(SD_MULTI_THREADED)
			ENTER_CRITICAL_SECTION(driver->context.busy_lock);
			#endif
		}	
		/*
		// mark the driver as busy
		*/
		driver->context.busy = 1;
		#if defined(SD_MULTI_THREADED)
		LEAVE_CRITICAL_SECTION(driver->context.busy_lock);
		#endif
		/*
		// set busy signal
		*/
		BP_SET(driver->context.busy_signal);
		/*
		// assert the CS line
		*/
		ASSERT_CS(driver);
		/*
		// send the read command
		*/	
		SEND_IO_COMMAND(driver->context.spi_module, READ_SINGLE_BLOCK, address);
		/*
		// read the response
		*/
		WAIT_FOR_RESPONSE(driver, tmp);
		/*
		// check for timeout condition
		*/
		if (driver->context.timeout) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			driver->context.busy = 0;
			return SD_TIMEOUT;	
		}
		/*
		// translate the response
		*/
		tmp = TRANSLATE_RESPONSE(tmp);
		/*
		// check for an error response
		*/
		if (tmp != 0) 
		{
			spi_write(driver->context.spi_module, 0xFF);
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			driver->context.busy = 0;
			return tmp;			
		}
		/*
		// wait for the start token
		*/
		WAIT_FOR_DATA(driver);
		/*
		// check for timeout condition
		*/
		if (driver->context.timeout) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			driver->context.busy = 0;
			return SD_TIMEOUT;	
		}
		/*
		// copy incoming data to buffer
		*/
		/*
		for ( i = 0; i < SD_BLOCK_LENGTH; i++ ) 
		{
			*buffer++ = spi_read(driver->context.spi_module);
		}
		*/
		spi_read_buffer(driver->context.spi_module, buffer, SD_BLOCK_LENGTH);
		/*
		// read and discard the 16-bit crc
		*/
		tmp = spi_read(driver->context.spi_module);
		tmp = spi_read(driver->context.spi_module);
		/*
		// send 8 clock cycles
		*/
		spi_write(driver->context.spi_module, 0xFF);
		/*
		// de-assert the CS line
		*/
		DEASSERT_CS(driver);
		/*
		// mark the driver as not busy
		*/
		driver->context.busy = 0;
		/*
		// clear busy signal
		*/
		BP_CLR(driver->context.busy_signal);
		/*
		// return success code
		*/
		return SD_SUCCESS;
	}
}

uint16_t sd_erase
(
	SD_DRIVER* driver, 
	uint32_t start_address,
	uint32_t end_address,
	char from_queue
)
{
	unsigned char tmp;
	/*
	// if the card is not ready return error
	*/
	if (BP_GET(driver->context.media_ready))
	{
		/*
		if (from_queue)
		{
			driver->context.busy = 0;
		}
		*/
		return SD_CARD_NOT_READY;
	}
	/*
	// set busy signal
	*/
	BP_SET(driver->context.busy_signal);
	/*
	// assert the CS line
	*/
	ASSERT_CS(driver);
	/*
	// send 16 clock pulses to the card
	*/
	spi_write(driver->context.spi_module, 0xFF);
	spi_write(driver->context.spi_module, 0xFF);
	/*
	// send the start block
	*/	
	SEND_IO_COMMAND(driver->context.spi_module, ERASE_WR_BLK_START, start_address);
	/*
	// read the response
	*/
	WAIT_FOR_RESPONSE(driver, tmp);
	/*
	// check for timeout condition
	*/
	if (driver->context.timeout) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		driver->context.busy = 0;
		return SD_TIMEOUT;	
	}
	/*
	// checks for an error code on the response
	*/
	if (tmp) 
	{		
		spi_write(driver->context.spi_module, 0xFF);
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		driver->context.busy = 0;
		return TRANSLATE_RESPONSE(tmp);	
	}
	/*
	// send 16 clock pulses to the card
	*/
	spi_write(driver->context.spi_module, 0xFF);
	spi_write(driver->context.spi_module, 0xFF);
	/*
	// send the end block
	*/	
	SEND_IO_COMMAND(driver->context.spi_module, ERASE_WR_BLK_END, end_address);
	/*
	// read the response
	*/
	WAIT_FOR_RESPONSE(driver, tmp);
	/*
	// check for timeout condition
	*/
	if (driver->context.timeout) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		driver->context.busy = 0;
		return SD_TIMEOUT;	
	}
	/*
	// checks for an error code on the response
	*/
	if (tmp) 
	{		
		spi_write(driver->context.spi_module, 0xFF);
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		driver->context.busy = 0;
		return TRANSLATE_RESPONSE(tmp);	
	}
	/*
	// send 16 clock pulses to the card
	*/
	spi_write(driver->context.spi_module, 0xFF);
	spi_write(driver->context.spi_module, 0xFF);
	/*
	// send the write command
	*/	
	SEND_COMMAND(driver->context.spi_module, ERASE);
	/*
	// read the response
	*/
	WAIT_FOR_RESPONSE(driver, tmp);
	/*
	// check for timeout condition
	*/
	if (driver->context.timeout) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		driver->context.busy = 0;
		return SD_TIMEOUT;	
	}
	/*
	// checks for an error code on the response
	*/
	if (tmp) 
	{		
		spi_write(driver->context.spi_module, 0xFF);
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		driver->context.busy = 0;
		return TRANSLATE_RESPONSE(tmp);	
	}
	/*
	// wait
	*/
	WAIT_WHILE_CARD_BUSY(driver->context.spi_module);
	/*
	// send 16 clock pulses to the card
	*/
	spi_write(driver->context.spi_module, 0xFF);
	spi_write(driver->context.spi_module, 0xFF);
	/*
	// de-assert CS
	*/
	DEASSERT_CS(driver);
	/*
	// clear busy signal
	*/
	BP_CLR(driver->context.busy_signal);
	/*
	// release device
	*/
	driver->context.busy = 0;
	/*
	// return success code
	*/
	return SD_SUCCESS;	
}

/*
// Writes a block to an SD card.
*/
uint16_t sd_write
( 
	SD_DRIVER* driver,	
	unsigned long address, 
	unsigned char* buffer, 
	uint16_t* async_state, 
	SD_CALLBACK_INFO* callback_info,
	char from_queue 
) 
{
	unsigned char tmp;
	uint16_t i;	
	/*
	// if the card is not ready return error
	*/
	if (BP_GET(driver->context.media_ready))
	{
		if (from_queue)
		{
			driver->context.busy = 0;
		}
		return SD_CARD_NOT_READY;
	}
	/*
	// if this is an async request and it doesn't come
	// from the queue we'll just enqueue it and return
	// for all other requests we'll wait for the device
	// to become available and continue
	*/
	#if defined(SD_QUEUE_ASYNC_REQUESTS)
	if (async_state && !from_queue)
	{
		while (1) 
		{
			SD_ASYNC_REQUEST** req;
			/*
			// lock the queue
			*/				
			#if defined(SD_MULTI_THREADED)
			ENTER_CRITICAL_SECTION(driver->context.request_queue_lock);
			#endif
			/*
			// find the end of the queue
			*/
			i = 0;
			req = &driver->context.request_queue;
			while ((*req))
			{
				i++;
				req = &(*req)->next;
			}
			/*
			// if the queue is full we must wait for an
			// open slot
			*/
			if (i >= SD_ASYNC_QUEUE_LIMIT)
			{
				#if defined(SD_MULTI_THREADED)
				LEAVE_CRITICAL_SECTION(driver->context.request_queue_lock);
				#endif
				sd_idle_processing(driver);
				continue;
			}
			/*
			// allocate memory for the request
			*/
			#if defined(SD_ALLOCATE_QUEUE_FROM_HEAP)
			(*req) = malloc(sizeof(SD_ASYNC_REQUEST));
			if (!(*req))
			{
				#if defined(SD_MULTI_THREADED)
				LEAVE_CRITICAL_SECTION(driver->context.request_queue_lock);
				#endif
				sd_idle_processing(driver);
				continue;
			}
			#else
			for (i = 0; i < SD_ASYNC_QUEUE_LIMIT; i++)
			{ 
				if (driver->context.request_queue_slots[i].mode == SD_FREE_REQUEST)
				{
					*req = &driver->context.request_queue_slots[i];
					break;
				}
			}
			#endif
			/*
			// copy request parameters to queue
			*/
			(*req)->address = address;
			(*req)->buffer = buffer;
			(*req)->async_state = async_state;
			(*req)->callback_info = callback_info;
			(*req)->mode = SD_WRITE_REQUEST;
			(*req)->next = 0;
			/*
			// unlock the queue
			*/
			#if defined(SD_MULTI_THREADED)
			LEAVE_CRITICAL_SECTION(driver->context.request_queue_lock);
			#endif
			/*
			// set the state to SD_PROCESSING
			*/
			*async_state = SD_PROCESSING;
			/*
			// return control to caller
			*/
			return *async_state;
		}
	}
	#endif
	/*
	// if this request doesn't come from the queue we need
	// to take ownership of the device.
	*/
	if (!from_queue)
	{		
		#if defined(SD_MULTI_THREADED)
		ENTER_CRITICAL_SECTION(driver->context.busy_lock);
		#endif
		while (driver->context.busy)
		{
			#if defined(SD_MULTI_THREADED)
			LEAVE_CRITICAL_SECTION(driver->context.busy_lock);
			#endif
			sd_idle_processing(driver);
			#if defined(SD_MULTI_THREADED)
			ENTER_CRITICAL_SECTION(driver->context.busy_lock);
			#endif
		}	
		/*
		// mark the driver as busy
		*/
		driver->context.busy = 1;
		#if defined(SD_MULTI_THREADED)
		LEAVE_CRITICAL_SECTION(driver->context.busy_lock);
		#endif
	}
	
	/*
	// set busy signal
	*/
	BP_SET(driver->context.busy_signal);
	/*
	// assert the CS line
	*/
	ASSERT_CS(driver);
	/*
	// send 16 clock pulses to the card
	*/
	spi_write(driver->context.spi_module, 0xFF);
	spi_write(driver->context.spi_module, 0xFF);
	/*
	// send the write command
	*/	
	SEND_IO_COMMAND(driver->context.spi_module, WRITE_SINGLE_BLOCK, address);
	/*
	// read the response
	*/
	WAIT_FOR_RESPONSE(driver, tmp);
	/*
	// check for timeout condition
	*/
	if (driver->context.timeout) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		driver->context.busy = 0;
		return SD_TIMEOUT;	
	}
	/*
	// translate the response
	*/
	tmp = TRANSLATE_RESPONSE(tmp);
	/*
	// checks for an error code on the response
	*/
	if (tmp) 
	{		
		spi_write(driver->context.spi_module, 0xFF);
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		driver->context.busy = 0;
		return tmp;	
	}
	/*
	// if this is an asynchronous write...
	*/		
	if (async_state) 
	{
		#if defined(SD_PRINT_DEBUG_INFO)
		printf("SDDRIVER: Async Single Block Write @ Address: 0x%lx\r\n", address);
		#endif
		/*
		// set up the dma transfer
		*/
		driver->context.dma_transfer_completed = 0;
		driver->context.dma_transfer_state = SD_BLOCK_WRITE_IN_PROGRESS;
		driver->context.dma_transfer_result = async_state;
		driver->context.dma_transfer_callback = callback_info->callback;
		driver->context.dma_transfer_callback_context = callback_info->context;
		/*
		// send the start token and let the
		// DMA do the rest
		*/
		spi_write(driver->context.spi_module, SD_BLOCK_START_TOKEN);
		/*
		// set dma buffer
		*/
		if (driver->context.async_buffer)
		{
			#if defined(__C30__)
			__asm__
			(
				"mov %0, w1\n"
				"mov %1, w2\n"
				"repeat #%2\n"
				"mov [w1++], [w2++]"
				:
				: "g" (buffer), "g" (driver->context.async_buffer), "i" ((SD_BLOCK_LENGTH / 2) - 1)
				: "w1", "w2"
			);
			#else
			for (i = 0; i < (SD_BLOCK_LENGTH / 2); i++)
			{
				((uint16_t*) driver->context.async_buffer)[i] = ((uint16_t*) buffer)[i];
			}
			#endif
			/*
			for (i = 0; i < SD_BLOCK_LENGTH; i++)
			{
				driver->context.async_buffer[i] = buffer[i];
			}
			*/
			DMA_SET_BUFFER_A(driver->context.dma_channel_2, driver->context.async_buffer);
		}
		else
		{
			DMA_SET_BUFFER_A(driver->context.dma_channel_2, buffer);
		}
		/*
		// enable the dma channels
		*/
		DMA_SET_CHANNEL_DIRECTION(driver->context.dma_channel_1, DMA_DIR_PERIPHERAL_TO_RAM);
		DMA_SET_CHANNEL_DIRECTION(driver->context.dma_channel_2, DMA_DIR_RAM_TO_PERIPHERAL);
		DMA_CHANNEL_ENABLE_INTERRUPT(driver->context.dma_channel_1);
		DMA_CHANNEL_DISABLE_INTERRUPT(driver->context.dma_channel_2);
		DMA_CHANNEL_ENABLE(driver->context.dma_channel_1);
		DMA_CHANNEL_ENABLE(driver->context.dma_channel_2);
		/*
		// force the 1st byte transfer
		*/
		DMA_CHANNEL_FORCE_TRANSFER(driver->context.dma_channel_2);
		/*
		// set the state to SD_PROCESSING and relinquish control
		*/
		*async_state = SD_PROCESSING;
		/*
		// return processing code
		*/
		return *async_state;
	}
	/*
	// if this is a synchronous write
	*/
	else 
	{
		#if defined(SD_PRINT_DEBUG_INFO)
		printf("SDDRIVER: Single Block Write @ Address: 0x%lx\r\n", address);
		#endif
		/*
		// send the start token
		*/
		spi_write(driver->context.spi_module, SD_BLOCK_START_TOKEN);
		/*
		// transfer the block of data & crc
		*/
		/*
		for ( i = 0; i < SD_BLOCK_LENGTH; i++ )	
		{		
			spi_write(driver->context.spi_module, buffer[i]);
		}
		*/
		spi_write_buffer(driver->context.spi_module, buffer, SD_BLOCK_LENGTH);
		/*
		// write the 16-bit CRC
		*/
		/*spi_write(driver->context.spi_module, 0x0);
		spi_write(driver->context.spi_module, 0X1);*/
		/*
		// read the data response
		*/
		WAIT_FOR_DATA_RESPONSE_TOKEN(driver, tmp);
		/*
		// check for timeout conditon
		*/
		if (driver->context.timeout) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			driver->context.busy = 0;
			return SD_TIMEOUT;	
		}
		/*
		// if the data was accepted then wait until
		// the card is done writing it
		*/
		if (SD_DATA_RESPONSE_ACCEPTED == (tmp & SD_DATA_RESPONSE_ACCEPTED)) 
		{
			WAIT_WHILE_CARD_BUSY(driver->context.spi_module);
		}
		/*
		// if the data was rejected due to a CRC error
		// return the CRC error code
		*/		
		else if (SD_DATA_RESPONSE_REJECTED_CRC_ERROR == (tmp & SD_DATA_RESPONSE_REJECTED_CRC_ERROR)) 
		{
			spi_write(driver->context.spi_module, 0xFF);
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			driver->context.busy = 0;
			return SD_CRC_ERROR;
		}
		/*
		// if it was rejected due to a write error
		// return the unknown error code ( for now )
		*/
		else if (SD_DATA_RESPONSE_REJECTED_WRITE_ERROR == (tmp & SD_DATA_RESPONSE_REJECTED_WRITE_ERROR)) 
		{
			spi_write(driver->context.spi_module, 0xFF);
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			driver->context.busy = 0;
			return SD_UNKNOWN_ERROR;		
		}
		/*
		// send the SEND_STATUS command
		*/
		SEND_COMMAND(driver->context.spi_module, SEND_STATUS);
		/*
		// wait for the response
		*/
		WAIT_FOR_RESPONSE(driver, tmp);
		/*
		// check for timeout condition
		*/
		if (driver->context.timeout) 
		{
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			return SD_TIMEOUT;	
		}
		/*
		// translate the response
		*/
		tmp = TRANSLATE_RESPONSE(tmp);
		/*
		// check for an error response
		*/
		if (tmp) 
		{
			spi_write(driver->context.spi_module, 0xFF);
			DEASSERT_CS(driver);
			BP_CLR(driver->context.busy_signal);
			return tmp;		
		}
		
		tmp = spi_read(driver->context.spi_module);
		
		
		if (tmp != 0)
		{
			HALT();
		
		}
		
		spi_write(driver->context.spi_module, 0xFF);
		
		/*
		// if the card is still busy
		// the time slice
		*/
		/*
		//tmp = spi_read(driver->context.spi_module);
		//if  (tmp == 0x0)
		//	return;
		*/
		/*
		// clock out 8 cycles
		*/
		spi_write(driver->context.spi_module,  0xFF );
		/*
		// de-assert the CS line
		*/
		DEASSERT_CS(driver);
		/*
		// mark the card as not busy
		*/
		driver->context.busy = 0;
		/*
		// clear busy signal
		*/
		BP_CLR(driver->context.busy_signal);
		/*
		// return success code
		*/
		return SD_SUCCESS;		
	}
}

/*
// writes multiple blocks to SD card
*/
#if defined(SD_ENABLE_MULTI_BLOCK_WRITE)
uint16_t sd_write_multiple_blocks
( 
	SD_DRIVER* driver,	
	unsigned long address, 
	unsigned char* buffer, 
	uint16_t* async_state, 
	SD_CALLBACK_INFO_EX* callback_info,
	char from_queue,
	char needs_data 
) 
{
	
	unsigned char tmp;
	uint16_t i;	
	/*
	// if the card is not ready return error
	*/
	if (BP_GET(driver->context.media_ready))
	{
		if (from_queue)
		{
			driver->context.busy = 0;
		}
		return SD_CARD_NOT_READY;
	}
	/*
	// if this is an async request and it doesn't come
	// from the queue we'll just enqueue it and return
	// for all other requests we'll wait for the device
	// to become available and continue
	*/
	#if defined(SD_QUEUE_ASYNC_REQUESTS)
	if (!from_queue)
	{
enqueue_request:
		while (1) 
		{
			SD_ASYNC_REQUEST** req;
			/*
			// lock the queue
			*/				
			#if defined(SD_MULTI_THREADED)
			ENTER_CRITICAL_SECTION(driver->context.request_queue_lock);
			#endif
			/*
			// find the end of the queue
			*/
			i = 0;
			req = &driver->context.request_queue;
			while ((*req))
			{
				i++;
				req = &(*req)->next;
			}
			/*
			// if the queue is full we must wait for an
			// open slot
			*/
			if (i >= SD_ASYNC_QUEUE_LIMIT)
			{
				#if defined(SD_MULTI_THREADED)
				LEAVE_CRITICAL_SECTION(driver->context.request_queue_lock);
				#endif
				sd_idle_processing(driver);
				continue;
			}
			/*
			// allocate memory for the request
			*/
			#if defined(SD_ALLOCATE_QUEUE_FROM_HEAP)
			(*req) = malloc(sizeof(SD_ASYNC_REQUEST));
			if (!(*req))
			{
				#if defined(SD_MULTI_THREADED)
				LEAVE_CRITICAL_SECTION(driver->context.request_queue_lock);
				#endif
				sd_idle_processing(driver);
				continue;
			}
			#else
			for (i = 0; i < SD_ASYNC_QUEUE_LIMIT + (needs_data ? 1 : 0); i++)
			{
				if (driver->context.request_queue_slots[i].mode == SD_FREE_REQUEST)
				{
					*req = &driver->context.request_queue_slots[i];
					break;
				}
			}
			#endif
			/*
			// copy request parameters to queue
			*/
			(*req)->address = address;
			(*req)->buffer = buffer;
			(*req)->async_state = async_state;
			(*req)->callback_info_ex = callback_info;
			(*req)->mode = SD_MULTI_BLOCK_WRITE_REQUEST;
			(*req)->next = 0;

			(*req)->callback_info_ex->callback = callback_info->callback;
			(*req)->callback_info_ex->context = callback_info->context;
			(*req)->needs_data = needs_data;
			/*
			// unlock the queue
			*/
			#if defined(SD_MULTI_THREADED)
			LEAVE_CRITICAL_SECTION(driver->context.request_queue_lock);
			#endif
			/*
			// set the state to SD_PROCESSING
			*/
			*async_state = SD_PROCESSING;
			/*
			// return control to caller
			*/
			return *async_state;
		}
	}
	#endif
	/*
	// if data is needed requested from the user code
	*/
	if (needs_data)
	{
		/*
		// set the result of the operation to AWAITING_DATA
		*/
		*driver->context.dma_transfer_result = SD_AWAITING_DATA;
		/*
		// set the response to the default STOP so that if user
		// doesn't handle callback the transfer gets stopped.]
		*/
		driver->context.dma_transfer_response = SD_MULTI_BLOCK_RESPONSE_STOP;
		/*
		// ask filesystem how to proceed
		*/
		if (driver->context.dma_transfer_callback_ex)
		{
			driver->context.dma_transfer_callback_ex(
				driver->context.dma_transfer_callback_context, 
				driver->context.dma_transfer_result,
				&driver->context.dma_transfer_buffer,
				&driver->context.dma_transfer_response
				);
		}	
		
		if (driver->context.dma_transfer_response == SD_MULTI_BLOCK_RESPONSE_SKIP)	
		{
			driver->context.busy = 0;
			goto enqueue_request;
		}
		else if (driver->context.dma_transfer_response == SD_MULTI_BLOCK_RESPONSE_STOP)
		{

			/* 
			// TODO: Instead of calling the callback right here
			// we need to set the state machine so that it gets called
			// the next time the processing loop runs
			*/
			driver->context.dma_transfer_state = SD_MULTI_BLOCK_WRITE_COMPLETING;
			driver->context.dma_transfer_completed = 1;
			return *async_state;
			
			#if defined(COMMENTED)			
			/*
			// set the operation state
			*/
			*driver->context.dma_transfer_result = SD_SUCCESS;
			/*
			// call the user/filesystem callback routine
			*/
			if (driver->context.dma_transfer_callback_ex != 0) 
			{
				driver->context.dma_transfer_callback_ex(
					driver->context.dma_transfer_callback_context, 
					driver->context.dma_transfer_result,
					&driver->context.dma_transfer_buffer,
					&driver->context.dma_transfer_response
					);
			}

			driver->context.busy = 0;
			/*
			// set the state to SD_PROCESSING
			*/
			*async_state = SD_SUCCESS;
			/*
			// return control to caller
			*/
			return *async_state;
			#endif
		}
		else if (driver->context.dma_transfer_response != SD_MULTI_BLOCK_RESPONSE_READY)
		{
			driver->context.busy = 0;
			/*
			// set the state to SD_PROCESSING
			*/
			*async_state = SD_INVALID_MULTI_BLOCK_RESPONSE;
			/*
			// return control to caller
			*/
			return *async_state;
		
		}
	}
	/*
	// if this request doesn't come from the queue we need
	// to take ownership of the device.
	*/
	if (!from_queue)
	{		
		#if defined(SD_MULTI_THREADED)
		ENTER_CRITICAL_SECTION(driver->context.busy_lock);
		#endif
		while (driver->context.busy)
		{
			#if defined(SD_MULTI_THREADED)
			LEAVE_CRITICAL_SECTION(driver->context.busy_lock);
			#endif
			sd_idle_processing(driver);
			#if defined(SD_MULTI_THREADED)
			ENTER_CRITICAL_SECTION(driver->context.busy_lock);
			#endif
		}	
		/*
		// mark the driver as busy
		*/
		driver->context.busy = 1;
		#if defined(SD_MULTI_THREADED)
		LEAVE_CRITICAL_SECTION(driver->context.busy_lock);
		#endif
	}	
	/*
	// set busy signal
	*/
	BP_SET(driver->context.busy_signal);
	/*
	// assert the CS line
	*/
	ASSERT_CS(driver);
	/*
	// send 16 clock pulses to the card
	*/
	spi_write(driver->context.spi_module, 0xFF);
	spi_write(driver->context.spi_module, 0xFF);
	/*
	// send the write command
	*/	
	SEND_IO_COMMAND(driver->context.spi_module, WRITE_MULTIPLE_BLOCK, address);
	/*
	// read the response
	*/
	WAIT_FOR_RESPONSE(driver, tmp);
	/*
	// check for timeout condition
	*/
	if (driver->context.timeout) 
	{
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		driver->context.busy = 0;
		return SD_TIMEOUT;	
	}
	/*
	// translate the response
	*/
	tmp = TRANSLATE_RESPONSE(tmp);
	/*
	// checks for an error code on the response
	*/
	if (tmp) 
	{		
		spi_write(driver->context.spi_module, 0xFF);
		DEASSERT_CS(driver);
		BP_CLR(driver->context.busy_signal);
		driver->context.busy = 0;
		return tmp;	
	}
	/*
	// set up the dma transfer
	*/
	driver->context.dma_transfer_skip_callback = 0;
	driver->context.dma_transfer_address = address;
	driver->context.dma_transfer_completed = 0;
	driver->context.dma_transfer_buffer = buffer;
	driver->context.dma_transfer_state = SD_MULTI_BLOCK_WRITE_IN_PROGRESS;
	driver->context.dma_transfer_result = async_state;
	driver->context.dma_transfer_callback_ex = callback_info->callback;
	driver->context.dma_transfer_callback_context = callback_info->context;
	driver->context.dma_transfer_callback_info_ex = callback_info;
	/*
	// reset block count
	*/
	#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
	driver->context.dma_transfer_block_count = 0;
	driver->context.dma_transfer_total_blocks = 0;
	driver->context.dma_transfer_program_time_s = 0;
	driver->context.dma_transfer_program_time_ms = 0;
	driver->context.dma_transfer_overhead_time_s = 0;
	driver->context.dma_transfer_overhead_time_ms = 0;
	rtc_timer(&driver->context.dma_transfer_time_s, &driver->context.dma_transfer_time_ms);
	printf("-------------------------\r\n");
	#endif
	#if defined(SD_PRINT_DEBUG_INFO) || defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
	printf("SDDRIVER: Multi-block Write @ Address: 0x%lx\r\n", address);
	#endif
	
	/*
	// calculate the # of bytes remaining in the RU
	*/
	if (driver->card_info.high_capacity)
	{
		driver->context.dma_transfer_blocks_remaining = 
			driver->card_info.ru_size - (address % driver->card_info.ru_size);
	}		
	else
	{
		driver->context.dma_transfer_blocks_remaining =
			driver->card_info.ru_size - ((address / 512) % driver->card_info.ru_size);
	}
	/*
	// send the start token and let the
	// DMA do the rest
	*/
	spi_write(driver->context.spi_module, SD_BLOCK_START_TOKEN_MULT);
	/*
	// set dma buffer
	*/
	if (driver->context.async_buffer)
	{
		#if defined(__C30__)
		__asm__
		(
			"mov %0, w1\n"
			"mov %1, w2\n"
			"repeat #%2\n"
			"mov [w1++], [w2++]\n"
			:
			: "g" (driver->context.dma_transfer_buffer), "g" (driver->context.async_buffer), "i" ((SD_BLOCK_LENGTH / 2) - 1)
			: "w1", "w2"
		);
		#else
		for (i = 0; i < (SD_BLOCK_LENGTH / 2); i++)
		{
			((uint16_t*) driver->context.async_buffer)[i] = ((uint16_t*) driver->context.dma_transfer_buffer)[i];
		}
		#endif
		/*
		//for (i = 0; i < SD_BLOCK_LENGTH; i++)
		//{
		//	driver->context.async_buffer[i] = buffer[i];
		//}
		*/

		DMA_SET_BUFFER_A(driver->context.dma_channel_2, driver->context.async_buffer);
	}
	else
	{
		DMA_SET_BUFFER_A(driver->context.dma_channel_2, buffer);
	}
	/*
	// enable the dma channels
	*/
	DMA_SET_CHANNEL_DIRECTION(driver->context.dma_channel_1, DMA_DIR_PERIPHERAL_TO_RAM);
	DMA_SET_CHANNEL_DIRECTION(driver->context.dma_channel_2, DMA_DIR_RAM_TO_PERIPHERAL);
	DMA_CHANNEL_ENABLE_INTERRUPT(driver->context.dma_channel_1);
	DMA_CHANNEL_DISABLE_INTERRUPT(driver->context.dma_channel_2);
	DMA_CHANNEL_ENABLE(driver->context.dma_channel_1);
	DMA_CHANNEL_ENABLE(driver->context.dma_channel_2);
	/*
	// save the start time of the multi-block transfer
	*/
	#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
	rtc_timer(&driver->context.dma_transfer_bus_time_s, &driver->context.dma_transfer_bus_time_ms);
	#endif
	/*
	// force the 1st byte transfer
	*/
	DMA_CHANNEL_FORCE_TRANSFER(driver->context.dma_channel_2);
	/*
	// set the state to SD_PROCESSING and relinquish control
	*/
	*async_state = SD_PROCESSING;
	/*
	// return processing code
	*/
	return *async_state;
}
#endif

/*
// performs driver background processing
*/
void sd_idle_processing(SD_DRIVER* sd) 
{
	static uint16_t i;
	static unsigned char tmp;
	/*
	// if a card has been inserted of removed remember
	// the new state and call notification function
	*/
	if (sd->context.last_media_ready != BP_GET(sd->context.media_ready))
	{
		/*
		// give the card some time to power up
		*/
		for (i = 0; i < 0xFFFF; i++)
		{	
			#if defined(__C30__)
			__asm__("nop");
			#else
			tmp = i;
			#endif
		}	
		/*
		// check that the change wasn't spurious
		*/
		if (sd->context.last_media_ready != BP_GET(sd->context.media_ready))
		{
			/*
			// try to mount the card
			*/
			if (!BP_GET(sd->context.media_ready))
			{
				i = sd_init_internal(sd);
			}
			else
			{
				i = 1;
			}	
			/*
			// update the state
			*/
			sd->context.last_media_ready = BP_GET(sd->context.media_ready);
			/*
			// if any notification requests are registered
			// call them back
			*/
			if (sd->context.media_changed_callback)
			{
				sd->context.media_changed_callback(sd->context.id, i == SD_SUCCESS);
			}
		}
	}
	/*
	// if an async op has comleted
	*/
	if (sd->context.dma_transfer_completed) 
	{
		switch (sd->context.dma_transfer_state)
		{
			case SD_BLOCK_READ_IN_PROGRESS:
				/*
				// reset the dma transfer completed flag
				*/
				sd->context.dma_transfer_completed = 0;
				/*
				// read and discard the 16-bit crc
				*/
				tmp = spi_read(sd->context.spi_module);
				tmp = spi_read(sd->context.spi_module);
				/*
				// send 8 clock cycles
				*/
				spi_write(sd->context.spi_module, 0xFF);
				/*
				// de-assert the CS line
				*/
				DEASSERT_CS(sd);
				/*
				// if we're using a dedicated buffer for DMA transfers
				// then copy the data to the user buffer
				*/
				if (sd->context.dma_transfer_buffer)
				{
					#if defined(__C30__)
					__asm__
					(
						"mov %0, w1\n"
						"mov %1, w2\n"
						"repeat #%2\n"
						"mov [w1++], [w2++]"
						:
						: "g" (sd->context.async_buffer), "g" (sd->context.dma_transfer_buffer), "i" ((SD_BLOCK_LENGTH / 2) - 1)
						: "w1", "w2"
					);
					#else
					for (i = 0; i < (SD_BLOCK_LENGTH / 2); i++)
					{
						((uint16_t*) sd->context.dma_transfer_buffer)[i] = ((uint16_t*) sd->context.async_buffer)[i];
					}
					#endif
					/*
					for (i = 0; i < SD_BLOCK_LENGTH; i++)
					
						sd->context.dma_transfer_buffer[i] = sd->context.async_buffer[i];
					}
					*/
				}
				/*
				// set the result code of the transfer
				*/
				*sd->context.dma_transfer_result = SD_SUCCESS;
				/*
				// clear busy signal
				*/
				BP_CLR(sd->context.busy_signal);
				/*
				// mark driver as not busy
				*/
				sd->context.busy = 0;
				/*
				// if a callback function was supplied call it
				*/
				if (sd->context.dma_transfer_callback != 0)
				{
					sd->context.dma_transfer_callback(
						sd->context.dma_transfer_callback_context, 
						sd->context.dma_transfer_result
						);
				}
				/*
				// return
				*/
				return;
				
			case SD_BLOCK_WRITE_IN_PROGRESS:
				/*
				// write the 16-bit CRC
				*/
				/*spi_write(sd->context.spi_module, 0x0);
				spi_write(sd->context.spi_module, 0X1); */
				/*
				// wait for the data transfer response token from the card
				*/
				WAIT_FOR_DATA_RESPONSE_TOKEN(sd, tmp);
				/*
				// if the operation times out...
				*/
				if (sd->context.timeout) 
				{
					/*
					// reset the dma op completed flag
					*/
					sd->context.dma_transfer_completed = 0;
					/*
					// send 8 clock cycles
					*/
					spi_write(sd->context.spi_module, 0xFF);
					/*
					// de-assert the CS line
					*/
					DEASSERT_CS(sd);
					/*
					// clear busy signal
					*/
					BP_CLR(sd->context.busy_signal);
					/*
					// mark driver as not busy
					*/
					sd->context.busy = 0;
					/*
					// set the operation state
					*/
					*sd->context.dma_transfer_result = SD_TIMEOUT;
					/*
					// if a callback routine was provided with the
					// operation request the call it.
					*/
					if (sd->context.dma_transfer_callback != NULL) 
					{
						sd->context.dma_transfer_callback(
							sd->context.dma_transfer_callback_context, 
							sd->context.dma_transfer_result
							);
					}
					/*
					// relinquish the time slice
					*/
					return;
				}
				
				/*
				// if the data was accepted then set transfer state to 2
				// we're now waiting for the SD card to finish programming
				*/
				if (SD_DATA_RESPONSE_ACCEPTED == (tmp & SD_DATA_RESPONSE_ACCEPTED)) 
				{
					sd->context.dma_transfer_state = SD_BLOCK_WRITE_PROGRAMMING;
					return;
				}
				/*
				// if the data was rejected due to a CRC error
				// return the CRC error code
				*/		
				else if (SD_DATA_RESPONSE_REJECTED_CRC_ERROR == (tmp & SD_DATA_RESPONSE_REJECTED_CRC_ERROR)) 
				{
					/*
					// reset the dma op completed flag
					*/
					sd->context.dma_transfer_completed = 0;
					/*
					// send 8 clock pulses to the card
					*/
					spi_write(sd->context.spi_module, 0xFF);
					/*
					// de-assert the CS line
					*/
					DEASSERT_CS(sd);
					/*
					// clear busy signal
					*/
					BP_CLR(sd->context.busy_signal);
					/*
					// mark driver as not busy
					*/
					sd->context.busy = 0;
					/*
					// set the operation state
					*/
					*sd->context.dma_transfer_result = SD_CRC_ERROR;
					/*
					// if a callback routine was provided with the
					// operation request the call it.
					*/
					if (sd->context.dma_transfer_callback != 0) 
					{
						sd->context.dma_transfer_callback(
							sd->context.dma_transfer_callback_context, 
							sd->context.dma_transfer_result
							);
					}
					/*
					// relinquish the time slot
					*/
					return;
				}
				/*
				// if it was rejected due to a write error
				// return the unknown error code ( for now )
				*/
				else if (SD_DATA_RESPONSE_REJECTED_WRITE_ERROR == (tmp & SD_DATA_RESPONSE_REJECTED_WRITE_ERROR)) 
				{
					/*
					// reset the dma op completed flag
					*/
					sd->context.dma_transfer_completed = 0;
					/*
					// send 8 clock pulses to the card
					*/
					spi_write(sd->context.spi_module, 0xFF);
					/*
					// de-assert the CS line
					*/
					DEASSERT_CS(sd);
					/*
					// clear busy signal
					*/
					BP_CLR(sd->context.busy_signal);
					/*
					// mark driver as not busy
					*/
					sd->context.busy = 0;
					/*
					// set the operation state
					*/
					*sd->context.dma_transfer_result = SD_UNKNOWN_ERROR;
					/*
					// if a callback routine was provided with the
					// operation request the call it.
					*/
					if (sd->context.dma_transfer_callback != 0) 
					{
						sd->context.dma_transfer_callback(
							sd->context.dma_transfer_callback_context, 
							sd->context.dma_transfer_result
							);
					}
					/*
					// relinquish the time slot
					*/
					return;
				}
				
			case SD_BLOCK_WRITE_PROGRAMMING:
			
				/*
				// if the card is still programming relinquish
				// the time slice
				*/
				tmp = spi_read(sd->context.spi_module);
				if  (tmp == 0x0)
					return;
				/*
				// reset the dma transfer completed flag
				*/
				sd->context.dma_transfer_completed = 0;
				/*
				// clock out 8 cycles
				*/
				spi_write(sd->context.spi_module, 0xFF);
				/*
				// de-assert the CS line
				*/
				DEASSERT_CS(sd);
				/*
				// set the operation state
				*/
				*sd->context.dma_transfer_result = SD_SUCCESS;
				/*
				// clear busy signal
				*/
				BP_CLR(sd->context.busy_signal);
				/*
				// mark device as not busy
				*/
				sd->context.busy = 0;
				/*
				// if a callback routine was provided with the
				// operation request the call it.
				*/
				if (sd->context.dma_transfer_callback != 0) 
				{
					sd->context.dma_transfer_callback(
						sd->context.dma_transfer_callback_context, 
						sd->context.dma_transfer_result
						);
				}
				/*
				// relinquish the time slot
				*/
				return;
				
			#if defined(SD_ENABLE_MULTI_BLOCK_WRITE)
			case SD_MULTI_BLOCK_WRITE_IN_PROGRESS:
				/*
				// write the 16-bit CRC
				*/
				/*spi_write(sd->context.spi_module, 0x0);
				spi_write(sd->context.spi_module, 0X1); */
				/*
				// record time of block completion
				*/
				#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
				rtc_timer_elapsed(&sd->context.dma_transfer_bus_time_s, &sd->context.dma_transfer_bus_time_ms);
				#if defined(SD_PRINT_TRANSIT_TIME)
				printf("Transit time: %ld secs, %ld ms\r\n", sd->context.dma_transfer_bus_time_s, sd->context.dma_transfer_bus_time_ms);
				#endif
				#endif
				/*
				// start counting programming time
				*/
				#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
				rtc_timer(&sd->context.dma_transfer_program_timer_s, &sd->context.dma_transfer_program_timer_ms);	
				#endif
				/*
				// wait for the data transfer response token from the card
				*/
				WAIT_FOR_DATA_RESPONSE_TOKEN(sd, tmp);
				/*
				// if the operation times out...
				*/
				if (sd->context.timeout) 
				{
					/*
					// reset the dma op completed flag
					*/
					sd->context.dma_transfer_completed = 0;
					/*
					// send 8 clock cycles
					*/
					spi_write(sd->context.spi_module, 0xFF);
					/*
					// de-assert the CS line
					*/
					DEASSERT_CS(sd);
					/*
					// clear busy signal
					*/
					BP_CLR(sd->context.busy_signal);
					/*
					// mark driver as not busy
					*/
					sd->context.busy = 0;
					/*
					// set the operation state
					*/
					*sd->context.dma_transfer_result = SD_TIMEOUT;
					/*
					// if a callback routine was provided with the
					// operation request the call it.
					*/
					if (sd->context.dma_transfer_callback_ex != NULL) 
					{
						sd->context.dma_transfer_callback_ex(
							sd->context.dma_transfer_callback_context, 
							sd->context.dma_transfer_result,
							&sd->context.dma_transfer_buffer,
							&sd->context.dma_transfer_response
							);
					}
					/*
					// relinquish the time slice
					*/
					return;
				}
				/*
				// if the data was accepted then set transfer state to 2
				// we're now waiting for the SD card to finish programming
				*/
				if (SD_DATA_RESPONSE_ACCEPTED == (tmp & SD_DATA_RESPONSE_ACCEPTED)) 
				{
					/*
					// set the dma transfer state to programming
					*/
					sd->context.dma_transfer_state = SD_MULTI_BLOCK_WRITE_PROGRAMMING;
					/*
					// set the result of the operation to AWAITING_DATA
					*/
					*sd->context.dma_transfer_result = SD_AWAITING_DATA;
					/*
					// set the response to the default STOP so that if user
					// doesn't handle callback the transfer gets stopped.]
					*/
					sd->context.dma_transfer_response = SD_MULTI_BLOCK_RESPONSE_STOP;
					/*
					// start overhead timer
					*/
					#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
					rtc_timer(&sd->context.dma_transfer_overhead_timer_s, &sd->context.dma_transfer_overhead_timer_ms);	
					#endif
					/*
					// ask users how to proceed
					*/
					if (sd->context.dma_transfer_callback_ex)
					{
						sd->context.dma_transfer_callback_ex(
							sd->context.dma_transfer_callback_context, 
							sd->context.dma_transfer_result,
							&sd->context.dma_transfer_buffer,
							&sd->context.dma_transfer_response
							);
					}		
					/*
					// reset state back to op in progress
					*/
					*sd->context.dma_transfer_result = SD_PROCESSING;
					/*
					// set dma buffer
					*/
					if (sd->context.dma_transfer_response == SD_MULTI_BLOCK_RESPONSE_READY)
					{
						if (sd->context.async_buffer)
						{
							#if defined(__C30__)
							__asm__
							(
								"mov %0, w1\n"
								"mov %1, w2\n"
								"repeat #%2\n"
								"mov [w1++], [w2++]"
								:
								: "g" (sd->context.dma_transfer_buffer), "g" (sd->context.async_buffer), "i" ((SD_BLOCK_LENGTH / 2) - 1)
								: "w1", "w2"
							);
							#else
							for (i = 0; i < (SD_BLOCK_LENGTH / 2); i++)
							{
								((uint16_t*) sd->context.async_buffer)[i] = ((uint16_t*) sd->context.dma_transfer_buffer)[i];
							}
							#endif
						}
						else
						{
							DMA_SET_BUFFER_A(sd->context.dma_channel_2, sd->context.dma_transfer_buffer);
						}
						/*
						// increase the DMA transfer address
						*/
						if (sd->card_info.high_capacity)
						{
							sd->context.dma_transfer_address++;
						}
						else
						{
							sd->context.dma_transfer_address += sd->card_info.block_length;
						}
						/*
						// reset the busy token counter (DEBUG)
						*/
						#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
						sd->context.dma_transfer_busy_count = 0;
						sd->context.dma_transfer_block_count++;	
						sd->context.dma_transfer_total_blocks++;
						if (sd->context.dma_transfer_response == SD_MULTI_BLOCK_RESPONSE_STOP)
						{
							printf("SDDRIVER: Filesystem sent STOP request\r\n");
						}
						#endif
					}
				}
				/*
				// if the data was rejected due to a CRC error
				// return the CRC error code
				*/		
				else if (SD_DATA_RESPONSE_REJECTED_CRC_ERROR == (tmp & SD_DATA_RESPONSE_REJECTED_CRC_ERROR)) 
				{
					/*
					// reset the dma op completed flag
					*/
					sd->context.dma_transfer_completed = 0;
					/*
					// send 8 clock pulses to the card
					*/
					spi_write(sd->context.spi_module, 0xFF);
					/*
					// de-assert the CS line
					*/
					DEASSERT_CS(sd);
					/*
					// clear busy signal
					*/
					BP_CLR(sd->context.busy_signal);
					/*
					// mark driver as not busy
					*/
					sd->context.busy = 0;
					/*
					// set the operation state
					*/
					*sd->context.dma_transfer_result = SD_CRC_ERROR;
					/*
					// if a callback routine was provided with the
					// operation request the call it.
					*/
					if (sd->context.dma_transfer_callback_ex != 0) 
					{
						sd->context.dma_transfer_callback_ex(
							sd->context.dma_transfer_callback_context, 
							sd->context.dma_transfer_result,
							&sd->context.dma_transfer_buffer,
							&sd->context.dma_transfer_response
							);
					}
					/*
					// relinquish the time slot
					*/
					return;
				}
				/*
				// if it was rejected due to a write error
				// return the unknown error code ( for now )
				*/
				else if (SD_DATA_RESPONSE_REJECTED_WRITE_ERROR == (tmp & SD_DATA_RESPONSE_REJECTED_WRITE_ERROR)) 
				{
					/*
					// reset the dma op completed flag
					*/
					sd->context.dma_transfer_completed = 0;
					/*
					// send 8 clock pulses to the card
					*/
					spi_write(sd->context.spi_module, 0xFF);
					/*
					// de-assert the CS line
					*/
					DEASSERT_CS(sd);
					/*
					// clear busy signal
					*/
					BP_CLR(sd->context.busy_signal);
					/*
					// mark driver as not busy
					*/
					sd->context.busy = 0;
					/*
					// set the operation state
					*/
					*sd->context.dma_transfer_result = SD_UNKNOWN_ERROR;
					/*
					// if a callback routine was provided with the
					// operation request the call it.
					*/
					if (sd->context.dma_transfer_callback_ex != 0) 
					{
						sd->context.dma_transfer_callback_ex(
							sd->context.dma_transfer_callback_context, 
							sd->context.dma_transfer_result,
							&sd->context.dma_transfer_buffer,
							&sd->context.dma_transfer_response
							);
					}
					/*
					// relinquish the time slot
					*/
					return;
				}
			case SD_MULTI_BLOCK_WRITE_PROGRAMMING:
				/*
				// stop overhead timer
				*/
				#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
				rtc_timer_elapsed(&sd->context.dma_transfer_overhead_timer_s, &sd->context.dma_transfer_overhead_timer_ms);	
				#endif
				/*
				// if the card is still programming relinquish
				// the time slice
				*/
				tmp = spi_read(sd->context.spi_module);
				if  (tmp == 0x0)
				{
					/*
					// restart overhead timer
					*/
					#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
					rtc_timer(&sd->context.dma_transfer_overhead_timer_s, &sd->context.dma_transfer_overhead_timer_ms);	
					#endif
					/*
					// count busy tokens
					*/
					#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
					sd->context.dma_transfer_busy_count++;
					#endif
					return;
				}	
				/*
				// calculate programming and overhead times
				*/
				#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
				rtc_timer_elapsed(&sd->context.dma_transfer_program_timer_s, &sd->context.dma_transfer_program_timer_ms);
				sd->context.dma_transfer_program_time_s += (uint16_t) sd->context.dma_transfer_program_timer_s;
				sd->context.dma_transfer_program_time_ms += (uint16_t) sd->context.dma_transfer_program_timer_ms;
				if (sd->context.dma_transfer_program_time_ms > 1000)
				{
					sd->context.dma_transfer_program_time_ms -= 1000;
					sd->context.dma_transfer_program_time_s += 1;
				}

				sd->context.dma_transfer_overhead_time_s += (uint16_t) sd->context.dma_transfer_overhead_timer_s;
				sd->context.dma_transfer_overhead_time_ms += (uint16_t) sd->context.dma_transfer_overhead_timer_ms;
				if (sd->context.dma_transfer_overhead_time_ms > 1000)
				{
					sd->context.dma_transfer_overhead_time_ms -= 1000;
					sd->context.dma_transfer_overhead_time_s += 1;
				}
				#endif
			
				/*
				// display programming time
				*/
				#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO) && defined(SD_PRINT_INTER_BLOCK_DELAYS)
				printf("SDDRIVER: Programming delay: %ld secs, %ld ms\r\n", sd->context.dma_transfer_program_timer_s, sd->context.dma_transfer_program_timer_ms);
				#endif
				/*
				// print busy token count
				*/
				#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO) && defined(SD_PRINT_PROGRAMMING_DELAYS)
				if (sd->context.dma_transfer_busy_count)
				{
					printf("SDDRIVER: Programming Delay! Blocks: %ld, Busy Tokens: %ld\r\n", 
						sd->context.dma_transfer_block_count, sd->context.dma_transfer_busy_count);
				}
				#endif
				
				/*
				// decide what to do based on user response
				*/
				switch (sd->context.dma_transfer_response)
				{
					case SD_MULTI_BLOCK_RESPONSE_READY:	/* data is ready to continue transfer */
						/*
						// decrease the # of blocks remaining in the current RU
						*/
						sd->context.dma_transfer_blocks_remaining--;
						/*
						// if there are not requests pending we can go on
						// with the multi-block transfer
						*/
						if (!sd->context.dma_transfer_blocks_remaining && !sd->context.request_queue)
						{
							sd->context.dma_transfer_blocks_remaining = sd->card_info.ru_size;
						}
						/*
						// if we've reached the end of the current RU
						// or no other requests are pending continue writting
						*/					
						if (sd->context.dma_transfer_blocks_remaining)
						{
							/*
							// reset dma transfer state
							*/
							sd->context.dma_transfer_completed = 0;
							sd->context.dma_transfer_state = SD_MULTI_BLOCK_WRITE_IN_PROGRESS;
							/*
							// send the start token and let the DMA do the rest
							*/
							spi_write(sd->context.spi_module, SD_BLOCK_START_TOKEN_MULT);
							/*
							// enable the dma channels
							*/
							DMA_CHANNEL_ENABLE(sd->context.dma_channel_1);
							DMA_CHANNEL_ENABLE(sd->context.dma_channel_2);
							/*
							// save the time of transfer start
							*/
							#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
							rtc_timer(&sd->context.dma_transfer_bus_time_s, &sd->context.dma_transfer_bus_time_ms);
							#endif
							/*
							// force the 1st byte transfer
							*/
							DMA_CHANNEL_FORCE_TRANSFER(sd->context.dma_channel_2);
							/*
							// set the state to SD_PROCESSING and relinquish control
							*/
							*sd->context.dma_transfer_result = SD_PROCESSING;
							/*
							// relinquish time slice
							*/
							return;
						}
						else
						{
							sd->context.dma_transfer_skip_callback = 1;
						}
					case SD_MULTI_BLOCK_RESPONSE_SKIP:
					{
						/*
						// enqueue the transfer to continue later
						// and stop.
						*/
						SD_ASYNC_REQUEST** req;
						/*
						// lock the queue
						*/				
						#if defined(SD_MULTI_THREADED)
						ENTER_CRITICAL_SECTION(sd->context.request_queue_lock);
						#endif
						/*
						// find the end of the queue
						*/
						i = 0;
						req = &sd->context.request_queue;
						while ((*req))
						{
							i++;
							req = &(*req)->next;
						}
						/*
						// if the queue is full we must wait for an
						// open slot
						*/
						_ASSERT(i < (SD_ASYNC_QUEUE_LIMIT + 1));
						/*
						// allocate memory for the request
						*/
						for (i = 0; i < (SD_ASYNC_QUEUE_LIMIT + 1); i++)
						{
							if (sd->context.request_queue_slots[i].mode == SD_FREE_REQUEST)
							{
								*req = &sd->context.request_queue_slots[i];
								break;
							}
						}
						/*
						// copy request parameters to queue
						*/
						(*req)->address = sd->context.dma_transfer_address;
						(*req)->buffer = sd->context.dma_transfer_buffer;
						(*req)->async_state = sd->context.dma_transfer_result;
						(*req)->callback_info_ex = sd->context.dma_transfer_callback_info_ex;
						(*req)->callback_info_ex->callback = sd->context.dma_transfer_callback_ex;
						(*req)->callback_info_ex->context = sd->context.dma_transfer_callback_context;
						(*req)->mode = SD_MULTI_BLOCK_WRITE_REQUEST;
						(*req)->needs_data = (sd->context.dma_transfer_response == SD_MULTI_BLOCK_RESPONSE_SKIP);
						(*req)->next = 0;
						/*
						// unlock the queue
						*/
						#if defined(SD_MULTI_THREADED)
						LEAVE_CRITICAL_SECTION(sd->context.request_queue_lock);
						#endif
					}	
					case SD_MULTI_BLOCK_RESPONSE_STOP:
						/*
						// send the start token and let the DMA do the rest
						*/
						spi_write(sd->context.spi_module, SD_BLOCK_STOP_TOKEN_MULT);
						/*
						// send 16 clock cycles
						*/
						spi_write(sd->context.spi_module, 0xFF);
						/*
						// set the transfer state to completing
						*/							
						sd->context.dma_transfer_state = SD_MULTI_BLOCK_WRITE_COMPLETING;
						/*
						// relinquish time slice
						*/
						return;
				}
			case SD_MULTI_BLOCK_WRITE_COMPLETING:
				/*
				// if the card is still programming relinquish
				// the time slice
				*/
				tmp = spi_read(sd->context.spi_module);
				if  (tmp == 0x0)
				{
					return;
				}	
				/*
				// print debug info
				*/
				#if defined(SD_PRINT_MULTI_BLOCK_TIME_INFO)
				rtc_timer_elapsed(&sd->context.dma_transfer_time_s, &sd->context.dma_transfer_time_ms);
				printf("SDDRIVER: Blocks written: %ld\r\n", sd->context.dma_transfer_total_blocks);
				printf("SDDRIVER: Programming time: %u secs, %u ms\r\n", sd->context.dma_transfer_program_time_s, sd->context.dma_transfer_program_time_ms);
				printf("SDDRIVER: Overhead time: %u secs, %u ms\r\n", sd->context.dma_transfer_overhead_time_s, sd->context.dma_transfer_overhead_time_ms);
				printf("SDDRIVER: Total write time: %ld secs, %ld ms\r\n", sd->context.dma_transfer_time_s, sd->context.dma_transfer_time_ms);
				printf("-------------------------\r\n");
				#endif
				/*
				// reset the dma transfer completed flag
				*/
				sd->context.dma_transfer_completed = 0;
				/*
				// clock out 8 cycles
				*/
				spi_write(sd->context.spi_module, 0xFF);
				/*
				// de-assert the CS line
				*/
				DEASSERT_CS(sd);
				/*
				// clear busy signal
				*/
				BP_CLR(sd->context.busy_signal);
				/*
				// mark device as not busy
				*/
				sd->context.busy = 0;
				/*
				// if a callback routine was provided with the
				// operation request the call it.
				*/
				if (sd->context.dma_transfer_response != SD_MULTI_BLOCK_RESPONSE_SKIP && sd->context.dma_transfer_skip_callback == 0)
				{
					/*
					// set the operation state
					*/
					*sd->context.dma_transfer_result = SD_SUCCESS;
					/*
					// call the user/filesystem callback routine
					*/
					if (sd->context.dma_transfer_callback_ex != 0) 
					{
						sd->context.dma_transfer_callback_ex(
							sd->context.dma_transfer_callback_context, 
							sd->context.dma_transfer_result,
							&sd->context.dma_transfer_buffer,
							&sd->context.dma_transfer_response
							);
					}
				}	
				sd->context.dma_transfer_skip_callback = 0;
				/*
				// relinquish the time slot
				*/
				return;
				
			#endif /* SD_ENABLE_MULTI_BLOCK_WRITE */
		}		
	}
	#if defined(SD_QUEUE_ASYNC_REQUESTS)
	else 
	{
		#if defined(SD_MULTI_THREADED)
		ENTER_CRITICAL_SECTION(sd->context.busy_lock);
		#endif
		if (!sd->context.busy)
		{
			/*
			// if there are requests on the queue
			*/
			if (sd->context.request_queue)
			{
				SD_ASYNC_REQUEST req;
				/*
				// lock the queue
				*/				
				#if defined(SD_MULTI_THREADED)
				ENTER_CRITICAL_SECTION(sd->context.request_queue_lock);
				#endif
				/*
				// grab the next request and
				// remove the request from the queue
				*/
				req = *sd->context.request_queue;
				#if defined(SD_ALLOCATE_QUEUE_FROM_HEAP)
				free(sd->context.request_queue);
				#else
				sd->context.request_queue->mode = SD_FREE_REQUEST;
				#endif
				sd->context.request_queue = req.next;
				/*
				// unlock the queue
				*/				
				#if defined(SD_MULTI_THREADED)
				LEAVE_CRITICAL_SECTION(sd->context.request_queue_lock);
				#endif
				/*
				// perform async op
				*/
				switch (req.mode)
				{
					case SD_WRITE_REQUEST:
						/*
						// take ownership of device
						*/
						sd->context.busy = 1;
						#if defined(SD_MULTI_THREADED)
						LEAVE_CRITICAL_SECTION(sd->context.busy_lock);
						#endif
						/*
						// process request
						*/
						sd_write(sd, req.address, req.buffer, req.async_state, req.callback_info, 1);
						/*
						// relinquish time slice
						*/
						return;
					
					case SD_READ_REQUEST:
						/*
						// take ownership of device
						*/
						sd->context.busy = 1;
						#if defined(SD_MULTI_THREADED)
						LEAVE_CRITICAL_SECTION(sd->context.busy_lock);
						#endif
						/*
						// process request
						*/
						sd_read(sd, req.address, req.buffer, req.async_state, req.callback_info, 1);
						/*
						// relinquish time slice
						*/
						return;
					
					#if defined(SD_ENABLE_MULTI_BLOCK_WRITE)	
					case SD_MULTI_BLOCK_WRITE_REQUEST:
						/*
						// take ownership of device
						*/
						sd->context.busy = 1;
						#if defined(SD_MULTI_THREADED)
						LEAVE_CRITICAL_SECTION(sd->context.busy_lock);
						#endif
						/*
						// process request
						*/
						sd_write_multiple_blocks(sd, req.address, 
							req.buffer, req.async_state, req.callback_info_ex, 1, req.needs_data);
						/*
						// relinquish time slice
						*/
						return;
						
					#endif	/* SD_ENABLE_MULTI_BLOCK_WRITE */
					
				}
			}
		}
		#if defined(SD_MULTI_THREADED)
		LEAVE_CRITICAL_SECTION(sd->context.busy_lock);
		#endif
	}
	#endif
}	

/*
// wait for a block start token
*/
static uint16_t sd_wait_for_data(SD_DRIVER* driver)
{
	unsigned int data;
	driver->context.timeout = SD_SPI_TIMEOUT;
	do
	{
		data = spi_read(driver->context.spi_module);
		
		switch (data)
		{
			case SD_DATA_ERROR_UNKNOWN :
				spi_write(driver->context.spi_module, 0xFF);
				DEASSERT_CS(driver);
				BP_CLR(driver->context.busy_signal);
				return SD_UNKNOWN_ERROR;
				
			case SD_DATA_ERROR_CC_ERROR:
				spi_write(driver->context.spi_module, 0xFF);
				DEASSERT_CS(driver);
				BP_CLR(driver->context.busy_signal);
				return SD_CC_ERROR;
				
			case SD_DATA_ERROR_ECC_FAILED:
				spi_write(driver->context.spi_module, 0xFF);
				DEASSERT_CS(driver);
				BP_CLR(driver->context.busy_signal);
				return SD_CARD_ECC_FAILED;
				
			case  SD_DATA_ERROR_OUT_OF_RANGE:
				spi_write(driver->context.spi_module, 0xFF);
				DEASSERT_CS(driver);
				BP_CLR(driver->context.busy_signal);
				return SD_OUT_OF_RANGE;
		}		
		/*
		// decrement timeout
		*/
		driver->context.timeout--;
	}
	while (data != SD_BLOCK_START_TOKEN && driver->context.timeout);
	/*
	// set the timeout flag
	*/
	driver->context.timeout = (driver->context.timeout) ? 0 : 1;
	/*
	// return success code
	*/
	return SD_SUCCESS;
}

/*
// waits for a response (R1)
*/
static void sd_wait_for_response(SD_DRIVER* driver, unsigned char* data)
{
	driver->context.timeout = SD_SPI_TIMEOUT;
	do
	{
		*data = spi_read(driver->context.spi_module);
		driver->context.timeout--;

	}
	while (driver->context.timeout && *data &&
		((*data & 0x80 ) != 0x0 										||
	  	((*data & SD_RSP_INVALID_PARAMETER) != SD_RSP_INVALID_PARAMETER &&
		(*data & SD_RSP_ADDRESS_ERROR) != SD_RSP_ADDRESS_ERROR 			&&
		(*data & SD_RSP_ERASE_SEQ_ERROR) != SD_RSP_ERASE_SEQ_ERROR 		&&
		(*data & SD_RSP_CRC_ERROR) != SD_RSP_CRC_ERROR 					&&
		(*data & SD_RSP_ILLEGAL_COMMAND) != SD_RSP_ILLEGAL_COMMAND 		&&
		(*data & SD_RSP_ERASE_RESET) != SD_RSP_ERASE_RESET 				&&
		(*data & SD_RSP_IDLE) != SD_RSP_IDLE)));		
	/*
	// set the timeout flag appropriately
	*/
	driver->context.timeout = (driver->context.timeout) ? 0 : 1;
}

/*
// translate the SD card response code into a result code
*/
static unsigned char sd_translate_response(unsigned char response)
{
	return ((response & SD_RSP_INVALID_PARAMETER) ? SD_INVALID_PARAMETER :
		(response & SD_RSP_ADDRESS_ERROR) ? SD_ADDRESS_ERROR :
		(response & SD_RSP_ERASE_SEQ_ERROR) ? SD_ERASE_SEQ_ERROR :
		(response & SD_RSP_CRC_ERROR) ? SD_CRC_ERROR :
		(response & SD_RSP_ILLEGAL_COMMAND) ? SD_ILLEGAL_COMMAND :
		(response & SD_RSP_ERASE_RESET) ? SD_ERASE_RESET :
		(response & SD_RSP_IDLE) ? SD_IDLE : 0x0);
}

/*
// sends an IO command to the sd card
*/
static void sd_send_io_command(SPI_MODULE spi_module, unsigned char cmd, uint32_t address)
{
	#if defined(USE_CRC)
	register unsigned char crc = 0;
	#endif
	/*
	// write the command byte
	*/
	#if defined(USE_CRC)
	crc = calc_crc7(cmd, crc);
	#endif
	spi_write(spi_module, cmd);
	/*
	// write byte 0 of address
	*/
	#if defined(USE_CRC)
	crc = calc_crc7(((unsigned char*) &address)[3], crc);
	#endif
	spi_write(spi_module, ((unsigned char*) &address)[3]);
	/*
	// write byte 1 of address
	*/
	#if defined(USE_CRC)
	crc = calc_crc7(((unsigned char*) &address)[2], crc);
	#endif
	spi_write(spi_module, ((unsigned char*) &address)[2]);
	/*
	// write byte 2 of address
	*/
	#if defined(USE_CRC)
	crc = calc_crc7(((unsigned char*) &address)[1], crc);
	#endif
	spi_write(spi_module, ((unsigned char*) &address)[1]);
	/*
	// write byte 3 of address
	*/
	#if defined(USE_CRC)
	crc = calc_crc7(((unsigned char*) &address)[0], crc);
	#endif
	spi_write(spi_module, ((unsigned char*) &address)[0]);
	/*
	// write the crc7 byte
	*/
	#if defined(USE_CRC)
	spi_write(spi_module, (crc << 1) | 1);
	#else
	spi_write(spi_module, 0x1);
	#endif
}

/*
// performs CRC7 calculations
*/
#if !defined(USE_CRC)
unsigned char calc_crc7(unsigned char value, unsigned char crc) 
{
	char c;
	
	for (c = 0; c < 8; c++)
	{
		crc <<= 1;
		if ((value & 0x80) ^ (crc & (0x80))) crc ^= (0x09);
		value <<= 1; 
	}	

	return crc;
} 
#endif
