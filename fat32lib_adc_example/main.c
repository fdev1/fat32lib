/*
 * fat32lib_adc_sample - Example for sampling from ADC to SD card with Fat32lib.
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
 
#include <common.h>
#include <dma.h>
#include <rtc.h>
#include <lcd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "..\sdlib\sd.h"
#include "..\fat32lib\storage_device.h"
#include "..\fat32lib\fat.h"
#include "..\fat32lib\filesystem_interface.h"
#include "..\smlib\sm.h"

#define USE_DOUBLE_BUFFERS
#define AUDIO_BUFFER_SIZE		6144	/* size of each buffer must be a multiple of sector size */
#define DMA_BUFFER_SIZE			8		/* size of DMA buffer must be a power of two >= 4 
											with double buffering smaller values give better performance
											when double buffering is disabled it must be a multiple of sector size */
//
// configuration bits
//
#if defined(EXPLORER16)
_FOSCSEL(FNOSC_PRIPLL & IESO_OFF);
_FOSC(POSCMD_XT & OSCIOFNC_ON & FCKSM_CSDCMD);
_FWDT(FWDTEN_OFF);
_FPOR(FPWRT_PWR128);
#endif

//
// global variables
//
#if defined(USE_DOUBLE_BUFFERS)
static unsigned char __attribute__((section(".audio_buffer"))) audio_buffer[AUDIO_BUFFER_SIZE * 2];
#define audio_buffer_a 		(audio_buffer)
#define audio_buffer_b		(audio_buffer + AUDIO_BUFFER_SIZE)

#endif
unsigned char __attribute__((space(dma), section(".adc_dma_buffer"), address(0x4600))) adc_buffer[DMA_BUFFER_SIZE];

#if defined(EXPLORER16)
LCD_CONFIG lcd_driver;		// lcd driver
#endif																
SD_DRIVER sd_card;																	// sd card driver
unsigned char __attribute__((space(dma), section(".sd_driver_dma_buffer"))) dma_buffer[512];	// DMA buffer for async/stream io
char __attribute__((space(dma), section(".sd_driver_dma_byte"))) dma_byte;					// sd driver needs 1 byte of dma memory

SM_FILE audio_file;
uint16_t audio_error;
char audio_waiting_for_data;
char __attribute__((near)) audio_recording = 0;
char __attribute__((near)) audio_active_buffer;
char __attribute__((near)) audio_dma_active_buffer;
#define WAIT_FOR_DATA
#define COUNT_LOST_CHUNKS
#define HALT_ON_LOST_CHUNK

#if defined(COUNT_LOST_CHUNKS)
uint16_t __attribute__((near)) audio_lost_chunks;
uint32_t __attribute__((near)) audio_good_chunks;
#endif
#if defined(USE_DOUBLE_BUFFERS)
unsigned char* __attribute__((near)) p_adc_buffer = adc_buffer;
unsigned char* __attribute__((near)) p_audio_buffer = audio_buffer;
#endif

//
// function prototypes
//
void init_cpu();
void init_adc();
void init_uart();
void init_lcd();
void init_fs();
void idle_processing();
void volume_mounted(char* volume_label);
void volume_dismounted(char* volume_label);
void start_sampling();
void init_pins();
void audio_stream_callback(SM_FILE* f, uint16_t* result, unsigned char** buffer, uint16_t* response);
void record_pressed();

//
// entry point
//
int main()
{
	//
	// configure LEDs
	//
	#if defined(EXPLORER16)
	IO_PORT_SET_AS_OUTPUT(A);	// LEDs
	IO_PIN_WRITE(A, 0, 1);		// power on led
	#endif
	
	IO_PIN_SET_AS_OUTPUT(A, 0);
	IO_PIN_WRITE(A, 0, 1);		// we'll flash this led during system idle
	IO_PIN_SET_AS_OUTPUT(B, 14);
	IO_PIN_WRITE(B, 14, 0);
	/*
	// configure switches
	*/
	#if defined(EXPLORER16)
	IO_PIN_SET_AS_INPUT(D, 6);	/* this will be our dismount button */
	IO_PIN_WRITE(D, 6, 1);	
	IO_PIN_SET_AS_INPUT(D, 7);	/* this will be our record button */
	IO_PIN_WRITE(D, 7, 1);
	#else
	IO_PIN_WRITE(A, 1, 1);		/* record/stop button */
	IO_PIN_SET_AS_INPUT(A, 1);
	#endif
	
	//
	// clock th/e cpu and mount the filesystem
	//
	init_cpu();
	/*
	// initialize pins
	*/
	#if !defined(EXPLORER16)
	init_pins();
	#endif
	/*
	// initialize uart (for printf)
	*/
	#if defined(EXPLORER16)
	init_uart();
	#endif
	/*
	// initialize ADC module
	*/
	init_adc();
	/*
	// initialize real-time clock
	*/
	rtc_init(40000000);
	/*
	// initialize lcd
	*/
	#if defined(EXPLORER16)
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
		idle_processing();
}

//
// initialize cpu
//
void init_cpu()
{
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
    //PLLFBD = 0x00A0;
    //CLKDIV = 0x0048;

	//
	// switch to primary oscillator
	//	
	//clock_sw();
	//
	// wait for PLL to lock
	//
	while (OSCCONbits.LOCK != 0x1);
}

/*
// configure pps
*/
#if !defined(EXPLORER16)
void init_pins()
{
	//__builtin_write_OSCCONL(0X46);
	//__builtin_write_OSCCONH(0X57);
	//OSCCONbits.IOLOCK = 0;

	RPINR20bits.SDI1R = 0XA;		/* RP9 is SDI */
	RPINR20bits.SCK1R = 0x9;
	RPOR4bits.RP9R = 0b01000;		/* RP10 is SCK */
	RPOR4bits.RP8R = 0b00111;		/* RP8 is SDO */
	// SDO1 = 0b00111
	// SCK1 = 0b01000
	
	__builtin_write_OSCCONL(0X46);
	__builtin_write_OSCCONH(0X57);
	OSCCONbits.IOLOCK = 1;
}
#endif

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
#if defined(EXPLORER16)
void init_lcd() 
{
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
}
#endif

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
	#if defined(EXPLORER16)
	IO_PIN_WRITE(F, 0, 0);		// power up sd card
	IO_PIN_WRITE(A, 6, 0);		// this led indicates that a volume is mounted
	IO_PIN_WRITE(F, 2, 1);		// set CD pin latch high
	IO_PIN_SET_AS_OUTPUT(F, 0);	// power pin of SD card (set to zero to power card)
	IO_PIN_SET_AS_OUTPUT(G, 3);	// chip select pin of SD card
	IO_PIN_SET_AS_OUTPUT(F, 2);	// card detected pin of SD card
	/*
	// initialize bit pointers for SD driver
	*/
	BP_INIT(cs, &LATG, 3);
	BP_INIT(media_ready, &PORTF, 2);
	BP_INIT(busy_signal, &LATA, 5);
	#else
	IO_PIN_WRITE(A, 2, 0);		// this led indicates that a volume is mounted
	IO_PIN_WRITE(B, 12, 1);		// set CD pin latch high
	IO_PIN_WRITE(A, 4, 1);		// ??
	IO_PIN_SET_AS_OUTPUT(A, 2);	// volume mounted led
	IO_PIN_SET_AS_INPUT(B, 12);	// CD pin
	IO_PIN_SET_AS_OUTPUT(B, 6); // Busy LED
	IO_PIN_SET_AS_OUTPUT(B, 7);	// chip select pin of SD card
	/*
	// initialize bit pointers for SD driver
	*/
	BP_INIT(cs, &LATB, 7);
	BP_INIT(media_ready, &PORTB, 12);
	BP_INIT(busy_signal, &LATB, 6);
	#endif
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
		DMA_GET_CHANNEL(0), 	// 1st DMA channel (interrupt must be configured for this channel)
		DMA_GET_CHANNEL(1), 	// 2nd DMA channel (interrupt must be configured for this channel)
		0xA, 					// DMA bus irq for SPI
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
	#if defined(EXPLORER16)
	IO_PIN_WRITE(A, 6, 1);
	#else
	//IO_PIN_WRITE(A, 2, 1);
	#endif
}

void volume_dismounted(char* volume_label)
{
	/*
	// turn off the drive mounted indicator LED
	*/
	#if defined(EXPLORER16)
	IO_PIN_WRITE(A, 6, 0);
	#else
	//IO_PIN_WRITE(A, 2, 0);
	#endif
}

/*
// initialized ADC module
*/
void init_adc()
{
	AD1CON1bits.ADON = 0;
	AD1CON1bits.ADSIDL = 1;
	AD1CON1bits.ADDMABM = 1;
	AD1CON1bits.AD12B = 1;		/* 12-bit operation */
	AD1CON1bits.FORM = 1;		/* signed integer */
	AD1CON1bits.SSRC = 7;		/* automatic sampling */
	AD1CON1bits.ASAM = 1;		/* begin sampling after conversion */
	AD1CON2bits.VCFG = 0;		/* use VDD and VSS as reference */
	AD1CON2bits.CSCNA = 0;	
	AD1CON2bits.CHPS = 0;		/* channel 0 */
	AD1CON2bits.SMPI = 0;		/* increment DMA address after each sample */
	AD1CON2bits.BUFM = 0;
	AD1CON2bits.ALTS = 0;
	/*
	// Sample Time = Sampling Time (TSamp) + Conversion Time (TConv)
	//
	// 32 Ksps = (FCY / 32000) * TCY = (40000000 / 32000) * TCY = 1250 * TCY
	//
	// TAD = (ADCS + 1) * TCY = 50 * TCY
	// TCONV = 14 * TAD	= 14 * 50 * TCY =  700 * TCY
	// TSAMP = SAMC * TAD = 11 * 50 * TCY = 550 * TCY
	// T = TSAMP + TCONV = 1250 * TCY
	//
	*/
	AD1CON3bits.ADRC = 0; 		/* use system clock */
	AD1CON3bits.SAMC = 11;		/* 11 TAD */
	AD1CON3bits.ADCS = 5;		/* TAD = (ADCS + 1) * Tcy */
	AD1CON4bits.DMABL = 7;		/* 256 bytes DMA buffer */
	/*
	// AN1/RA1 is analog input
	*/	
	AD1CHS0bits.CH0NB = 0;		/* negative input is VSS */
	AD1CHS0bits.CH0SB = 9;		/* positive input is AN9 */
	AD1CHS0bits.CH0NA = 0;		/* negative input is VSS */
	AD1CHS0bits.CH0SA = 9;		/* positive input is AN9 */
	
	/*
	// set AN9/RB15 as analog input. everything 
	// else is digital
	*/
	#if defined(EXPLORER16)
	AD1PCFGH = 0xffff;
	AD2PCFGL = 0Xffff;	
	AD1PCFGL = 0Xff7f
	#else
	AD1PCFGL = 0b1111110111111111;
	IO_PIN_SET_AS_INPUT(B, 15);
	#endif

	/*
	// configure dma channel
	*/
	DMA_CHANNEL_DISABLE(DMA_GET_CHANNEL(2));
	DMA_SET_CHANNEL_WIDTH(DMA_GET_CHANNEL(2), DMA_CHANNEL_WIDTH_WORD);
	DMA_SET_CHANNEL_DIRECTION(DMA_GET_CHANNEL(2), DMA_DIR_PERIPHERAL_TO_RAM);
	DMA_SET_INTERRUPT_MODE(DMA_GET_CHANNEL(2), DMA_INTERRUPT_MODE_FULL);
	DMA_SET_NULL_DATA_WRITE_MODE(DMA_GET_CHANNEL(2), 0);
	DMA_SET_ADDRESSING_MODE(DMA_GET_CHANNEL(2), DMA_ADDR_REG_IND_W_POST_INCREMENT);
	DMA_SET_PERIPHERAL(DMA_GET_CHANNEL(2), 0xD);
	DMA_SET_PERIPHERAL_ADDRESS(DMA_GET_CHANNEL(2), &ADC1BUF0);
	DMA_SET_MODE(DMA_GET_CHANNEL(2), DMA_MODE_CONTINUOUS_W_PING_PONG);
	DMA_SET_TRANSFER_LENGTH(DMA_GET_CHANNEL(2), (DMA_BUFFER_SIZE / 4) - 1);
	DMA_SET_BUFFER_A(DMA_GET_CHANNEL(2), adc_buffer);
	DMA_SET_BUFFER_B(DMA_GET_CHANNEL(2), adc_buffer + (DMA_BUFFER_SIZE / 2));
	
	DMA_CHANNEL_CLEAR_INTERRUPT_FLAG(DMA_GET_CHANNEL(2));
	DMA_CHANNEL_ENABLE_INTERRUPT(DMA_GET_CHANNEL(2));
	DMA_CHANNEL_SET_INT_PRIORITY(DMA_GET_CHANNEL(2), 0x5);
	/*
	// configure modulo addressing
	*/
	#if defined(USE_DOUBLE_BUFFERS)
	XMODSRT = (int) audio_buffer;
	XMODEND = (int) (audio_buffer + ((AUDIO_BUFFER_SIZE * 2) - 1));
	YMODSRT = (int) adc_buffer;
	YMODEND = (int) (adc_buffer + ((DMA_BUFFER_SIZE) - 1));
	MODCONbits.XWM = 4;		// w4
	MODCONbits.YWM = 11;		// w11
	//MODCONbits.XMODEN = 1;
	//MODCONbits.YMODEN = 1;
	#endif
}

/*
// starts sampling when record button is pressed
*/
void record_pressed()
{
	if (audio_recording)
	{
		audio_recording = 0;
	}
	else
	{
		start_sampling();
	}
}

/*
// creates a file for sampled data and starts sampling
*/
void start_sampling()
{
	uint16_t ipl;
	/*
	// open file for stream io
	*/
	audio_error = sm_file_open(&audio_file, "x:\\audio.pcm", 
		SM_FILE_ACCESS_CREATE | SM_FILE_ACCESS_OVERWRITE | SM_FILE_FLAG_NO_BUFFERING | SM_FILE_FLAG_OPTIMIZE_FOR_FLASH);
	if (audio_error)
	{
		#if defined(EXPLORER16)
		printf("Error opening file: 0x%x", audio_error);
		#endif
		return;
	}	
	/*
	// allocate 256 MB for file
	*/
	audio_error = sm_file_alloc(&audio_file, 1024L * 1024 * 256);
	if (audio_error)
	{
		#if defined(EXPLORER16)
		printf("Error allocating space: 0x%x", err);
		#endif
		sm_file_close(&audio_file);
		return;
	}
	
	audio_recording = 0;
	audio_active_buffer = 0;
	audio_dma_active_buffer = 0;
	#if defined(COUNT_LOST_CHUNKS)
	audio_lost_chunks = 0;
	audio_good_chunks = 0;
	#endif
	#if defined(USE_DOUBLE_BUFFERS)
	p_adc_buffer = adc_buffer;
	p_audio_buffer = audio_buffer;
	#endif
	audio_waiting_for_data = 1;
	/*
	// enable DMA channel and ADC
	*/
	DMA_CHANNEL_ENABLE(DMA_GET_CHANNEL(2));
	AD1CON1bits.ADON = 1;
	IO_PIN_WRITE(B, 14, 1);
	audio_recording = 1;
	/*
	// wait for the first round of data
	*/
	while (1)
	{
		SET_AND_SAVE_CPU_IPL(ipl, 7);
		if (audio_active_buffer != audio_dma_active_buffer)
		{
			audio_waiting_for_data = 0;
			audio_active_buffer ^= 1;
			RESTORE_CPU_IPL(ipl);
			break;
		}
		RESTORE_CPU_IPL(ipl);
	}

	/*
	// begin writing. This will continue to write in chunks of 4 KB until the
	// response argument of the callback function (file_write_stream_callback) is set
	// to stop
	*/
	#if defined(USE_DOUBLE_BUFFERS)
	audio_error = sm_file_write_stream(&audio_file, audio_buffer_a, AUDIO_BUFFER_SIZE, &audio_error, (SM_STREAM_CALLBACK) &audio_stream_callback, &audio_file);
	#else
	audio_error = sm_file_write_stream(&audio_file, adc_buffer, DMA_BUFFER_SIZE / 2, &audio_error, (SM_STREAM_CALLBACK) &audio_stream_callback, &audio_file);
	#endif
	if (audio_error != FILESYSTEM_OP_IN_PROGRESS)
	{
		#if defined(EXPLORER16)
		printf("Error starting stream: 0x%x", audio_error);
		//#else
		//while(1);
		#endif
		DMA_CHANNEL_DISABLE(DMA_GET_CHANNEL(2));
		AD1CON1bits.ADON = 0;
		audio_recording = 0;
		IO_PIN_WRITE(B, 14, 0);
		sm_file_close(&audio_file);
		return;
	}
	/*
	// set the active buffer to 1
	*/
	//audio_waiting_for_data = 1;
}

/*
// this function is called by smlib when it is ready to 
// write more data to the file, we just set the *response
// parameter to READY when the buffer is loaded or if the
// data is not ready we set it to SKIP and the file system
// will call back as soon as it is ready to write again
// which is almost immediately unless there are more IO requests
// pending.
*/
void audio_stream_callback(SM_FILE* f, uint16_t* result, unsigned char** buffer, uint16_t* response)
{
	if (*result == FILESYSTEM_AWAITING_DATA)
	{
		if (audio_recording)
		{
			audio_waiting_for_data = 1;
			/*
			// wait for data
			*/
			#if defined (WAIT_FOR_DATA)
			uint16_t ipl;
			char cmp;
			do
			{
				SET_AND_SAVE_CPU_IPL(ipl, 7);
				cmp = (audio_active_buffer == audio_dma_active_buffer);
				RESTORE_CPU_IPL(ipl);
			}	
			while (cmp);
			#else
			if (audio_active_buffer == audio_dma_active_buffer)
			{
				*response = FAT_STREAMING_RESPONSE_SKIP;
			}	
			else					
			#endif
			{
				/*
				// set the stream buffer
				*/
				#if defined(USE_DOUBLE_BUFFERS)
				*buffer = (audio_active_buffer == 0) ? audio_buffer_a : audio_buffer_b;
				#else
				*buffer = (audio_active_buffer == 0) ? adc_buffer : adc_buffer + (DMA_BUFFER_SIZE / 2);
				#endif
				/*
				// toggle the active buffer and set response to READY
				*/				
				audio_active_buffer ^= 1;
				#if defined(COUNT_LOST_CHUNKS)
				audio_good_chunks++;
				#endif
				audio_waiting_for_data = 0;
				*response = FAT_STREAMING_RESPONSE_READY;
			}	
		}
		else
		{
			AD1CON1bits.ADON = 0;
			IO_PIN_WRITE(B, 14, 0);
			DMA_CHANNEL_DISABLE(DMA_GET_CHANNEL(2));
			*response = FAT_STREAMING_RESPONSE_STOP;
		}	
	}
	else
	{
		if (*result != FILESYSTEM_SUCCESS)
		{
			AD1CON1bits.ADON = 0;
			DMA_CHANNEL_DISABLE(DMA_GET_CHANNEL(2));
			audio_recording = 0;
			IO_PIN_WRITE(B, 14, 0);
			#if defined(EXPLORER16)
			printf("Error!\n");
			#else
			while(1);
			#endif
		}
		sm_file_close(&audio_file);
	}	
}

/*
// background processing routine
*/
void idle_processing()
{
	static time_t last_time = 0;
	static char record_press_processed = 0;
	#if defined(EXPLORER16)
	static time_t unmount_pressed_time = 0;
	static struct tm* timeinfo;
	static char time_string[9];
	static char date_string[11];
	#endif
	/*
	// SD driver processing
	*/
	sd_idle_processing(&sd_card);
	/* static unsigned char glyph = 'z' + 39;
	static char glyph_hex[3]; */
	
	if (last_time < time(0))
	{
		/*
		// update lcd
		*/
		time(&last_time);
		/*
		// flash the A7 LED
		*/
		#if defined(EXPLORER16)
		IO_PIN_WRITE(A, 7, IO_PIN_READ(A, 7) == 0);
		#else
		IO_PIN_WRITE(A, 0, IO_PIN_READ(A, 0) == 0);
		#endif
		/*
		// update the date and time
		*/	
		#if defined(EXPLORER16)
		timeinfo = localtime(&last_time);
		sprintf(date_string, "%02d/%02d/%d", timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_year + 1900);
		sprintf(time_string, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
		/*
		// write time and date to lcd
		*/
		lcd_set_pos(&lcd_driver, 0, 3);
		lcd_write_string(&lcd_driver, (unsigned char*) date_string);
		lcd_set_pos(&lcd_driver, 1, 4);
		lcd_write_string(&lcd_driver, (unsigned char*) time_string);
		
		/*
		lcd_set_pos(&lcd_driver, 0, 0);
		lcd_write_char(&lcd_driver, glyph++);
		sprintf(glyph_hex, "%02x", glyph);
		lcd_set_pos(&lcd_driver, 1, 0);
		lcd_write_string(&lcd_driver, (unsigned char*) glyph_hex);
		*/
		
		/*
		// flash disk activity indicator
		*/
		if (IO_PIN_READ(A, 5))
		{
			lcd_set_pos(&lcd_driver, 0, 15);
			lcd_write_char(&lcd_driver, (IO_PIN_READ(A, 7)) ? 0xF3 : ' ');
		}
		#endif		
	}
	/*
	// SD driver processing
	*/
	sd_idle_processing(&sd_card);
	/*
	// check for dismount button press
	*/
	#if defined(EXPLORER16)
	if (!IO_PIN_READ(D, 6))
	{
		/* glyph = 'z' + 39; */
		
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
	// check for start/stop sampling button press
	*/
	#if defined(EXPLORER16)
	if (!IO_PIN_READ(D, 7))
	#else
	if (!IO_PIN_READ(A, 1))
	#endif
	{
		if (!record_press_processed)
		{
			record_pressed();
			record_press_processed = 1;
		}	
	}
	else
	{
		record_press_processed = 0;
	}
	/*
	// SD driver processing
	*/
	sd_idle_processing(&sd_card);
	/*
	// update media ready LED
	*/
	#if defined(EXPLORER16)
	IO_PIN_WRITE(A, 1, IO_PIN_READ(F, 2) == 0);
	/*
	// update media ready LCD indicator
	*/
	if (IO_PIN_READ(F, 2) == 0)
	{
		if (!IO_PIN_READ(A, 5))
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
	// lcd driver processing
	*/
	#if defined(EXPLORER16)
	lcd_idle_processing(&lcd_driver);
	#endif
	/*
	// SD driver processing
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

/*
// interrupt for ADC conversions
*/
void __attribute__((__interrupt__, __no_auto_psv__, __shadow__)) _DMA2Interrupt(void) 
{
	/*
	// count lost chunks
	*/
	#if defined(USE_DOUBLE_BUFFERS)
	//if ((audio_active_buffer == 0 && p_audio_buffer == audio_buffer_a) || 
	//	(audio_active_buffer == 1 && p_audio_buffer == audio_buffer_b))
	if (p_audio_buffer == audio_buffer_a || p_audio_buffer == audio_buffer_b)
	{
		if (audio_active_buffer != audio_dma_active_buffer && !audio_waiting_for_data && audio_good_chunks)
		{
			audio_lost_chunks++;
			#if defined(HALT_ON_LOST_CHUNK)
			HALT();
			#endif
		}
	}	
	#else
	#if defined(COUNT_LOST_CHUNKS)
	#if defined(USE_DOUBLE_BUFFERS)
	if (p_audio_buffer == audio_buffer_a || p_audio_buffer == audio_buffer_b)
	#endif
	{
		if (audio_active_buffer != audio_dma_active_buffer)
		{
			audio_lost_chunks++;
		}
	}
	#if defined(USE_DOUBLE_BUFFERS)
	else 
	#endif
	#endif
	#endif
	/*
	// if this is the end of the buffer toggle the active buffer bit
	*/
	#if defined(USE_DOUBLE_BUFFERS)
	if (p_audio_buffer == (audio_buffer + (AUDIO_BUFFER_SIZE - (DMA_BUFFER_SIZE / 2))) ||
		p_audio_buffer == (audio_buffer + ((AUDIO_BUFFER_SIZE * 2) - (DMA_BUFFER_SIZE / 2))))
	#endif
	{
		audio_dma_active_buffer ^= 1;
	}	
	/*
	// copy data to buffer
	*/
	#if defined(USE_DOUBLE_BUFFERS)
	#if (DMA_BUFFER_SIZE >= 8)
	__asm__
	(
		"mov %0, w4\n"
		"mov %1, w11\n"
		"bset MODCON, #14\n"
		"bset MODCON, #15\n"
		"push RCOUNT\n"
		"repeat #%2\n"
		"mov [w11++], [w4++]\n"
		"mov [w11], [w4++]\n"
		"mov w4, %0\n"
		"movsac A, [w11] += 2, w4\n"
		"mov w11, %1\n"
		"pop RCOUNT\n"
		"bclr MODCON, #14\n"
		"bclr MODCON, #15"
		: "+g" (p_audio_buffer), "+g" (p_adc_buffer)
		: "i" ((DMA_BUFFER_SIZE / 4) - 2)
		: "w11", "w4"
	);
	#else
	__asm__
	(
		"mov %0, w4\n"
		"mov %1, w11\n"
		"bset MODCON, #14\n"
		"bset MODCON, #15\n"
		"mov [w11], [w4++]\n"
		"mov w4, %0\n"
		"movsac A, [w11] += 2, w4\n"
		"mov w11, %1\n"
		"bclr MODCON, #14\n"
		"bclr MODCON, #15"
		: "+g" (p_audio_buffer), "+g" (p_adc_buffer)
		: 
		: "w11", "w4"
	);
	#endif
	#endif
	/*
	// clear interrupt flag
	*/
	IFS1bits.DMA2IF = 0;
}
