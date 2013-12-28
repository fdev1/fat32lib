#ifndef __SPI_H__
#define __SPI_H__

#include "common.h"

static volatile uint16_t __dummy__;

#define SPI_MODULE			2

//
// macros for the registers of the
// desired SPI module
//
#if SPI_MODULE == 2
#define SPIBUF				SPI1BUF
#define SPISTAT				SPI1STAT
#define SPICON1				SPI1CON1
#define SPICON2				SPI1CON1
#define SPICON1bits			SPI1CON1bits
#define SPICON2bits			SPI1CON2bits
#define SPISTATbits			SPI1STATbits
#define SPI_DMA_ASSOC		SPI1_DMA_ASSOC
#else
#define SPIBUF				SPI2BUF
#define SPISTAT				SPI2STAT
#define SPICON1				SPI2CON1
#define SPICON2				SPI2CON2
#define SPICON1bits			SPI2CON1bits
#define SPICON2bits			SPI2CON2bits
#define SPISTATbits			SPI2STATbits
#define SPI_DMA_ASSOC		SPI2_DMA_ASSOC
#endif

//
// misc macros
//
#define SPI1_DMA_ASSOC				( 0b0001010 )
#define SPI2_DMA_ASSOC				( 0b0100001 )
#define SPI_ENABLE( )				SPISTATbits.SPIEN = 0x1
#define SPI_DISABLE( )				SPISTATbits.SPIEN = 0x0
#define SPI_WRITE( x )				SPIBUF = ( x )
#define SPI_READ( )					( SPIBUF )
#define SPI_IS_RX_BUFFER_FULL( )	(  SPISTATbits.SPIRBF )
#define SPI_WAIT_FOR_TRANSMIT( )	while ( SPI_IS_TX_BUFFER_FULL( ) )
#define SPI_WAIT_FOR_RECEIVE( )		while ( !SPI_IS_RX_BUFFER_FULL( ) )

//
// according to the dsPIC33 errata, if we
// write to the TX buffer right after the
// SPISTATbits.SPIRBF clears the SPI module
// may ignore
//
#define SPI_IS_TX_BUFFER_FULL( )	( !SPISTATbits.SPIRBF ) 	// ( SPISTATbits.SPITBF )
//
// sets the SPI clock prescaller
//
#define SPI_SET_PRESCALER( pri, sec )	\
{										\
	SPICON1bits.PPRE = pri;				\
	SPICON1bits.SPRE = sec;				\
}	


//
// flush the spi receive buffer
//
#define SPI_FLUSH_BUFFER( )			\
{									\
	if ( SPI_IS_RX_BUFFER_FULL( ) )	\
		__dummy__ = SPI_READ( );	\
}		

//
// reads data from the SPI module
//
#define SPI_READ_DATA( x )			\
{									\
	SPI_WRITE( 0xFF );				\
	SPI_WAIT_FOR_RECEIVE( );		\
	x = SPI_READ( );				\
}

//
// writes data to the SPI module
//
#define SPI_WRITE_DATA( x )			\
{									\
	SPI_WRITE( x );					\
	SPI_WAIT_FOR_RECEIVE( );		\
	SPI_FLUSH_BUFFER( );			\
}	

//
// initializes the SPI module
//
void spi_init( void );

#endif
