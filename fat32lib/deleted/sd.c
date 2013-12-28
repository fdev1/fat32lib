//
// SD Card Version 2.0 Driver
// ==========================
//
// Written by: Fernando Rodriguez ( frodriguez.im@hotmail.com )
// Copyright (c) 2009 All Rights Reserved
//
// Driver Resources:
// 
// - 1 SPI Module
// - 2 DMA Channels
// - 3 IO Pins ( CS, WP, and CD )
//

#include "sd.h"
#include "spi.h"

//
// SD version
//
#define SD_VERSION_1_0							0b00010000
#define SD_VERSION_2_0							0b00100000

//
// SD R1 response fields
//
#define SD_RSP_INVALID_PARAMETER				0b01000000
#define SD_RSP_ADDRESS_ERROR					0b00100000
#define SD_RSP_ERASE_SEQ_ERROR					0b00010000
#define SD_RSP_CRC_ERROR						0b00001000
#define SD_RSP_ILLEGAL_COMMAND					0b00000100
#define SD_RSP_ERASE_RESET						0b00000010
#define SD_RSP_IDLE								0b00000001

//
// misc
//
#define SD_BLOCK_START_TOKEN					0b11111110
#define SD_WAIT_TOKEN							0x00
#define SD_DATA_ERROR_UNKNOWN					0x01
#define SD_DATA_RESPONSE_MASK					0b00011111
#define SD_DATA_RESPONSE_ACCEPTED				0x5
#define SD_DATA_RESPONSE_REJECTED_CRC_ERROR		0xB
#define SD_DATA_RESPONSE_REJECTED_WRITE_ERROR	0xD
#define SD_DATA_ERROR_CC_ERROR					0x02
#define SD_DATA_ERROR_ECC_FAILED				0x04
#define SD_DATA_ERROR_OUT_OF_RANGE				0x08
#define SD_SPI_TIMEOUT							65000

//
// SD commands
//
#define CMD_MASK								( 0b01000000 )
#define GO_IDLE_STATE							( CMD_MASK | 0x0 )
#define SEND_IF_COND							( CMD_MASK | 0x8 )
#define SEND_CSD								( CMD_MASK | 0x9 )
#define SEND_BLOCK_LEN							( CMD_MASK | 16 )
#define READ_SINGLE_BLOCK						( CMD_MASK | 17 )
#define WRITE_SINGLE_BLOCK						( CMD_MASK | 24 )
#define SD_APP_OP_COND							( CMD_MASK | 0x29 )
#define APP_CMD									( CMD_MASK | 0x37 )
#define READ_OCR								( CMD_MASK | 0x3A )

//
// convenience macros
//
#define ASSERT_CS( )							IO_PIN_WRITE( G, 3, 0 )
#define DEASSERT_CS( )							IO_PIN_WRITE( G, 3, 1 )
#define IS_CARD_LOCKED( )						( 0 )

PSD_CARD_INFO write_card_info;


//
// send a command to the SD card with
// all-zeroes arguments
//
#define SEND_COMMAND( _cmd )		\
{									\
	SPI_WRITE_DATA( _cmd );			\
	SPI_WRITE_DATA( 0x00 );			\
	SPI_WRITE_DATA( 0x00 );			\
	SPI_WRITE_DATA( 0x00 );			\
	SPI_WRITE_DATA( 0x00 );			\
	SPI_WRITE_DATA( 0x95 );			\
}

//
// send a command /w args to the SD card
//
#define SEND_COMMAND_WITH_ARGS( 				\
			cmd, arg0, arg1, arg2, arg3 )		\
{												\
	SPI_WRITE_DATA( cmd );						\
	SPI_WRITE_DATA( arg0 );						\
	SPI_WRITE_DATA( arg1 );						\
	SPI_WRITE_DATA( arg2 );						\
	SPI_WRITE_DATA( arg3 );						\
	SPI_WRITE_DATA( 0x95 );						\
}

//
// sends an IO command to the SD card
//
#define SEND_IO_COMMAND( cmd, address )				\
{													\
	SPI_WRITE_DATA(cmd);							\
	SPI_WRITE_DATA(((unsigned char*) &address)[3]);	\
	SPI_WRITE_DATA(((unsigned char*) &address)[2]);	\
	SPI_WRITE_DATA(((unsigned char*) &address)[1]);	\
	SPI_WRITE_DATA(((unsigned char*) &address)[0]);	\
	SPI_WRITE_DATA(0x95);							\
}


//
// wait for a response to an SD command
//
#define WAIT_FOR_RESPONSE( rsp, timeout, toval ) 							\
{																			\
	timeout = toval;														\
	do {																	\
		SPI_READ_DATA( rsp );												\
		timeout--;															\
	}																		\
	while ( timeout && rsp 												&&	\
	  ( ( rsp & 0x80 ) != 0x0 											||	\
	  ( ( rsp & SD_RSP_INVALID_PARAMETER ) != SD_RSP_INVALID_PARAMETER 	&& 	\
		( rsp & SD_RSP_ADDRESS_ERROR ) != SD_RSP_ADDRESS_ERROR 			&&	\
		( rsp & SD_RSP_ERASE_SEQ_ERROR ) != SD_RSP_ERASE_SEQ_ERROR 		&&	\
		( rsp & SD_RSP_CRC_ERROR ) != SD_RSP_CRC_ERROR 					&&	\
		( rsp & SD_RSP_ILLEGAL_COMMAND ) != SD_RSP_ILLEGAL_COMMAND 		&&	\
		( rsp & SD_RSP_ERASE_RESET ) != SD_RSP_ERASE_RESET 				&&	\
		( rsp & SD_RSP_IDLE ) != SD_RSP_IDLE ) ) );							\
}	

//
// wait for the data response token
//
#define WAIT_FOR_DATA_RESPONSE_TOKEN( tkn, timeout, toval )	\
{															\
	timeout = toval;										\
	do {													\
		SPI_READ_DATA( tkn );								\
		tkn &= SD_DATA_RESPONSE_MASK;						\
		timeout--;											\
	}														\
	while ( tkn != SD_DATA_RESPONSE_ACCEPTED &&				\
			tkn != SD_DATA_RESPONSE_REJECTED_CRC_ERROR &&	\
			tkn != SD_DATA_RESPONSE_REJECTED_WRITE_ERROR );	\
}			

//
// translate the response
//
#define TRANSLATE_RESPONSE( rsp )								\
	( tmp & SD_RSP_INVALID_PARAMETER ) ? SD_INVALID_PARAMETER :	\
	( tmp & SD_RSP_ADDRESS_ERROR ) ? SD_ADDRESS_ERROR :			\
	( tmp & SD_RSP_ERASE_SEQ_ERROR ) ? SD_ERASE_SEQ_ERROR :		\
	( tmp & SD_RSP_CRC_ERROR ) ? SD_CRC_ERROR :					\
	( tmp & SD_RSP_ILLEGAL_COMMAND ) ? SD_ILLEGAL_COMMAND :		\
	( tmp & SD_RSP_ERASE_RESET ) ? SD_ERASE_RESET :				\
	( tmp & SD_RSP_IDLE ) ? SD_IDLE : 0x0;
	
//
// waits for incoming data
//
#define WAIT_FOR_DATA( timeout, toval )							\
{																\
	timeout = toval;											\
	do { 														\
		SPI_READ_DATA( __dummy__ );								\
		if ( __dummy__ == SD_DATA_ERROR_UNKNOWN ) {				\
			SPI_WRITE_DATA( 0xFF );								\
			DEASSERT_CS( );										\
			return SD_UNKNOWN_ERROR;							\
		}														\
		else if ( __dummy__ == SD_DATA_ERROR_CC_ERROR ) {		\
			SPI_WRITE_DATA( 0xFF );								\
			DEASSERT_CS( );										\
			return SD_CC_ERROR;									\
		}														\
		else if ( __dummy__ == SD_DATA_ERROR_ECC_FAILED ) {		\
			SPI_WRITE_DATA( 0xFF );								\
			DEASSERT_CS( );										\
			return SD_CARD_ECC_FAILED;							\
		}														\
		else if ( __dummy__ == SD_DATA_ERROR_OUT_OF_RANGE ) {	\
			SPI_WRITE_DATA( 0xFF );								\
			DEASSERT_CS( );										\
			return SD_OUT_OF_RANGE;								\
		}														\
		timeout--;												\
	}															\
	while ( __dummy__ != SD_BLOCK_START_TOKEN && timeout );		\
}		

//
// waits for incoming data - this version is to be
// called by asynchronous function that return void and
// set the output state before returning
//
#define WAIT_FOR_DATA_ASYNC( timeout, toval, state )			\
{																\
	timeout = toval;											\
	do { 														\
		SPI_READ_DATA( __dummy__ );								\
		if ( __dummy__ == SD_DATA_ERROR_UNKNOWN ) {				\
			SPI_WRITE_DATA( 0xFF );								\
			DEASSERT_CS( );										\
			*state = SD_UNKNOWN_ERROR;							\
			return;												\
		}														\
		else if ( __dummy__ == SD_DATA_ERROR_CC_ERROR ) {		\
			SPI_WRITE_DATA( 0xFF );								\
			DEASSERT_CS( );										\
			*state = SD_CC_ERROR;								\
			return;												\
		}														\
		else if ( __dummy__ == SD_DATA_ERROR_ECC_FAILED ) {		\
			SPI_WRITE_DATA( 0xFF );								\
			DEASSERT_CS( );										\
			*state = SD_CARD_ECC_FAILED;						\
			return;												\
		}														\
		else if ( __dummy__ == SD_DATA_ERROR_OUT_OF_RANGE ) {	\
			SPI_WRITE_DATA( 0xFF );								\
			DEASSERT_CS( );										\
			*state = SD_OUT_OF_RANGE;							\
			return;												\
		}														\
		timeout--;												\
	}															\
	while ( __dummy__ != SD_BLOCK_START_TOKEN && timeout );		\
}		

//
// wait while the SD card is busy
//
#define WAIT_WHILE_CARD_BUSY( )		\
{									\
	do { SPI_READ_DATA( tmp ); }	\
	while ( tmp == 0x0 );			\
}	

//
// prototypes
//
void sd_init_dma( PSD_CARD_INFO sd );


//
// storage device interface
//
uint16_t sd_get_sector_size( PSD_CARD_INFO card_info );
uint16_t sd_read_sector( PSD_CARD_INFO card_info, uint32_t sector_address, unsigned char* buffer );
uint16_t sd_write_sector( PSD_CARD_INFO card_info, uint32_t sector_address, unsigned char* buffer );
uint16_t sd_read_sector_async( PSD_CARD_INFO card_info, uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, PSD_CALLBACK_INFO callback_info );
uint16_t sd_write_sector_async( PSD_CARD_INFO card_info, uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, PSD_CALLBACK_INFO callback_info );
uint32_t sd_get_total_sectors( PSD_CARD_INFO card_info );

//
// gets the storage device interface
//
void sd_get_storage_device_interface( PSD_CARD_INFO card_info, PSTORAGE_DEVICE device ) {
	device->Media = card_info;
	device->ReadSector = ( STORAGE_DEVICE_READ* ) &sd_read_sector;
	device->WriteSector = ( STORAGE_DEVICE_WRITE* ) &sd_write_sector;
	device->GetSectorSize = ( STORAGE_DEVICE_GET_SECTOR_SIZE* ) &sd_get_sector_size;
	device->ReadSectorAsync = ( STORAGE_DEVICE_READ_ASYNC* ) &sd_read_sector_async;
	device->WriteSectorAsync = ( STORAGE_DEVICE_WRITE_ASYNC* ) &sd_write_sector_async;
	device->GetTotalSectors = ( STORAGE_DEVICE_GET_SECTOR_COUNT* ) &sd_get_total_sectors;
}	
//
// gets the total capacity of the SD card
// in bytes
//
uint32_t sd_get_total_sectors( PSD_CARD_INFO card_info ) {
	return card_info->Capacity / card_info->BlockLength;
}	
//
// gets the sector length
//
uint16_t sd_get_sector_size( PSD_CARD_INFO card_info ) {
	return card_info->BlockLength;	
}	
//
// read sector synchronously
//
uint16_t sd_read_sector( PSD_CARD_INFO card_info, uint32_t sector_address, unsigned char* buffer ) {
	//
	// calculate the sector address
	//
	uint32_t address = ( card_info->HighCapacity ) ? 
		sector_address : sector_address * card_info->BlockLength;
	//
	// read the sector
	//
	return sd_read( card_info, address, 
		card_info->BlockLength, buffer, NULL, NULL );
}
//
// read sector asynchronously
//
uint16_t sd_read_sector_async( PSD_CARD_INFO card_info, uint32_t sector_address, 
	unsigned char* buffer, uint16_t* async_state, PSD_CALLBACK_INFO callback ) {
	//
	// calculate the sector address
	//
	uint32_t address = ( card_info->HighCapacity ) ? 
		sector_address : sector_address * card_info->BlockLength;
	//
	// read the sector
	//
	return sd_read( card_info, address, 
		card_info->BlockLength, buffer, async_state, callback );	
}	
//
// write sector synchronously
//
uint16_t sd_write_sector( PSD_CARD_INFO card_info, uint32_t sector_address, unsigned char* buffer ) {
	//
	// calculate the sector address
	//
	uint32_t address = ( card_info->HighCapacity ) ? 
		sector_address : sector_address * card_info->BlockLength;
	//
	// write the sector
	//
	return sd_write( card_info, address, card_info->BlockLength, buffer, NULL, NULL );	
}	

//
// write sector asynchronously
//
uint16_t sd_write_sector_async( PSD_CARD_INFO card_info, 
	uint32_t sector_address, unsigned char* buffer, uint16_t* async_state, 
		PSD_CALLBACK_INFO callback ) {
			
	//
	// calculate the sector address
	//
	uint32_t address = ( card_info->HighCapacity ) ? 
		sector_address : sector_address * card_info->BlockLength;
	//
	// write the sector
	//
	return sd_write( card_info, address, 
		card_info->BlockLength, buffer, async_state, callback );
	
}	

//
// sd card initialization routine
//
uint16_t sd_init(
	SD_CARD_INFO* card_info, 
	unsigned char* working_bytes, 
	DMA_CHANNEL* const dma_channel1, 
	DMA_CHANNEL* const dma_channel2, 
	const unsigned char bus_dma_irq ) 
{
	//
	unsigned char tmp;			// temp buffer for data received from SPI
	unsigned char read_bl_len;	// read_bl_len of the CSD
	unsigned char c_size_mult;	// c_size_mult of the CSD
	uint16_t i = 0;			// temp storage
	uint16_t timeout;		// timeout counter
	uint16_t mult;			// card size multiplier
	uint32_t block_nr;		// block nr
	uint32_t c_size;		// c_size field of the CSD
	//
	// if thi is the first call to the initialization
	// sequence
	//
	if ( card_info->WorkingBytes != working_bytes ) {
		//
		// initialize the driver's context
		//
		card_info->Context.dma_op_completed = 0;
		//
		// copy the pointer to the working bytes
		// to the card_info structure
		//
		card_info->WorkingBytes = working_bytes;
		card_info->DmaChannel1 = dma_channel1;
		card_info->DmaChannel2 = dma_channel2;
		card_info->BusDmaIrq = bus_dma_irq;
		//
		// configure the dma channels used
		// by this driver
		//
		sd_init_dma( card_info );
		//
		// register the driver's timeslot		
		//
		//system_register_timeslot( ( TIME_SLOT_ENTRY* ) &sd_timeslot, card_info );
	}
	//
	// initialize the sp module @ < 400 khz
	//
	spi_init( );	// 310.5 khz
	//
	// initialize the CRC section of the
	// block buffer to a constant value since we're
	// not using CRC
	//
	card_info->WorkingBytes[ SD_BLOCK_LENGTH ] = 0x0;
	card_info->WorkingBytes[ SD_BLOCK_LENGTH + 0x1 ] = 0x1;
	card_info->WorkingBytes[ SD_BLOCK_LENGTH + 0x2 ] = 0xFF;
	//
	// set pin 3 of port F as output,
	// the SD card's CS line will be connected
	// to this pin
	//
	IO_PIN_WRITE( G, 3, 1 );
	IO_PIN_SET_AS_OUTPUT( G, 3 );
	//
	// send at least 74 clock pulses to the card for it
	// to initialize ( 80 is as close as we can get ).
	//
	SPI_WRITE_DATA( 0xFF );	
	SPI_WRITE_DATA( 0xFF );	
	SPI_WRITE_DATA( 0xFF );	
	SPI_WRITE_DATA( 0xFF );	
	SPI_WRITE_DATA( 0xFF );	
	SPI_WRITE_DATA( 0xFF );	
	SPI_WRITE_DATA( 0xFF );	
	SPI_WRITE_DATA( 0xFF );	
	SPI_WRITE_DATA( 0xFF );	
	SPI_WRITE_DATA( 0xFF );	
	//
	// assert the CS line of the SD card
	//
	ASSERT_CS( );
	//
	// send 16 clock pulses to the card
	//
	SPI_WRITE_DATA( 0xFF );	
	SPI_WRITE_DATA( 0xFF );	
	//
	// send the reset command
	//
	SEND_COMMAND( GO_IDLE_STATE );
	//
	// wait for the response
	//
	WAIT_FOR_RESPONSE( tmp, timeout, SD_SPI_TIMEOUT );
	//
	// check for timeout condition
	//
	if ( !timeout ) {
		DEASSERT_CS( );
		return SD_TIMEOUT;
	}
	//
	// translate the response
	//	
	tmp = TRANSLATE_RESPONSE( tmp );
	//
	// if an error was returned, then
	// deassert the CS line and return
	// the error code
	//
	if ( tmp != SD_IDLE ) {
		SPI_WRITE_DATA( 0xFF );
		DEASSERT_CS( );
		return tmp;
	}
	//
	// send 8 clock pulses
	// 
	SPI_WRITE_DATA( 0xFF );
	//
	// send the command 8
	//
	SEND_COMMAND_WITH_ARGS( SEND_IF_COND, 0x00, 0x00, 0x01, 0xAA );
	//
	// discard the 1st 4 bytes of the response
	//	
	WAIT_FOR_RESPONSE( tmp, timeout, SD_SPI_TIMEOUT );
	//
	// check for timeout condition
	//
	if ( !timeout ) {
		DEASSERT_CS( );
		return SD_TIMEOUT;
	}
	//
	// translate the response
	// 
	tmp = TRANSLATE_RESPONSE( tmp );
	//
	// if the command is not supported then
	// this is either a standard capacity card,
	// or a legacy SD card or an MMC card
	//
	if ( tmp == SD_ILLEGAL_COMMAND ) {
		//
		// for now we just assume that it is a legacy
		// card and we set the version on the card info 
		// structure, later down the line we'll know if
		// it's not an SD card when we get the same 
		// response to other commands. If the card is a
		// standard capacity SD version 2 card we'll
		// treat it just as a legacy card so there's no
		// need to check for that condition
		//
		card_info->Version = SD_VERSION_1_0;
		//
		// send 8 clock pulses to the card
		//	
		SPI_WRITE_DATA( 0xFF );
		//
		// skip the version 2-specific steps of the
		// initialization process
		//
		goto __legacy_card;
	}
	//
	// if we're here, then this must be an
	// SD version 2 card
	//
	card_info->Version = SD_VERSION_2_0;
	//
	// discard the next 2 bytes of the SEND_IF_COND
	// command response
	//
	SPI_READ_DATA( tmp );
	SPI_READ_DATA( tmp );
	//
	// read the next byte which is the supported 
	// operating voltage of the card
	//
	SPI_READ_DATA( tmp );
	//
	// check that the card supports our operating
	// voltage
	//
	if ( ( tmp & 0xF ) != 0x1 )
		i = SD_VOLTAGE_NOT_SUPPORTED;
	//
	// read the 5th byte of the SEND_IF_COND (8) 
	// command response
	//
	SPI_READ_DATA( tmp );
	//
	// if the pattern bits of the response
	// don't match then the there has been a
	// communication error
	//
	if ( tmp != 0xAA ) {
		DEASSERT_CS( );
		i = SD_COMMUNICATION_ERROR;
	}
	//
	// read and discard the last byte of the
	// SEND_IF_COND ( 8 ) command response ( crc check )
	//
	SPI_READ_DATA( tmp );
	//
	// send 8 clock pulses to the card
	//
	SPI_WRITE_DATA( 0xFF );
	//
	// if an error occurred ( volatage not
	// supported ) de-assert the CS line and
	// return the error code
	//
	if ( i != NULL ) {
		DEASSERT_CS( );
		return i;
	}
__legacy_card:
	do {
		//
		// send an APP_CMD (55) command to indicate that the next
		// command is an application specific command
		//
		SEND_COMMAND( APP_CMD );
		//
		// read the response
		//
		WAIT_FOR_RESPONSE( tmp, timeout, SD_SPI_TIMEOUT );
		//
		// check for timeout condition
		//
		if ( !timeout ) {
			DEASSERT_CS( );
			return SD_TIMEOUT;
		}
		//
		// send 8 clock pulses to the card
		//
		SPI_WRITE_DATA( 0xFF );
		//
		// if we got an invalid command response
		// then the card is not a valid SD card, it
		// must be an MMC card
		// 
		if ( tmp & SD_RSP_ILLEGAL_COMMAND ) {
			DEASSERT_CS( );
			return SD_INVALID_CARD;
		}
		//
		// send the SD_APP_OP_COND (41) command to find
		// out if the card has completed it's initialization
		// process
		//
		SEND_COMMAND( SD_APP_OP_COND );
		//
		// discard the 1st byte of the response
		//
		WAIT_FOR_RESPONSE( tmp, timeout, SD_SPI_TIMEOUT );
		//
		// check for timeout condition
		//
		if ( !timeout ) {
			DEASSERT_CS( );
			return SD_TIMEOUT;	
		}
		//
		// if we got an invalid command response
		// this is not a valid SD card ( mmc? )
		//
		if ( tmp & SD_RSP_ILLEGAL_COMMAND )
			i = SD_INVALID_CARD;
		//
		// send 8 clock cycles
		//
		SPI_WRITE_DATA( 0xFF );
		//
		// if we received an error from the
		// card then return the error code
		//		
		if ( i != 0 ) {
			DEASSERT_CS( );
			return i;
		}
	}
	//
	// loop while the card is initializing
	//
	while ( tmp & SD_RSP_IDLE );
	//
	// send READ_OCR (58) command to get the OCR 
	// register in order to check that the card supports our
	// supply volate
	//
	SEND_COMMAND( READ_OCR );
	//
	// read the response
	//
	WAIT_FOR_RESPONSE( tmp, timeout, SD_SPI_TIMEOUT );	// R1
	//
	// check for timeout condition
	//
	if ( !timeout ) {
		DEASSERT_CS( );
		return SD_TIMEOUT;	
	}
	//
	// translate the response
	//
	tmp = TRANSLATE_RESPONSE( tmp );
	//
	// if an error was returned by the
	// card fail the initialization
	//
	if ( tmp ) {
		DEASSERT_CS( );
		return tmp;
	}
	//
	// read the 1st byte of the OCR
	//
	SPI_READ_DATA( tmp );			// OCR1
	//
	// save the card capacity status bit
	//
	card_info->HighCapacity = ( tmp & 0x40 );
	//
	// read the 2nd byte of the OCR
	//
	SPI_READ_DATA( tmp );			// OCR2
	//
	// if neither bit 20 or 21 of the OCR 
	// register is set then the card does
	// not support our supply volage
	//
	if ( !( tmp & 0x30 ) ) {
		//
		// de-assert the CS line
		//
		DEASSERT_CS( );
		//
		// return error
		//
		return SD_VOLTAGE_NOT_SUPPORTED;
	}
	//
	// read and discard the rest of the OCR
	//
	SPI_READ_DATA( tmp );
	SPI_READ_DATA( tmp );
	//
	// send 8 clock pulses to the card
	//
	SPI_WRITE_DATA( 0xFF );
	//
	// send the SEND_CSD command
	//
	SEND_COMMAND( SEND_CSD );
	//
	// wait for the response
	//
	WAIT_FOR_RESPONSE( tmp, timeout, SD_SPI_TIMEOUT );
	//
	// check for timeout condition
	//
	if ( !timeout ) {
		DEASSERT_CS( );
		return SD_TIMEOUT;	
	}
	//
	// translate the response
	//
	tmp = TRANSLATE_RESPONSE( tmp );
	//
	// check for an error response
	//
	if ( tmp ) {
		SPI_WRITE_DATA( 0xFF );
		DEASSERT_CS( );
		return tmp;		
	}
	//
	// wait for the data to arrive
	// 
	WAIT_FOR_DATA( timeout, SD_SPI_TIMEOUT );
	//
	// test for timeout condition
	//
	if ( !timeout ) {
		DEASSERT_CS( );
		return SD_TIMEOUT;
	}
	//
	// read and discard the 1st byte 
	// ofthe CSD register
	//
	SPI_READ_DATA( tmp );
	//
	// read the 2nd byte of the CSD register
	// into the TAAC field of the card_info
	// structure
	//
	SPI_READ_DATA( card_info->TAAC );
	//
	// read the 3rd byte of the CSD register
	// into the NSAC field of the card_info
	// structure
	//
	SPI_READ_DATA( card_info->NSAC );
	//
	// read bytes 4-6 of the CSD register
	// discarding bytes 4 and 5
	//
	SPI_READ_DATA( tmp );
	SPI_READ_DATA( tmp );
	SPI_READ_DATA( tmp );
	//
	// the last 4 bits of the 6th byte of
	// the CSD is the READ_BL_LEN value
	//
	read_bl_len = ( tmp & 0xF );
	//
	// read the 7th byte of the CSD register
	//
	SPI_READ_DATA( tmp );
	//
	// VERSION 1:
	// bits 1-0 of the 7th byte of the CSD contains bits
	// 11-10 of the csize field
	//
	if ( card_info->Version == SD_VERSION_1_0 )
		c_size = ( ( uint16_t ) ( tmp & 0x3 ) ) << 10;
	//
	// read the 8th byte
	//
	SPI_READ_DATA( tmp );
	//
	// version 1:
	// the 8th byte of the CSD contains bits
	// 9-2 of the c_size field
	//
	if ( card_info->Version == SD_VERSION_1_0 )
		c_size |= ( ( uint16_t ) tmp ) << 2;
	//else c_size |= ( ( ( uint32_t ) tmp ) << 16 );
	
	//
	// read the 9th byte of the CSD register
	//
	SPI_READ_DATA( tmp );
	//
	// copy bits 4-2 of the 9th byte of the
	// CSD register to the R2W_FACTOR field of
	// the card_info structure
	//
	if ( card_info->Version == SD_VERSION_1_0 ) 
		card_info->R2W_FACTOR = ( tmp & 0b00011100 ) >> 2;
	//
	// version 1:
	// bits 7-6 of the 9th byte of the CSD contains
	// bits 1-0 of the c_size field
	// ---
	// bits 1-0 of the 9th byte of the CSD contains
	// bits 2-1 of the C_SIZE_MULT field
	// 
	if ( card_info->Version == SD_VERSION_1_0 ) {
		c_size |= ( ( uint16_t ) ( tmp & 0xC0 ) ) >> 6;
		c_size_mult = ( ( uint16_t ) ( tmp & 0x3 ) ) << 1;	
	}	
	//else
	//	c_size |= ( ( uint32_t ) tmp ) << 8;
	
	//
	// read the 10th byte of the CSD register
	//
	SPI_READ_DATA( tmp );
	//
	// version 1:
	// bits 7 of the 10th byte of the CSD
	// contains bit 0 of the field C_SIZE_MULT
	// which is 3 bits wide.
	//
	if ( card_info->Version == SD_VERSION_1_0 )
		c_size_mult |= ( tmp & 0x3 ) << 1;
	//else
	//	c_size |= ( ( uint32_t ) tmp );

	//
	// read the 11th byte of the CSD register
	//
	SPI_READ_DATA( tmp );
	//	
	// version 1:
	// bit 7 of the 11th byte of the CSD register
	// contains bit 0 of the C_SIZE_MULT field
	//
	if ( card_info->Version == SD_VERSION_1_0 )
		c_size_mult |= ( ( uint16_t ) ( tmp & 0x80 ) ) >> 7;
	//
	// read and bytes 12-16 of the CSD register
	//
	SPI_READ_DATA( tmp );
	SPI_READ_DATA( tmp );
	SPI_READ_DATA( tmp );
	SPI_READ_DATA( tmp );
	SPI_READ_DATA( tmp );
	//
	// read and discard the 16 bit crc
	//
	SPI_READ_DATA( tmp );
	SPI_READ_DATA( tmp );	
	//
	// send 8 clock pulses to card
	//
	SPI_WRITE_DATA( 0xFF );
	// 
	// set the clock speed to 10 mhz
	//	
	SPI_SET_PRESCALER( 0x3, 0x4 ); // 10 mhz
	//
	// calculate the SD card's capacity
	//
	if ( card_info->Version == SD_VERSION_1_0 ) {		
		//
		// BLOCK_LEN 	= 	2 ^ READ_BL_LEN
		// MULT 		= 	2 ^ ( C_SIZE_MULT + 2 )
		// BLOCK_NR 	= 	( C_SIZE + 1 ) * MULT
		// CAPACITY 	= 	BLOCK_NR * BLOCK_LEN
		//
		card_info->BlockLength 	= 0x2 << ( read_bl_len - 0x1 );
		mult 					= 0x2 << ( c_size_mult + 0x1 );
		block_nr 				= ( c_size + 0x1 ) * mult;
		card_info->Capacity 	= block_nr * card_info->BlockLength;	
	}
	else if ( card_info->Version == SD_VERSION_2_0 ) {
		//
		// CAPACITY = ( C_SIZE + 1 ) * 512
		//
		card_info->Capacity 	= ( c_size + 0x1 ) * 512;
	}
	//
	// de-assert the CS line
	//
	DEASSERT_CS( );
	//
	// return success code
	// 
	return SD_SUCCESS;
}

//
// initializes the DMA channels used
// by this driver
//
void sd_init_dma( PSD_CARD_INFO sd ) {

	//
	// configure the dma channel 0 to transfer a single
	// byte from the SPI receive buffer one time for every
	// byte that is being transfered by channel 1. This is
	// needed in order to sustain the SPI transfer since
	// SPI does not support one-way communication
	//
	DMA_CHANNEL_DISABLE( sd->DmaChannel1 );
	DMA_SET_CHANNEL_WIDTH( sd->DmaChannel1, DMA_CHANNEL_WIDTH_BYTE );
	DMA_SET_CHANNEL_DIRECTION( sd->DmaChannel1, DMA_DIR_PERIPHERAL_TO_RAM );
	DMA_SET_INTERRUPT_MODE( sd->DmaChannel1, DMA_INTERRUPT_MODE_FULL );
	DMA_SET_NULL_DATA_WRITE_MODE( sd->DmaChannel1, 0 );
	DMA_SET_ADDRESSING_MODE( sd->DmaChannel1, DMA_ADDR_REG_IND_NO_INCREMENT );
	DMA_SET_PERIPHERAL( sd->DmaChannel1, sd->BusDmaIrq );
	DMA_SET_PERIPHERAL_ADDRESS( sd->DmaChannel1, &SPIBUF );
	DMA_SET_BUFFER_A( sd->DmaChannel1, sd->WorkingBytes + SD_BLOCK_LENGTH + 0x2 );
	DMA_SET_BUFFER_B( sd->DmaChannel1, sd->WorkingBytes + SD_BLOCK_LENGTH + 0x2 );
	DMA_SET_MODE( sd->DmaChannel1, DMA_MODE_ONE_SHOT );
	DMA_SET_TRANSFER_LENGTH( sd->DmaChannel1, SD_BLOCK_LENGTH + 0x1 );
	DMA_CHANNEL_CLEAR_INTERRUPT_FLAG( sd->DmaChannel1 );
	DMA_CHANNEL_DISABLE_INTERRUPT( sd->DmaChannel1 );
	
	//
	// configure dma channel 1 to transfer a buffer
	// from system memory to the SPI buffer
	//
	DMA_CHANNEL_DISABLE( sd->DmaChannel2 );
	DMA_SET_CHANNEL_WIDTH( sd->DmaChannel2, DMA_CHANNEL_WIDTH_BYTE );
	DMA_SET_CHANNEL_DIRECTION( sd->DmaChannel2, DMA_DIR_RAM_TO_PERIPHERAL );
	DMA_SET_INTERRUPT_MODE( sd->DmaChannel2, DMA_INTERRUPT_MODE_FULL );
	DMA_SET_NULL_DATA_WRITE_MODE( sd->DmaChannel2, 0 );
	DMA_SET_ADDRESSING_MODE( sd->DmaChannel2, DMA_ADDR_REG_IND_W_POST_INCREMENT );
	DMA_SET_PERIPHERAL( sd->DmaChannel2, sd->BusDmaIrq );
	DMA_SET_PERIPHERAL_ADDRESS( sd->DmaChannel2, &SPIBUF );
	DMA_SET_BUFFER_A( sd->DmaChannel2, sd->WorkingBytes );
	DMA_SET_BUFFER_B( sd->DmaChannel2, sd->WorkingBytes );
	DMA_SET_MODE( sd->DmaChannel2, DMA_MODE_ONE_SHOT );
	DMA_SET_TRANSFER_LENGTH( sd->DmaChannel2, SD_BLOCK_LENGTH + 0x2 );
	//DMA_CHANNEL_SET_INTERRUPT( sd->DmaChannel2, &SDInterrupt );
	DMA_CHANNEL_CLEAR_INTERRUPT_FLAG( sd->DmaChannel2 );
	DMA_CHANNEL_ENABLE_INTERRUPT( sd->DmaChannel2 );
	DMA_CHANNEL_SET_INT_PRIORITY( sd->DmaChannel2, 0x5 );

}

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
uint16_t sd_write( 
	SD_CARD_INFO* card_info,	
	unsigned long address, 
	uint16_t length, 
	unsigned char* buffer, 
	uint16_t* async_state, 
	SD_CALLBACK_INFO* callback_info ) 
{
	
	unsigned char tmp;
	uint16_t i;	
	uint16_t timeout;
	uint16_t padbytes;
	uint32_t addr;
	//
	// calculate the address of the beggining
	// of the 1st block and the # of bytes that 
	// won't be read from the 1st block
	//
	padbytes = address % SD_BLOCK_LENGTH;	
	addr = ( padbytes == 0 ) ? address : address - padbytes;
	//
	// while there are bytes to write...
	//
	while ( length > 0x0 ) 
	{
		//
		// if this is a partial block write we must
		// read the original block from the card, then modify the
		// bytes being writen to before writing it back
		//
		if ( padbytes || length < SD_BLOCK_LENGTH ) 
		{
			//
			// if we need bytes from both the beggining
			// and the end of the block then we read
			// the whole block
			//
			if ( padbytes && length < SD_BLOCK_LENGTH ) 
			{
				i = sd_read( card_info, addr, 
					SD_BLOCK_LENGTH, card_info->WorkingBytes, NULL, NULL );
			}
			//
			// if we only need bytes from the beggining of
			// the block then get those bytes
			//
			else if ( padbytes )
			{
				i = sd_read( card_info, addr, padbytes, card_info->WorkingBytes, NULL, NULL );
			}
			//
			// if we only need bytes from the end of the
			// block then get them
			//
			else 
			{
				i = SD_BLOCK_LENGTH - length;
				i = sd_read( card_info, addr + i, length, card_info->WorkingBytes + i, NULL, NULL );
			}
			//
			// if the read routine returned an error
			// we return the error code
			//
			if ( i ) 
				return i;
			//
			// copy the bytes that are being modified
			// to the original block buffer
			//
			for ( i = 0; i < MIN( length, SD_BLOCK_LENGTH ); i++ )				
			{
				card_info->WorkingBytes[ i + padbytes ] = *buffer++;
			}
			//
			// substract the bytes being written from
			// the in this block from the total length
			//
			length -= MIN( length, SD_BLOCK_LENGTH ) - padbytes;
			//
			// reset the padbytes value
			//
			padbytes = 0;
		}		
		//
		// if this is a full block write...
		//
		else 
		{
			//
			// copy the new block data into the
			// SD write buffer
			//			
			for ( i = 0; i < SD_BLOCK_LENGTH; i++ )
				card_info->WorkingBytes[ i ] = *buffer++;
			//
			// substract the block length from the
			// request length
			//			
			length -= SD_BLOCK_LENGTH;
		}
		//
		// assert the CS line
		//
		ASSERT_CS( );
		//
		// send 16 clock pulses to the card
		//
		SPI_WRITE_DATA( 0xFF );
		SPI_WRITE_DATA( 0xFF );
		//
		// send the write command
		//	
		SEND_IO_COMMAND( WRITE_SINGLE_BLOCK, addr );
		//
		// read the response
		//
		WAIT_FOR_RESPONSE( tmp, timeout, SD_SPI_TIMEOUT );
		//
		// check for timeout condition
		//
		if ( !timeout ) 
		{
			DEASSERT_CS( );
			return SD_TIMEOUT;	
		}
		//
		// translate the response
		//
		tmp = TRANSLATE_RESPONSE( tmp );
		//
		// checks for an error code on the response
		//
		if ( tmp ) 
		{		
			SPI_WRITE_DATA( 0xFF );
			DEASSERT_CS( );
			return tmp;	
		}
		//
		// if this is an asynchronous write...
		//		
		if ( async_state ) 
		{
			//
			// save the write parameters to global 
			// memory
			//
			write_card_info = card_info;
			card_info->Context.pending_op_write = 1;
			card_info->Context.pending_op_address = addr;
			card_info->Context.pending_op_length = length;
			card_info->Context.pending_op_buffer = buffer;
			card_info->Context.pending_op_state = async_state;
			card_info->Context.callback = callback_info->Callback;
			card_info->Context.context = callback_info->Context;
			//
			// send the start token and let the
			// DMA do the rest
			//
			SPI_WRITE_DATA( SD_BLOCK_START_TOKEN );
			//
			// enable the dma channels
			//
			DMA_SET_CHANNEL_DIRECTION( card_info->DmaChannel1, DMA_DIR_PERIPHERAL_TO_RAM );
			DMA_SET_CHANNEL_DIRECTION( card_info->DmaChannel2, DMA_DIR_RAM_TO_PERIPHERAL );
			DMA_CHANNEL_ENABLE( card_info->DmaChannel1 );
			DMA_CHANNEL_ENABLE( card_info->DmaChannel2 );
			//
			// force the 1st byte transfer
			//
			DMA_CHANNEL_FORCE_TRANSFER( card_info->DmaChannel2 );
			//
			// set the state to SD_PROCESSING
			//
			*async_state = SD_PROCESSING;
			//
			// return the current state ( processing )
			//
			return *async_state;
		}
		//
		// if this is a synchronous write
		//
		else 
		{
			//
			// send the start token
			//
			SPI_WRITE_DATA( SD_BLOCK_START_TOKEN );
			//
			// transfer the block of data & crc
			//
			for ( i = 0; i < SD_BLOCK_LENGTH + 0x2; i++ )			
				SPI_WRITE_DATA( card_info->WorkingBytes[ i ] );
			//
			// read the data response
			//
			WAIT_FOR_DATA_RESPONSE_TOKEN( tmp, timeout, SD_SPI_TIMEOUT );
			//
			// check for timeout conditon
			//
			if ( !timeout ) 
			{
				DEASSERT_CS( );
				return SD_TIMEOUT;	
			}
			//
			// if the data was accepted then wait until
			// the card is done writing it
			//
			if ( SD_DATA_RESPONSE_ACCEPTED ==
				( tmp & SD_DATA_RESPONSE_ACCEPTED ) ) 

			{
				WAIT_WHILE_CARD_BUSY( );
			}
			//
			// if the data was rejected due to a CRC error
			// return the CRC error code
			//		
			else if ( SD_DATA_RESPONSE_REJECTED_CRC_ERROR ==
				( tmp & SD_DATA_RESPONSE_REJECTED_CRC_ERROR ) ) 
			{
				SPI_WRITE_DATA( 0xFF );
				DEASSERT_CS( );
				return SD_CRC_ERROR;
			}
			//
			// if it was rejected due to a write error
			// return the unknown error code ( for now )
			//
			else if ( SD_DATA_RESPONSE_REJECTED_WRITE_ERROR ==
				( tmp & SD_DATA_RESPONSE_REJECTED_WRITE_ERROR ) ) 
			{
				SPI_WRITE_DATA( 0xFF );
				DEASSERT_CS( );
				return SD_UNKNOWN_ERROR;		
			}
			//
			// clock out 8 cycles
			//
			SPI_WRITE_DATA( 0xFF );
			//
			// de-assert the CS line
			//
			DEASSERT_CS( );
			//
			// increase the address to point to
			// the next block start
			//
			addr += SD_BLOCK_LENGTH;
		}
	}
	return SD_SUCCESS;		
}

//
// reads an arbitrary amount of data 
// from an SD card synchronously
//
// parameters:
//
// card_info 	- structure returned by sd_init
// address 		- the address of the block to be read
// length		- the amount of bytes to read
// buffer		- a pointer to the location of the data to be read
//
// remarks: 
//
// for optimal performance both address and length should be
// multiples of 512
//
uint16_t sd_read( SD_CARD_INFO* card_info, 
	uint32_t address, uint16_t length, unsigned char* buffer, 
	uint16_t* async_state, PSD_CALLBACK_INFO callback_info ) 
{

	unsigned char tmp;
	uint16_t i;
	uint16_t len;
	uint16_t timeout;
	uint16_t padbytes;
	uint32_t addr;
	
	//
	// calculate the address of the beggining
	// of the 1st page and the # of bytes that 
	// won't be read from the 1st block
	//
	padbytes = address % SD_BLOCK_LENGTH;	
	addr = ( padbytes == 0 ) ? address : address - padbytes;
	//
	// if this is an asynchronous read...
	//
	if ( *async_state != NULL ) 
	{		
		//
		// keep doing block reads until all the requested
		// data has been written to the disk
		//	
		if ( length > 0x0 ) 
		{
			//
			// calculate the amount of bytes to be
			// read from this block
			//
			len = ( length > SD_BLOCK_LENGTH ) ? 
				SD_BLOCK_LENGTH - padbytes : length - padbytes;
			//
			// assert the CS line
			//
			ASSERT_CS( );
			//
			// send the read command
			//	
			SEND_IO_COMMAND( READ_SINGLE_BLOCK, addr );
			//
			// read the response
			//
			WAIT_FOR_RESPONSE( tmp, timeout, SD_SPI_TIMEOUT );
			//
			// check for timeout condition
			//
			if ( !timeout ) 
			{
				DEASSERT_CS( );
				return SD_TIMEOUT;	
			}
			//
			// translate the response
			//
			tmp = TRANSLATE_RESPONSE( tmp );
			//
			// check for an error response
			//
			if ( tmp != 0 ) 
			{
				SPI_WRITE_DATA( 0xFF );
				DEASSERT_CS( );
				return tmp;			
			}
			//
			// wait for the start token
			//
			WAIT_FOR_DATA( timeout, SD_SPI_TIMEOUT );
			//
			// check for timeout condition
			//
			if ( !timeout ) 
			{
				DEASSERT_CS( );
				return SD_TIMEOUT;	
			}
			//
			// set the asynchronous operation state
			//
			write_card_info = card_info;
			card_info->Context.pending_op_write = 0;
			card_info->Context.pending_op_address = addr;
			card_info->Context.pending_op_length = length;
			card_info->Context.pending_op_buffer = buffer;
			card_info->Context.pending_op_state = async_state;
			card_info->Context.pending_padbytes = padbytes;
			card_info->Context.pending_len = len;
			card_info->Context.callback = callback_info->Callback;
			card_info->Context.context = callback_info->Context;
			//
			// enable the dma channels
			//
			DMA_SET_CHANNEL_DIRECTION( card_info->DmaChannel1, DMA_DIR_RAM_TO_PERIPHERAL );
			DMA_SET_CHANNEL_DIRECTION( card_info->DmaChannel2, DMA_DIR_PERIPHERAL_TO_RAM );
			DMA_CHANNEL_ENABLE( card_info->DmaChannel1 );
			DMA_CHANNEL_ENABLE( card_info->DmaChannel2 );
			//
			// force the 1st byte transfer
			//
			DMA_CHANNEL_FORCE_TRANSFER( card_info->DmaChannel1 );
			//
			// set the state to SD_PROCESSING
			//
			*async_state = SD_PROCESSING;
			//
			// return the current state ( processing )
			//
			return *async_state;
		}
		//
		// this feature is not implemented
		//
		return SD_SUCCESS;
		
	}
	//
	// if this is a synchronous read
	//
	else 
	{
		//
		// keep doing block reads until all the requested
		// data has been written to the disk
		//	
		while ( length > 0x0 ) 
		{
			//
			// calculate the amount of bytes to be
			// read from this block
			//
			len = ( length > SD_BLOCK_LENGTH ) ? 
				SD_BLOCK_LENGTH - padbytes : length - padbytes;
			//
			// assert the CS line
			//
			ASSERT_CS( );
			//
			// send the read command
			//	
			SEND_IO_COMMAND( READ_SINGLE_BLOCK, addr );
			//
			// read the response
			//
			WAIT_FOR_RESPONSE( tmp, timeout, SD_SPI_TIMEOUT );
			//
			// check for timeout condition
			//
			if ( !timeout ) 
			{
				DEASSERT_CS( );
				return SD_TIMEOUT;	
			}
			//
			// translate the response
			//
			tmp = TRANSLATE_RESPONSE( tmp );
			//
			// check for an error response
			//
			if ( tmp != 0 ) 
			{
				SPI_WRITE_DATA( 0xFF );
				DEASSERT_CS( );
				return tmp;			
			}
			//
			// wait for the start token
			//
			WAIT_FOR_DATA( timeout, SD_SPI_TIMEOUT );
			//
			// check for timeout condition
			//
			if ( !timeout ) 
			{
				DEASSERT_CS( );
				return SD_TIMEOUT;	
			}
			//
			// for each byte on the block...
			//
			for ( i = 0; i < SD_BLOCK_LENGTH; i++ ) 
			{
				//
				// read the next byte in the block
				//
				SPI_READ_DATA( tmp );
				//
				// if this byte is not included in
				// the read request decrease the pad count
				// and discard it
				//
				if ( padbytes > 0 ) padbytes--;	
				//
				// otherwise if there are bytes left to copy
				// then copy the next byte to our buffer and
				// increase it's pointer
				//				
				else if ( len ) 
				{
					*buffer++ = tmp;
					length--;
					len--;
				}
			}
			//
			// read and discard the 16-bit crc
			//
			SPI_READ_DATA( tmp );
			SPI_READ_DATA( tmp );
			//
			// send 8 clock cycles
			//
			SPI_WRITE_DATA( 0xFF );
			//
			// de-assert the CS line
			//
			DEASSERT_CS( );
			//
			// increase the address to the
			// beggining of the next block
			//
			addr += SD_BLOCK_LENGTH;
		}
	}
	return SD_SUCCESS;
}

//
// SD driver timeslot - checks if a pending
// dma write has completed and if so it completes
// the request
//
void sd_timeslot( SD_CARD_INFO* sd ) 
{
	//
	static unsigned char tmp;
	static uint16_t i;
	//
	// if the operation is completed...
	//
	if ( sd->Context.dma_op_completed ) 
	{
		//
		// reset the dma op completed flag
		//
		INTERRUPT_PROTECT( sd->Context.dma_op_completed = 0 );
		//
		// if the operation is a write operation...
		//
		if ( sd->Context.pending_op_write == 1 ) 
		{		
			//
			// wait for the data transfer response
			// from the card
			//
			WAIT_FOR_DATA_RESPONSE_TOKEN( tmp, i, SD_SPI_TIMEOUT );
			//
			// if the operation times out...
			//
			if ( ! i ) 
			{
				//
				// de-assert the CS line
				//
				DEASSERT_CS( );
				//
				// set the operation state
				//
				*sd->Context.pending_op_state = SD_TIMEOUT;
				//
				// if a callback routine was provided with the
				// operation request the call it.
				//
				if ( sd->Context.callback != NULL ) 
					sd->Context.callback( sd->Context.pending_op_state, 
					sd->Context.context );
				//
				// relinquish the time slot
				//
				return;
			}
			//
			// if the data was accepted then wait until
			// the card is done writing it
			//
			if ( SD_DATA_RESPONSE_ACCEPTED ==
				( tmp & SD_DATA_RESPONSE_ACCEPTED ) ) 
			{
				WAIT_WHILE_CARD_BUSY( );
			}
			//
			// if the data was rejected due to a CRC error
			// return the CRC error code
			//		
			else if ( SD_DATA_RESPONSE_REJECTED_CRC_ERROR ==
				( tmp & SD_DATA_RESPONSE_REJECTED_CRC_ERROR ) ) 
			{
				//
				// send 8 clock pulses to the card
				//
				SPI_WRITE_DATA( 0xFF );
				//
				// de-assert the CS line
				//
				DEASSERT_CS( );
				//
				// set the operation state
				//
				*sd->Context.pending_op_state = SD_CRC_ERROR;
				//
				// if a callback routine was provided with the
				// operation request the call it.
				//
				if ( sd->Context.callback != NULL ) 
					sd->Context.callback( sd->Context.pending_op_state, 
					sd->Context.context );
				//
				// clear the interrupt flag
				//
				IFS0bits.DMA1IF = 0x0;
				//
				// relinquish the time slot
				//
				return;
			}
			//
			// if it was rejected due to a write error
			// return the unknown error code ( for now )
			//
			else if ( SD_DATA_RESPONSE_REJECTED_WRITE_ERROR ==
				( tmp & SD_DATA_RESPONSE_REJECTED_WRITE_ERROR ) ) 
			{
				//
				// send 8 clock pulses to the card
				//
				SPI_WRITE_DATA( 0xFF );
				//
				// de-assert the CS line
				//
				DEASSERT_CS( );
				//
				// set the operation state
				//
				*sd->Context.pending_op_state = SD_UNKNOWN_ERROR;
				//
				// if a callback routine was provided with the
				// operation request the call it.
				//
				if ( sd->Context.callback != NULL ) 
					sd->Context.callback( sd->Context.pending_op_state,	sd->Context.context );
				//
				// relinquish the time slot
				//
				return;
			}
			//
			// clock out 8 cycles
			//
			SPI_WRITE_DATA( 0xFF );
			//
			// de-assert the CS line
			//
			DEASSERT_CS( );
			//
			// if the operation is not complete
			//
			if ( sd->Context.pending_op_length ) 
			{
				//
				// increase the address to point to
				// the next block
				//
				sd->Context.pending_op_address += SD_BLOCK_LENGTH;
				//
				// if this is not a full block write...
				//
				if ( sd->Context.pending_op_length < SD_BLOCK_LENGTH ) 
				{
					//
					// read the original block ( just the part
					// that is not being overwritten
					//
					i = SD_BLOCK_LENGTH - sd->Context.pending_op_length;
					i = sd_read( sd, sd->Context.pending_op_address + i, 
						sd->Context.pending_op_length, sd->WorkingBytes + i, NULL, NULL );
					//
					// if the read failed...
					//
					if ( i ) 
					{
						//
						// set the operation state
						//
						*sd->Context.pending_op_state = i;
						//
						// if a callback routine was provided with the
						// operation request the call it.
						//
						if ( sd->Context.callback != NULL ) 
							sd->Context.callback( sd->Context.pending_op_state, sd->Context.context );
						//
						// relinquish the time slot
						//
						return;
					}
					//
					// copy the part of the block that
					// is being modified
					//
					for ( i = 0; i < sd->Context.pending_op_length; i++ )				
						sd->WorkingBytes[ i ] = *sd->Context.pending_op_buffer++;
					//
					// clear the write length
					//
					sd->Context.pending_op_length = 0x0;
				}
				//
				// decrease the write_length by
				// one block
				//
				else 
				{
					for ( i = 0; i < sd->Context.pending_op_length; i++ )
						sd->WorkingBytes[ i ] = *sd->Context.pending_op_buffer++;
					sd->Context.pending_op_length -= SD_BLOCK_LENGTH;
				}
				//
				// assert the CS line
				//
				ASSERT_CS( );
				//
				// send the write command
				//	
				SEND_IO_COMMAND( WRITE_SINGLE_BLOCK, sd->Context.pending_op_address );
				//
				// read the response
				//
				WAIT_FOR_RESPONSE( tmp, i, SD_SPI_TIMEOUT );
				//
				// check for timeout condition
				//
				if ( ! i ) 
				{
					//
					// de-assert the CS line
					//
					DEASSERT_CS( );
					//
					// set the operation state
					//
					*sd->Context.pending_op_state = SD_TIMEOUT;
					//
					// if a callback routine was provided with the
					// operation request the call it.
					//
					if ( sd->Context.callback != NULL ) 
						sd->Context.callback( sd->Context.pending_op_state, sd->Context.context );
					//
					// relinquish the time slot
					//
					return;
				}
				//
				// translate the response
				//
				tmp = TRANSLATE_RESPONSE( tmp );
				//
				// checks for an error code on the response
				//
				if ( tmp ) 
				{
					//
					// send 8 clock pulses to the card
					//
					SPI_WRITE_DATA( 0xFF );
					//
					// de-assert the CS line
					//
					DEASSERT_CS( );
					//
					// set the operation state
					//
					*sd->Context.pending_op_state = tmp;
					//
					// if a callback routine was provided with the
					// operation request the call it.
					//
					if ( sd->Context.callback != NULL ) 
						sd->Context.callback( sd->Context.pending_op_state, sd->Context.context );
					//
					// relinquish the time slot
					//
					return;
				}
				//
				// send the start token
				//
				SPI_WRITE_DATA( SD_BLOCK_START_TOKEN );
				//
				// enable the DMA channel
				//
				DMA_SET_CHANNEL_DIRECTION( sd->DmaChannel1, DMA_DIR_PERIPHERAL_TO_RAM );
				DMA_SET_CHANNEL_DIRECTION( sd->DmaChannel2, DMA_DIR_RAM_TO_PERIPHERAL );
				DMA_CHANNEL_ENABLE( sd->DmaChannel1 );
				DMA_CHANNEL_ENABLE( sd->DmaChannel2 );
				//
				// initialize the next dma transfer
				//
				DMA_CHANNEL_FORCE_TRANSFER( sd->DmaChannel2 );
				//
				// relinquish the time slot
				//
				return;
			}
			//
			// if the operation is complete...
			//
			else 
			{
				//
				// set the operation state
				//
				*sd->Context.pending_op_state = SD_SUCCESS;
				//
				// if a callback routine was provided with the
				// operation request the call it.
				//
				if ( sd->Context.callback != NULL ) 
					sd->Context.callback( sd->Context.pending_op_state, sd->Context.context );
				//
				// relinquish the time slot
				//
				return;
			}
		}
	}
	//
	// if the current operation is a read operation
	//
	else 
	{
		if ( sd->Context.dma_op_completed ) 
		{
		
			uint16_t timeout;
			//card_info->Context.pending_write_address = addr;
			//card_info->Context.pending_write_length = length;
			//card_info->Context.pending_write_buffer = buffer;
			//card_info->Context.pending_write_state = async_state;
			//card_info->Context.callback = callback_info->Callback;
			//card_info->Context.context = callback_info->Context;
			//card_info->Context.pending_padbytes = padbytes;
			//card_info->Context.pending_len = len;
	
			//
			// for each byte on the block...
			//
			for ( i = 0; i < SD_BLOCK_LENGTH; i++ ) 
			{
				//
				// if this byte is not included in
				// the read request decrease the pad count
				// and discard it
				//
				if ( sd->Context.pending_padbytes > 0 ) 
							sd->Context.pending_padbytes--;	
				//
				// otherwise if there are bytes left to copy
				// then copy the next byte to our buffer and
				// increase it's pointer
				//				
				else if ( sd->Context.pending_len ) {
					*sd->Context.pending_op_buffer++ = tmp;
					sd->Context.pending_op_length--;
					sd->Context.pending_len--;
				}
			}
			//
			// keep doing block reads until all the requested
			// data has been written to the disk
			//	
			if ( sd->Context.pending_op_length > 0x0 ) 
			{
				//
				// calculate the amount of bytes to be
				// read from this block
				//
				sd->Context.pending_len = 
					( sd->Context.pending_op_address > SD_BLOCK_LENGTH ) ? 
					SD_BLOCK_LENGTH - sd->Context.pending_padbytes : 
					sd->Context.pending_op_address - 
					sd->Context.pending_padbytes;
				//
				// assert the CS line
				//
				ASSERT_CS( );
				//
				// send the read command
				//	
				SEND_IO_COMMAND( READ_SINGLE_BLOCK, sd->Context.pending_op_address );
				//
				// read the response
				//
				WAIT_FOR_RESPONSE( tmp, timeout, SD_SPI_TIMEOUT );
				//
				// check for timeout condition
				//
				if ( !timeout ) 
				{
					//
					// deassert the CS line
					//
					DEASSERT_CS( );
					//
					// set the operation state
					//
					*sd->Context.pending_op_state = SD_TIMEOUT;
					//
					// if a callback function was supplied then
					// call it
					//
					if ( sd->Context.callback != NULL )
						sd->Context.callback( sd->Context.pending_op_state, 
						sd->Context.context );
					//
					// relinquish the timeslot
					//
					return;
				}
				//
				// translate the response
				//
				tmp = TRANSLATE_RESPONSE( tmp );
				//
				// check for an error response
				//
				if ( tmp != 0 ) 
				{
					//
					// send 1 clock cycle to the card
					//
					SPI_WRITE_DATA( 0xFF );
					//
					// deassert the CS line
					//
					DEASSERT_CS( );
					//
					// set the operation state
					//
					*sd->Context.pending_op_state = tmp;
					//
					// if a callback function was supplied then
					// call it
					//
					if ( sd->Context.callback != NULL )
						sd->Context.callback( sd->Context.pending_op_state, sd->Context.context );
					//
					// relinquish the timeslot
					//
					return;
				}
				//
				// wait for the start token
				//
				WAIT_FOR_DATA_ASYNC( timeout, 
					SD_SPI_TIMEOUT, sd->Context.pending_op_state );
				//
				// check for timeout condition
				//
				if ( !timeout ) 
				{
					//
					// deassert the CS line
					//
					DEASSERT_CS( );
					//
					// set the operation state
					//
					*sd->Context.pending_op_state = SD_TIMEOUT;
					//
					// if a callback function was supplied then
					// call it
					//
					if ( sd->Context.callback != NULL )
						sd->Context.callback( sd->Context.pending_op_state, sd->Context.context );
					//
					// return
					//
					return;	
				}
				//
				// enable the dma channel
				//
				DMA_SET_CHANNEL_DIRECTION( sd->DmaChannel1, DMA_DIR_RAM_TO_PERIPHERAL );
				DMA_SET_CHANNEL_DIRECTION( sd->DmaChannel2, DMA_DIR_PERIPHERAL_TO_RAM );
				DMA_CHANNEL_ENABLE( sd->DmaChannel1 );
				DMA_CHANNEL_ENABLE( sd->DmaChannel2 );
				//
				// force the 1st byte transfer
				//
				DMA_CHANNEL_FORCE_TRANSFER( sd->DmaChannel2 );
				//
				// set the state to SD_PROCESSING
				//
				*sd->Context.pending_op_state = SD_PROCESSING;
				//
				// increase the address to the
				// beggining of the next block
				//
				sd->Context.pending_op_address += SD_BLOCK_LENGTH;
				//
				// return the current state ( processing )
				//
				return;
			}
			//
			// read and discard the 16-bit crc
			//
			SPI_READ_DATA( tmp );
			SPI_READ_DATA( tmp );
			//
			// send 8 clock cycles
			//
			SPI_WRITE_DATA( 0xFF );
			//
			// de-assert the CS line
			//
			DEASSERT_CS( );
			//
			// this feature is not implemented
			//
			*sd->Context.pending_op_state = SD_SUCCESS;
			//
			// if a callback function was supplied then
			// call it
			//
			if ( sd->Context.callback != NULL )
				sd->Context.callback( sd->Context.pending_op_state, sd->Context.context );
			//
			// return
			//
			return;
		}	
	}
}	

// 
// dma channel 1 interrupt
//
void __attribute__ ( ( __interrupt__, __no_auto_psv__ ) ) _DMA1Interrupt( void );
void __attribute__ ( ( __interrupt__, __no_auto_psv__ ) ) _DMA1Interrupt( void ) 
{
	//
	// mark the dma write as completed
	//
	write_card_info->Context.dma_op_completed = 1;
	
	DMA_CHANNEL_DISABLE( write_card_info->DmaChannel1 );
	DMA_CHANNEL_DISABLE( write_card_info->DmaChannel2 );
	
	//
	// clear the interrupt flag
	//
	IFS0bits.DMA1IF = 0x0;
}

