
#include "spi.h"

//
// initializes the spi module
//
void spi_init(void) 
{
	
	SPI_DISABLE( );
	SPISTATbits.SPISIDL = 0x1;		// stop when in idle
//	SPISTATbits.BUFELM = 0x0;		// interrupt ( or dma ) after 1 byte
	SPICON1bits.DISSCK = 0x0;		// spi clock enabled
	SPICON1bits.DISSDO = 0x0;		// sdo pin enabled
	SPICON1bits.MODE16 = 0x0;		// buffer is 8 bits
	SPICON1bits.CKE = 0x1;			// 1 for sd, 0 for eeprom
	SPICON1bits.SSEN = 0x0;			// we're master
	SPICON1bits.CKP = 0x0;			// clock is high during idle time
	SPICON1bits.MSTEN = 0x1;		// we're master
	SPICON1bits.SMP = 0x0;			// data sampled at the end of output time 0=beggining
	SPICON2bits.FRMEN = 0x0;		// not using framed spi
	SPICON2bits.SPIFSD = 0x0;		// FS pin is output
	SPICON2bits.FRMPOL = 0x1;		// FS polarity is 1
	SPICON2bits.FRMDLY = 0x1;		// FS pulse one cycle before data 1=same cycle
	
	//IPC8bits.SPI2IP = 0x6;
	//IFS2bits.SPI2IF = 0x0;			// clear int flag
	//IEC2bits.SPI2IE = 0x1;
	IPC2bits.SPI1IP = 0x6;
	IFS0bits.SPI1IF = 0x0;
	IEC0bits.SPI1IE = 0x0;

	SPI_ENABLE( );
	
}
