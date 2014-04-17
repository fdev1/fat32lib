/*
 * fat32lib_file_io_example - Example for using Fat32 lib
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

//#define USE_STDIO
#define USE_SM_IO
/* #define USE_ASYNC_IO; */
#define USE_STREAM_IO
#define STREAM_SIZE_32MB
//#define USE_ILI9341
#define EXPLORER16

/*
// pin configuration
*/
#if defined(EXPLORER16)
	#define SD_CARD_SPI_MODULE					2			/* SPI module for SD card */
	#define SD_CARD_CS_LINE_PORT				PORTG		/* SD card CS line port */
	#define SD_CARD_CS_LINE_TRIS				TRISG		/* SD card CS line tris */
	#define SD_CARD_CS_LINE_LAT					LATG		/* SD card CS line lat */
	#define SD_CARD_CS_LINE_PIN					3			/* SD card CS line pin # */
	
	#define SD_CARD_CD_LINE_PORT				PORTF		/* SD card C/D (card detect) line port */
	#define SD_CARD_CD_LINE_TRIS				TRISF		/* SD card C/D (card detect) line tris */
	#define SD_CARD_CD_LINE_LAT					LATF		/* SD card C/D (card detect) line lat */
	#define SD_CARD_CD_LINE_PIN					2			/* SD card C/D (card detect) line pin # */
	
	#define SD_CARD_ACTIVITY_LED_PORT 			PORTA 		/* SD card activity LED port */
	#define SD_CARD_ACTIVITY_LED_TRIS			TRISA		/* SD card activity LED tris */
	#define SD_CARD_ACTIVITY_LED_LAT			LATA		/* SD card activity LED lat */
	#define SD_CARD_ACTIVITY_LED_PIN			5			/* SD card activity LED pin # */
	
	/*
	// heartbeat LED
	*/
	#define HEARTBEAT 							LATAbits.LATA7
	#define HEARTBEAT_TRIS						TRISAbits.TRISA7
	
#else

	#define SD_CARD_SPI_MODULE					1
	#define SD_CARD_CS_LINE_PORT				PORTB
	#define SD_CARD_CS_LINE_TRIS				TRISB
	#define SD_CARD_CS_LINE_LAT					LATB
	#define SD_CARD_CS_LINE_PIN					11
	
	
	#define SD_CARD_CD_LINE_PORT				PORTA
	#define SD_CARD_CD_LINE_TRIS				TRISA
	#define SD_CARD_CD_LINE_LAT					LATA
	#define SD_CARD_CD_LINE_PIN					3
	
	#define SD_CARD_ACTIVITY_LED_PORT 			PORTB
	#define SD_CARD_ACTIVITY_LED_TRIS			TRISB
	#define SD_CARD_ACTIVITY_LED_LAT			LATB
	#define SD_CARD_ACTIVITY_LED_PIN			6
	
	#define HEARTBEAT 							LATBbits.LATB12
	#define HEARTBEAT_TRIS						TRISBbits.TRISB12
	
	/*
	// pin to enable microphone circuit. This is used
	// by custom board only
	*/
	#define USE_MICEN
	#define MICEN								LATBbits.LATB15
	#define MICEN_TRIS							TRISBbits.TRISB15
#endif
 

 
#if defined(USE_ILI9341)
#include <ili9341.h>
#include <lg.h>
#elif defined(EXPLORER16)
#include <lcd.h>
#endif

#include <common.h>
#include <rtc.h>
#include <spi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "..\sdlib\sd.h"
#include "..\fat32lib\storage_device.h"
#include "..\fat32lib\fat.h"
#include "..\fat32lib\filesystem_interface.h"
#include "..\smlib\sm.h"

//
// configuration bits
//
#if defined(EXPLORER16)
_FOSCSEL(FNOSC_PRIPLL & IESO_OFF);
_FOSC(POSCMD_XT & OSCIOFNC_ON & FCKSM_CSDCMD);
_FWDT(FWDTEN_OFF);
_FPOR(FPWRT_PWR128);
#else
_FOSCSEL(FNOSC_FRCPLL & IESO_OFF);
_FOSC(POSCMD_NONE & OSCIOFNC_ON & FCKSM_CSDCMD & IOL1WAY_OFF);
_FWDT(FWDTEN_OFF);
#if defined(__dsPIC33E__)
_FPOR(ALTI2C1_OFF & ALTI2C2_OFF & WDTWIN_WIN25);
_FICD(ICS_PGD3 & JTAGEN_OFF);
#else
_FPOR(FPWRT_PWR128);
#endif
#endif

/*
// constants for stream size
*/
#define CHUNK_COUNT_4MB		(1024L * 2 * 4)
#define CHUNK_COUNT_32MB	(4096L * 2 * 8)
#define ALLOC_SIZE_4MB		(1024L * 1024 * 4)
#define ALLOC_SIZE_32MB		(4096L * 4096L * 2L)
#if defined(STREAM_SIZE_32MB)
#define CHUNK_COUNT			CHUNK_COUNT_32MB
#define ALLOC_SIZE			ALLOC_SIZE_32MB
#else
#define CHUNK_COUNT			CHUNK_COUNT_4MB
#define ALLOC_SIZE			ALLOC_SIZE_4MB
#endif

//
// global variables
//
#if defined(USE_ILI9341)
int16_t time_label;
int16_t date_label;
int16_t volume_icon;
#elif defined(EXPLORER16)
LCD_CONFIG lcd_driver;
#endif																// lcd driver
SD_DRIVER sd_card;																	// sd card driver
#if defined(__dsPIC33F__)
unsigned char __attribute__((space(dma), section(".sd_driver_dma_buffer"))) dma_buffer[512];	// DMA buffer for async/stream io
char __attribute__((space(dma), section(".sd_driver_dma_byte"))) dma_byte;					// sd driver needs 1 byte of dma memory
#else
unsigned char __attribute__((section(".sd_driver_dma_buffer"))) dma_buffer[512];	// DMA buffer for async/stream io
char __attribute__((section(".sd_driver_dma_byte"))) dma_byte;					// sd driver needs 1 byte of dma memory
#endif

#if defined(USE_STREAM_IO)
SM_FILE file;				// file handle for stream write
uint16_t err;				// result of stream write
unsigned char buff[512];	// buffer for stream op
uint32_t filewrites = 0;
uint32_t data_value = 0;
#endif

#if defined(USE_ASYNC_IO)
uint16_t err1;				// result of async write
uint16_t err2;
uint16_t err3;
SM_FILE file1;				// file handles for async writes
SM_FILE file2;
SM_FILE file3;
unsigned char buff1[512]; 	// buffer for async writes
unsigned char buff2[512]; 	// buffer for async writes
unsigned char buff3[512]; 	// buffer for async writes
uint16_t file1writes = 0;
uint16_t file2writes = 0;
uint16_t file3writes = 0;
uint16_t async_value1 = 0;
uint16_t async_value2 = 0;
uint16_t async_value3 = 0;
#endif

//
// function prototypes
//
void init_cpu();
void init_uart();
void init_lcd();
void init_fs();
void idle_processing();
void file_test();
void file_test2();
void volume_mounted(char* volume_label);
void volume_dismounted(char* volume_label);
void file_write_callback(SM_FILE* file, uint16_t* result);
void file_write_stream_callback(SM_FILE* f, uint16_t* result, unsigned char** buffer, uint16_t* response);
void init_pins();
//
// entry point
//
int main()
{
	//
	// clock th/e cpu and mount the filesystem
	//
	init_cpu();
	/*
	// initialize pins
	*/
	init_pins();
	/*
	// initialize uart (for printf)
	*/
	#if defined(EXPLORER16)
	init_uart();
	#endif
	/*
	// initialize real-time clock
	*/
	#if defined(__dsPIC33F__)
	rtc_init(40000000);
	#elif defined(__dsPIC33E__)
	rtc_init(70000000);
	#else
	#error !!
	#endif
	/*
	// initialize lcd
	*/
	#if defined(EXPLORER16) || defined(USE_ILI9341)
	init_lcd();
	#endif
	/*
	// initialize filesystem	
	*/
	init_fs();
	/*
	// do background processing
	*/
	while (1) 
	{
		/*
		// workaround for ILI9341 driver bug
		*/
		#if defined(USE_ILI9341)
			static char painted = 0;
			if (!painted)
			{
				if (!ili9341_is_painting())
				{
					ili9341_paint();
					painted = 1;
				}
			}
		#endif
		/*
		// app tasks
		*/
		idle_processing();
		
	}
}

//
// initialize cpu
//
void init_cpu()
{
	#if defined(__dsPIC33F__)
		//
		// Configure oscillator to operate the device at 40Mhz
		// using the primary oscillator with PLL and a 8 MHz crystal
		// on the explorer 16
		//
		// Fosc = Fin * M / ( N1 * N2 )
		// Fosc = 8M * 32 / ( 2 * 2 ) = 80 MHz
		// Fcy = Fosc / 2 
		// Fcy = 80 MHz / 2
		// Fcy = 40 MHz
		//
		PLLFBDbits.PLLDIV = 38;
		CLKDIVbits.PLLPOST = 0;		
		CLKDIVbits.PLLPRE = 0;	
		//
		// switch to primary oscillator
		//	
		//clock_sw();
		//
		// wait for PLL to lock
		//
		while (OSCCONbits.LOCK != 0x1);
		
	#elif defined(__dsPIC33E__)
		//
		// Fosc = Fin * ((PLLDIV + 2) / ((PLLPRE + 2) * 2 * (PLLPOST + 1)))
		// Fosc = 8 * ((68 + 2) / (2 * 2 * 1)) = 140
		// Fcy = Fosc / 2
		// Fcy = 140 / 2
		// Fcy = 70 MHz
		//
		PLLFBDbits.PLLDIV = 68;
		CLKDIVbits.PLLPRE = 0;
		CLKDIVbits.PLLPOST = 0;
		
		while (OSCCONbits.LOCK != 0x1);
	#endif
}

/*
// configure pps
*/
void init_pins()
{
	#if defined(EXPLORER16)
		/*
		// configure all IO pins as digital
		*/
		AD1PCFGH = 0xffff;
		AD2PCFGL = 0Xffff;	
		AD1PCFGL = 0Xffff;
		/*
		// configure io ports
		*/	
		IO_PORT_SET_AS_OUTPUT(A);	// LEDs
		IO_PIN_SET_AS_INPUT(D, 6);	/* this will be our dismount button */
		IO_PIN_WRITE(D, 6, 1);	
		
	#else /* custom board */
	
		/*
		// configure all IO pins as digital
		*/
		#if defined(__dsPIC33F__)
			AD1PCFGL = 0xFFFF;
		#elif defined(__dsPIC33E__)
			ANSELA = 0;
			ANSELB = 0;
		#else
			#error !!
		#endif
		/*
		// disable MIC circuit (custom board)
		*/
		#if defined(MICEN)
			MICEN_TRIS = 0;
			MICEN = 0;
		#endif
		/*
		// configure PPS
		*/
		#if defined(__dsPIC33E__)
			/*
			// configure SPI2 for ILI9341
			*/		
			#if defined(USE_ILI9341)
				RPOR0bits.RP35R = 0b001000; // SDO
				RPOR4bits.RP43R = 0b001001; // SCK
				IO_PIN_SET_AS_OUTPUT(B, 5);
				IO_PIN_SET_AS_OUTPUT(B, 3);
			#endif
			/*
			// lock pin configuration
			*/	
			OSCCONbits.IOLOCK = 1;
		
		#elif defined(__dsPIC33F__)
		
			/*
			// configure SPI2 for ILI9341
			*/
			#if defined(USE_ILI9341)
				RPOR2bits.RP5R = 0b01011;		/* SCK */
				RPOR1bits.RP3R = 0b1010;		/* SDO */
				IO_PIN_SET_AS_OUTPUT(B, 5);
				IO_PIN_SET_AS_OUTPUT(B, 3);
			#endif
			/*
			// configure SPI1 for SD card
			*/
			RPINR20bits.SDI1R = 0X9;		/* SDI */
			RPOR3bits.RP7R = 0b01000;		/* SCK */
			RPOR4bits.RP8R = 0b00111;		/* SDO */
			IO_PIN_SET_AS_OUTPUT(B, 7);
			IO_PIN_SET_AS_OUTPUT(B, 8);
			IO_PIN_SET_AS_INPUT(B, 9);
			/*
			// lock pin configuration
			*/
			__builtin_write_OSCCONL(0X46);
			__builtin_write_OSCCONH(0X57);
			OSCCONbits.IOLOCK = 1;
		#endif
	#endif
	/*
	// heartbeat LED
	*/
	HEARTBEAT_TRIS = 0;
	HEARTBEAT = 1;
}

#if defined(EXPLORER16)
void init_uart()
{
	U2BRG = 259; //4; //(40000000L / (16 * 460800)) - 1;
	U2MODEbits.BRGH = 0;
	U2MODEbits.UARTEN = 1;
	U2MODEbits.RTSMD = 0;
	
	printf("Welcome\r\n");
	/* while (IO_PIN_READ(D, 6)); */
	printf("Starting\r\n");
}
#endif

//
// initialize LCD
//
void init_lcd() 
{
	#if defined(USE_ILI9341)
		ili9341_init();
		lg_init((LG_DISPLAY_PAINT)&ili9341_paint, 
			(LG_DISPLAY_PAINT_PARTIAL) &ili9341_paint_partial);
		/*
		// set the background color
		*/
		lg_set_background(0x0000FF); 
		/*
		// add labels for date/time and volume mounted icon
		*/
		time_label = lg_label_add((unsigned char*) "10:30:00", 0, 1, 1, 0xFFFFFF, 220, 12);
		date_label = lg_label_add((unsigned char*) "00/00/2000", 0, 1, 1, 0xFFFFFF, 125, 12);
		volume_icon = lg_label_add((unsigned char*) "\1", 0, 1, 2, 0xFFFFFF, 298, 12);
		lg_label_add((unsigned char*) "(c)2014 Fernando Rodriguez", 0, 1, 2, 0xFFFFFF, 28, 220);
		lg_label_set_visibility(volume_icon, 0);
	#elif defined(EXPLORER16)
		// 
		// For Explorer 16 board:
		//
		// RS -> RB15
		// E  -> RD4
		// RW -> RD5
		// DATA -> RE0 - RE7   
		// ---
		//
		// set the pins used by the LCD as outputs
		//
		IO_PORT_SET_DIR_MASK(E, IO_PORT_GET_DIR_MASK(E) & 0xFF00);
		IO_PIN_SET_AS_OUTPUT(D, 5);
		IO_PIN_SET_AS_OUTPUT(B, 15);
		IO_PIN_SET_AS_OUTPUT(D, 4);
		IO_PIN_WRITE(D, 5, 0);
		IO_PIN_WRITE(B, 15, 0);
		//
		// configure the LCD pins
		//
		lcd_driver.DataLine = (unsigned char*) &LATE; 	// DATA line connected to pins 7-0 on port E
		BP_INIT(lcd_driver.RWLine, &LATD, 5);			// RW line connected to pin 5 on port D
		BP_INIT(lcd_driver.RSLine, &LATB, 15);			// RS line connected to pin 15 on port B
		BP_INIT(lcd_driver.ELine, &LATD, 4);			// E line connected to pin 4 on port D
		//
		// set the VDD rise delay (ms)
		//
		lcd_driver.VddRiseDelay = 0x1;
		//
		// initialize the LCD driver
		//
		lcd_init(&lcd_driver);
	#endif
}

//
// initialize the filesystem drivers
//
void init_fs()
{
	BIT_POINTER cs;
	BIT_POINTER media_ready;
	BIT_POINTER busy_signal;
	STORAGE_DEVICE storage_device;
	FILESYSTEM fat_filesystem;
	//
	// configure SD card pins
	//
	SD_CARD_CS_LINE_TRIS &= ~(1 << SD_CARD_CS_LINE_PIN);			/* set CS line as output */
	SD_CARD_CD_LINE_TRIS |= (1 << SD_CARD_CD_LINE_PIN);				/* set CD line as input */
	SD_CARD_ACTIVITY_LED_TRIS &= ~(1 << SD_CARD_ACTIVITY_LED_PIN);	/* set disk activity led line as output */
	SD_CARD_ACTIVITY_LED_LAT &= ~(1 << SD_CARD_ACTIVITY_LED_PIN);	/* set disk activity led off */
	SD_CARD_CD_LINE_LAT |= (1 << SD_CARD_CD_LINE_PIN);				/* set CD line latch to high */
	/*
	// initialize bit pointers for sd driver
	*/
	BP_INIT(busy_signal, &SD_CARD_ACTIVITY_LED_LAT, SD_CARD_ACTIVITY_LED_PIN);
	BP_INIT(media_ready, &SD_CARD_CD_LINE_PORT, SD_CARD_CD_LINE_PIN);
	BP_INIT(cs, &SD_CARD_CS_LINE_LAT, SD_CARD_CS_LINE_PIN);
	/*
	// set the priority of the driver's DMA channels
	*/
	DMA_CHANNEL_SET_INT_PRIORITY(DMA_GET_CHANNEL(0), 0x6);
	DMA_CHANNEL_SET_INT_PRIORITY(DMA_GET_CHANNEL(1), 0x6);
	
	/*
	// initialize SD card driver
	*/
	sd_init
	(
		&sd_card, 				// pointer to driver handle
		SPI_GET_MODULE(SD_CARD_SPI_MODULE),
		DMA_GET_CHANNEL(0), 	// 1st DMA channel (interrupt must be configured for this channel)
		DMA_GET_CHANNEL(1), 	// 2nd DMA channel (interrupt must be configured for this channel)
		dma_buffer,				// optional async buffer (DMA memory)
		&dma_byte, 				// 1 byte of dma memory
		media_ready, 			// bit-pointer to pin that rises when card is on slot
		cs,						// bit-pointer to pin where chip select line is connected
		busy_signal,			// bit-pointer to IO indicator LED
		34						// device id
	);
	//
	// get the STORAGE_DEVICE interface for the SD card
	// driver and the FILESYSTEM interface for the FAT driver
	//
	sd_get_storage_device_interface(&sd_card, &storage_device);
	fat_get_filesystem_interface(&fat_filesystem);
	//
	// register the FAT driver with smlib
	//
	fat_init();
	sm_register_filesystem(&fat_filesystem);
	//
	// register the SD device with smlib as drive x:
	// anytime a card is inserted it will be automatically
	// mounted as drive x:
	//
	sm_register_storage_device(&storage_device, "x:");
	//
	// register a callback function to receive notifications
	// when a new drivve is mounted.
	//
	sm_register_volume_mounted_callback(&volume_mounted);
	sm_register_volume_dismounted_callback(&volume_dismounted);
}

//
// callback function to receive notifications when
// a new drive is mounted
//
void volume_mounted(char* volume_label)
{
	/*
	// lignt LED to indicate that drive is mounted
	*/
	#if defined(USE_ILI9341)
		lg_label_set_visibility(volume_icon, 1);
	#elif defined(EXPLORER16)
		IO_PIN_WRITE(A, 6, 1);
	#else
		//IO_PIN_WRITE(A, 2, 1);
	#endif
	/*
	// perform file io tests
	*/
	file_test();
}

/*
// callback function to receive notification when
// a volume is dismounted.
*/
void volume_dismounted(char* volume_label)
{
	/*
	// turn off the drive mounted indicator LED
	*/
	#if defined(USE_ILI9341)
		lg_label_set_visibility(volume_icon, 0);
	#elif defined(EXPLORER16)
		IO_PIN_WRITE(A, 6, 0);
	#else
		//IO_PIN_WRITE(A, 2, 0);
	#endif
}

//
// filesystem tests
//
void file_test()
{
	#if defined(USE_STDIO) || defined(USE_SM_IO)
	char hello[] = "Hello World.";
	#endif
	#if defined(USE_ASYNC_IO) || defined(USE_STREAM_IO)
	uint16_t i;
	#endif

	#if defined(USE_STDIO)
	FILE* f;
	/*
	// write a file in text mode (this gets corrupted with non-legacy libc)
	// due to bug in libc
	*/
	f = fopen("x:\\file1.txt", "w");
	if (f)
	{
		fwrite(hello, sizeof(char), strlen(hello), f);
		fclose(f);
	}
	/*
	// write a file in binary mode 
	*/
	f = fopen("x:\\file1_binary.txt", "wb");
	if (f)
	{
		fwrite(hello, sizeof(char), strlen(hello), f);
		fclose(f);
	}
	#endif
	#if defined(USE_SM_IO)
	uint16_t r;
	SM_FILE smfile;
	/*
	// write a file using smlib API
	*/
	r = sm_file_open(&smfile, "x:\\file1.txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	if (r == SM_SUCCESS)
	{
		r = sm_file_write(&smfile, (unsigned char*) hello, sizeof(hello));
		r = sm_file_close(&smfile);
	}
	#endif
	/*
	// reset the data value
	*/
	#if defined(USE_ASYNC_IO)
	async_value1 = 0;
	async_value2 = 0;
	async_value3 = 0;
	#endif
	#if defined(USE_STREAM_IO)
	data_value = 0;
	#endif
	/*
	// fill buffer with data
	*/
	#if defined(USE_ASYNC_IO) || defined(USE_STREAM_IO)
	for (i = 0; i < 512; i += 4)
	{
		#if defined(USE_STREAM_IO)
		*((uint32_t*) &buff[i]) = data_value++;
		#endif
		#if defined(USE_ASYNC_IO)
		*((uint32_t*) &buff1[i]) = async_value1++;
		*((uint32_t*) &buff2[i]) = async_value2++;
		*((uint32_t*) &buff3[i]) = async_value3++;
		#endif
	}
	#endif
	/*
	// reset the file writes count
	*/
	#if defined(USE_STREAM_IO)
	filewrites = 0;
	#endif
	#if defined(USE_ASYNC_IO)
	file1writes = 0;
	file2writes = 0;
	file3writes = 0;
	#endif
	/*
	// start an asynchronous write.
	// since our driver supports asynchronous writes from DMA memory
	// only we must either define an async buffer (off dma memory) when we call sd_init or
	// we must call sm_file_set_buffer to set the buffer of the
	// file to one that is stored in DMA memory. This buffer must be
	// MAX_SECTOR_LENGTH bytes (almost always 512 bytes).
	*/
	#if defined(USE_ASYNC_IO)
	err1 = sm_file_open(&file1, "x:\\xxx1.txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	err2 = sm_file_open(&file2, "x:\\xxx2.txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);
	err3 = sm_file_open(&file3, "x:\\xxx3.txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE);

	if (err1 || err2 || err3)
	{
		#if defined(EXPLORER16)
		printf("Error opening file: 0x%x", err);
		#endif
		return;
	}	
	/*
	// start 3 async writes
	*/
	/* sm_file_set_buffer(&file1, dma_buffer); */
	sm_file_write_async(&file1, buff1, 512, &err1, (void*) &file_write_callback, &file1);
	sm_file_write_async(&file2, buff2, 512, &err2, (void*) &file_write_callback, &file2);
	sm_file_write_async(&file3, buff3, 512, &err3, (void*) &file_write_callback, &file3);
	#endif
	
	#if defined(USE_STREAM_IO)

	/*
	// open file for stream io
	*/
	err = sm_file_open(&file, "x:\\stream.txt", SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE | SM_FILE_FLAG_NO_BUFFERING);
	if (err)
	{
		#if defined(EXPLORER16)
		printf("Error opening file: 0x%x", err);
		#endif
		return;
	}	
	/*
	// allocate 32 MB for file
	*/
	err = sm_file_alloc(&file, ALLOC_SIZE);
	if (err)
	{
		#if defined(EXPLORER16)
		printf("Error allocating space: 0x%x", err);
		#endif
		return;
	}
	/*
	// begin writing. This will continue to write in chunks of 4 KB until the
	// response argument of the callback function (file_write_stream_callback) is set
	// to stop
	*/
	err = sm_file_write_stream(&file, buff, 512, &err, (SM_STREAM_CALLBACK) &file_write_stream_callback, &file);
	if (err != FILESYSTEM_OP_IN_PROGRESS)
	{
		#if defined(EXPLORER16)
		printf("Error starting stream: 0x%x", err);
		#endif
		return;
	}
	#endif
}

/*
// asynchronous write callback function (called when write
// is completed).
*/
#if defined(USE_ASYNC_IO)
void file_write_callback(SM_FILE* f, uint16_t* result)
{
	int i;
	if (f == &file1)
	{
		file1writes++;
		if (file1writes == 80)
		{
			/*
			// close the file
			*/
			sm_file_close(f);
		}
		else
		{
			for (i = 0; i < 512; i += 4)
			{
				*((uint32_t*) &buff1[i]) = async_value1++;
			}
			sm_file_write_async(&file1, buff1, 512, &err1, (void*) &file_write_callback, &file1);
		}
	}
	else if (f == &file2)
	{
		file2writes++;
		if (file2writes == 80)
		{
			/*
			// close the file
			*/
			sm_file_close(f);
		}
		else
		{
			for (i = 0; i < 512; i += 4)
			{
				*((uint32_t*) &buff2[i]) = async_value2++;
			}
			sm_file_write_async(&file2, buff2, 512, &err2, (void*) &file_write_callback, &file2);
		}
	}
	else 
	{
		file3writes++;
		if (file3writes == 80)
		{
			/*
			// close the file
			*/
			sm_file_close(f);
		}
		else
		{
			for (i = 0; i < 512; i += 4)
			{
				*((uint32_t*) &buff3[i]) = async_value3++;
			}
			sm_file_write_async(&file3, buff3, 512, &err3, (void*) &file_write_callback, &file3);
		}
	}
}
#endif

/*
// callback for stream write. This is the callback function for sm_file_write_stream. It is
// called by the file system driver everytime it finishes writing a chunk. Here you must either reload
// the buffer or change the pointer to a new buffer and set *response to SM_STREAM_RESPONSE.READY to continue
// writing or SM_STREAM_RESPONSE_SKIP to surrender the IO device. Once the device becomes available
// again this function will be called again. To finish writing set *response to SM_STREAM_RESPONSE_STOP and return. After
// the file system driver receives the SM_STREAM_RESPONSE_STOP this function will be called one more time with
// *result set to either SM_SUCCESS or an error code.
*/
#if defined(USE_STREAM_IO)
void file_write_stream_callback(SM_FILE* f, uint16_t* result, unsigned char** buffer, uint16_t* response)
{
	static int i;	
	
	if (*result == FILESYSTEM_AWAITING_DATA)
	{
		/*
		// if we got data set the response to READY otherwise set it to STOP
		*/
		if (++filewrites < CHUNK_COUNT)
		{
			for (i = 0; i < 512; i += 4)
			{
				*((uint32_t*)&buff[i]) = data_value++;
			}
			*response = FAT_STREAMING_RESPONSE_READY;
		}
		else
		{
			*response = FAT_STREAMING_RESPONSE_STOP;
		}
	}
	else
	{
		if (*result != SM_SUCCESS)
		{
			_ASSERT(0);
			#if defined(EXPLORER16)
			printf("Stream error: 0x%x\r\n", *result);
			#endif
		}
		/*
		// close the file
		*/
		sm_file_close(f);
		/*
		// file read test
		*/
		#if defined(VERIFY_DATA)
		file_test2();
		#endif
	}
}
#endif

/*
// this function checks that the file was written correctly.
// if it fails it will break on your debugger. If this happens it
// is mostlikely a bad SD card. If you have this problem with
// more than one SD card contact the developer.
*/
#if defined(USE_STREAM_IO)
void file_test2()
{
	uint16_t i;
	uint32_t bytes_read = 0;
	uint32_t last_value = 0;
	/*
	// open file for stream io
	*/
	err = sm_file_open(&file, "x:\\stream.txt", SM_FILE_ACCESS_READ);
	if (err)
	{
		#if defined(EXPLORER16)
			printf("Error opening file for reading: 0x%x", err);
		#else
			_ASSERT(0);
		#endif
		return;
	}
	/*
	// read 512 bytes from file
	*/
	err = sm_file_read(&file, buff, 512, &bytes_read);
	if (err)
	{
		#if defined(EXPLORER16)
			printf("Error reading file: 0x%x", err);
		#else
			_ASSERT(0);
		#endif
		sm_file_close(&file);
		return;
	}
	
	while (bytes_read)
	{
		/*
		// verify that the bytes where written correctly
		*/
		for (i = 0; i < 512 / 4; i++)
		{
			if (((uint32_t*) buff)[i] != last_value++)
			{
				sm_file_close(&file);
				#if defined(EXPLORER16)
					printf("File corrupted.\r\n");
				#else
					_ASSERT(0);
				#endif
				return;
			}
		}
		/*
		// read the next 512 bytes
		*/
		err = sm_file_read(&file, buff, 512, &bytes_read);
		if (err)
		{
			#if defined(EXPLORER16)
				printf("Error opening file for reading: 0x%x\r\n", err);
			#else
				_ASSERT(0);				
			#endif
			/*
			// close file
			*/
			sm_file_close(&file);
			return;
		}
	}
	/*
	// close the file
	*/
	sm_file_close(&file);

}
#endif

/*
// background processing routine
*/
void idle_processing()
{
	static time_t last_time = 0;
	static struct tm* timeinfo;
	static char time_string[9];
	static char date_string[11];
	#if defined(EXPLORER16)
	static time_t unmount_pressed_time = 0;
	#endif
	/*
	// this code runs once per second. it updates LCD time and
	// disk indicators
	*/
	if (last_time < time(0))
	{
		/*
		// update lcd
		*/
		time(&last_time);
		/*
		// toggle heartbeat LED
		*/
		HEARTBEAT ^= 1;
		/*
		// update the date and time
		*/	
		timeinfo = localtime(&last_time);
		sprintf(date_string, "%02d/%02d/%d", timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_year + 1900);
		sprintf(time_string, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
		/*
		// write time and date to lcd
		*/
		#if defined(USE_ILI9341)
			lg_label_set_string(time_label, (unsigned char*) time_string);
			lg_label_set_string(date_label, (unsigned char*) date_string);
		#elif defined(EXPLORER16)
			lcd_set_pos(&lcd_driver, 0, 3);
			lcd_write_string(&lcd_driver, (unsigned char*) date_string);
			lcd_set_pos(&lcd_driver, 1, 4);
			lcd_write_string(&lcd_driver, (unsigned char*) time_string);
		#endif
		/*
		// toggle disk activity indicator
		*/
		#if defined(USE_ILI9341)
			if (IO_PIN_READ(B, 6))
			{
				lg_label_set_color(volume_icon, HEARTBEAT ? 0xFFFFFF : 0xFF0000);
			}	
			else
			{
				lg_label_set_color(volume_icon, 0xFFFFFF);
			}
		#elif defined(EXPLORER16)
			if (IO_PIN_READ(A, 5))
			{
				lcd_set_pos(&lcd_driver, 0, 15);
				lcd_write_char(&lcd_driver, (IO_PIN_READ(A, 7)) ? 0xF3 : ' ');
			}
		#endif		
	}
	/*
	// check for dismount button press
	*/
	#if defined(EXPLORER16)
		if (!IO_PIN_READ(D, 6))
		{
			if (!unmount_pressed_time)
			{
				time(&unmount_pressed_time);
			}
			else if (unmount_pressed_time + 2 < time(0))
			{
				sm_dismount_volume("x:");
			}
		}
		else
		{
			unmount_pressed_time = 0;
		}
	#endif
	/*
	// update media ready LCD indicator
	*/
	#if defined(EXPLORER16)
		if ((SD_CARD_CD_LINE_PORT & (1 << SD_CARD_CD_LINE_PIN)) == 0)
		{
					
			if ((SD_CARD_ACTIVITY_LED_PORT & (1 << SD_CARD_ACTIVITY_LED_PIN)) == 0)
			{
				lcd_set_pos(&lcd_driver, 0, 15);
				lcd_write_char(&lcd_driver, 0xF3);
			}
		}
		else
		{
			lcd_set_pos(&lcd_driver, 0, 15);
			lcd_write_char(&lcd_driver, 0x20);
		}
	#endif
	/*
	// lcd driver process
	*/
	#if defined(USE_ILI9341)
		ili9341_do_processing();
	#elif defined(EXPLORER16)
		lcd_idle_processing(&lcd_driver);
	#endif
	/*
	// SD driver process
	*/
	sd_idle_processing(&sd_card);
}

/*
// DMA interrupts for SD driver
*/
void __attribute__((__interrupt__, __no_auto_psv__)) _DMA0Interrupt(void) 
{
	SD_DMA_CHANNEL_1_INTERRUPT(&sd_card);
}

void __attribute__((__interrupt__, __no_auto_psv__)) _DMA1Interrupt(void) 
{
	SD_DMA_CHANNEL_2_INTERRUPT(&sd_card);
}

static unsigned long pc;

/*
// trap for AddressError
*/
void __attribute__((__interrupt__, __no_auto_psv__)) _AddressError(void)
{
	/*
	// get the value of the PC before trap
	*/
	pc = __PC();
	/*
	// break
	*/
	HALT();
	/*
	// clear interrupt flag
	*/
	INTCON1bits.ADDRERR = 0;
}

void __attribute__((__interrupt__, __no_auto_psv__)) _StackError(void)
{
	/*
	// get the value of the PC before trap
	*/
	pc = __PC();
	/*
	// halt cpu
	*/
	HALT();
	/*
	// clear interrupt flag
	*/
	INTCON1bits.STKERR = 0;
}

void __attribute__((__interrupt__, __no_auto_psv__)) _MathError(void)
{
	HALT();
	INTCON1bits.MATHERR = 0;
}

void __attribute__((__interrupt__, __no_auto_psv__)) _DMACError(void)
{
	HALT();
	
}

void __attribute__((__interrupt__, __no_auto_psv__)) _OscillatorFail(void)
{
	HALT();
}

	