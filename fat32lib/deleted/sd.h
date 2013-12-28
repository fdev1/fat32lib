#ifndef __SD_H__
#define __SD_H__

#include "common.h"
#include "dma.h"
#include "storage_device.h"

//
// resource macros
//
#define SPI_MODULE					2
#define CS_PORT						F
#define CS_PIN						3
#define WRITE_DMA_CHANNEL			0
#define READ_DMA_CHANNEL			1

//
// the default block length supported
// by SD cards ( and the only block length
// used by this implementation
//
#define SD_BLOCK_LENGTH				( 0x200 )
#define SD_WORKING_BYTES			( SD_BLOCK_LENGTH + 0x4 )

//
// return codes
//
#define SD_SUCCESS					( STORAGE_SUCCESS )
#define SD_VOLTAGE_NOT_SUPPORTED	( STORAGE_VOLTAGE_NOT_SUPPORTED )
#define SD_INVALID_CARD				( STORAGE_INVALID_MEDIA )
#define SD_UNKNOWN_ERROR			( STORAGE_UNKNOWN_ERROR )
#define SD_INVALID_PARAMETER		( STORAGE_INVALID_PARAMETER )
#define SD_ADDRESS_ERROR			( STORAGE_ADDRESS_ERROR )
#define SD_ERASE_SEQ_ERROR			( STORAGE_ERASE_SEQ_ERROR )
#define SD_CRC_ERROR				( STORAGE_CRC_ERROR )
#define SD_ILLEGAL_COMMAND			( STORAGE_ILLEGAL_COMMAND )
#define SD_ERASE_RESET				( STORAGE_ERASE_RESET )
#define SD_COMMUNICATION_ERROR		( STORAGE_COMMUNICATION_ERROR )
#define SD_TIMEOUT					( STORAGE_TIMEOUT )
#define SD_IDLE						( STORAGE_IDLE )
#define SD_OUT_OF_RANGE				( STORAGE_OUT_OF_RANGE )
#define SD_CARD_ECC_FAILED			( STORAGE_MEDIA_ECC_FAILED )
#define SD_CC_ERROR					( STORAGE_CC_ERROR )
#define SD_PROCESSING				( STORAGE_OP_IN_PROGRESS )

//
// SD callback function
// 
typedef void SD_ASYNC_CALLBACK( uint16_t* State, void* Context );

//
// SD callback info/state structure
//
typedef struct _SD_CALLBACK_INFO 
{
	SD_ASYNC_CALLBACK* Callback;
	void* Context;
}
SD_CALLBACK_INFO, *PSD_CALLBACK_INFO;

//
// driver context structure
//
typedef struct _SD_DRIVER_CONTEXT 
{
	char dma_op_completed;
	char pending_op_write;
	uint16_t pending_op_length;
	uint32_t pending_op_address;
	unsigned char* pending_op_buffer;
	uint16_t pending_padbytes;
	uint16_t pending_len;
	uint16_t* pending_op_state;
	SD_ASYNC_CALLBACK* callback;
	void* context;
}
SD_DRIVER_CONTEXT, *PSD_DRIVER_CONTEXT;

//
// stores information about the SD card.
//
typedef struct _SD_CARD_INFO 
{
	unsigned char Version;
	char HighCapacity;
	uint16_t BlockLength;
	uint32_t Capacity;
	unsigned char TAAC;
	unsigned char NSAC;
	unsigned char R2W_FACTOR;
	unsigned char padding1;
	unsigned char* WorkingBytes;
	DMA_CHANNEL* DmaChannel1;
	DMA_CHANNEL* DmaChannel2;
	unsigned char BusDmaIrq;
	SD_DRIVER_CONTEXT Context;
} 
SD_CARD_INFO, *PSD_CARD_INFO;	

//
// initializes the SD card
//
uint16_t sd_init( 
	SD_CARD_INFO* card_info, 
	unsigned char* working_bytes, 
	DMA_CHANNEL* const channel1, 
	DMA_CHANNEL* const channel2, 
	const unsigned char bus_dma_irq );

//
// does sd background processing
//
void sd_timeslot( PSD_CARD_INFO sd );

//
// gets the storage device interface for
// this driver
//
void sd_get_storage_device_interface( PSD_CARD_INFO card_info, PSTORAGE_DEVICE device );
//
// reads an arbitrary amount of data to an
// SD card synchronously
//
uint16_t sd_read( SD_CARD_INFO*, 
	uint32_t address, uint16_t length, unsigned char* buffer, uint16_t* async_state, PSD_CALLBACK_INFO callback_info );

//
// Writes an arbitrary amount of data to an SD card.
// ---
//
// Parameters:
//
// card_info 	- structure returned by sd_init
// address 		- the address of the block to be written
// length		- the amount of bytes to writen
// buffer		- a pointer to the location of the data to be written
// async_state	- a pointer to a byte that contains the operation state
// ---
//
// Returns:
//
// If async_state is set to anything other than NULL this
// routine will return SD_PROCESSING ( unless an error occurs
// prior to the beggining of the transfer - ie. addressing
// errors ) and the value pointed to by async_state will be
// set to SD_PROCESSING until the operation completes, then it
// will be set to one of the return codes specified on the
// header file 'sd.h' except SD_PROCESSING.
//
// If async_state is set to NULL this routine will return
// one of the return codes specified on the header file
// 'sd.h' except SD_PROCESSING.
// ---
//
// Remarks: 
//
// for optimal performance both address and length should be
// multiples of 512
//
uint16_t sd_write( SD_CARD_INFO* card_info, 
	uint32_t address, uint16_t length, unsigned char* buffer, uint16_t* async_state, PSD_CALLBACK_INFO callback_info );

#endif
