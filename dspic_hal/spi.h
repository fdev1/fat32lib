/*
 * dspic_hal - Fat32lib hardware abstraction library for dspic MCUs
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
 
#ifndef __SPI_H__
#define __SPI_H__

#include "common.h"

static volatile uint16_t __dummy__;

#define SPI_MODULE			2

/*
// macros for the registers of the
// desired SPI module
*/
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

/*
// misc macros
*/
#define SPI1_DMA_ASSOC				( 0b0001010 )
#define SPI2_DMA_ASSOC				( 0b0100001 )
#define SPI_ENABLE( )				SPISTATbits.SPIEN = 0x1
#define SPI_DISABLE( )				SPISTATbits.SPIEN = 0x0
#define SPI_WRITE( x )				SPIBUF = ( x )
#define SPI_READ( )					( SPIBUF )
#define SPI_IS_RX_BUFFER_FULL( )	(  SPISTATbits.SPIRBF )
#define SPI_WAIT_FOR_TRANSMIT( )	while ( SPI_IS_TX_BUFFER_FULL( ) )
#define SPI_WAIT_FOR_RECEIVE( )		while ( !SPI_IS_RX_BUFFER_FULL( ) )

/*
// according to the dsPIC33 errata, if we
// write to the TX buffer right after the
// SPISTATbits.SPIRBF clears the SPI module
// may ignore
*/
#define SPI_IS_TX_BUFFER_FULL( )	( !SPISTATbits.SPIRBF ) 	/* ( SPISTATbits.SPITBF ) */

/*
// sets the SPI clock prescaller
*/
#define SPI_SET_PRESCALER( pri, sec )	\
{										\
	SPICON1bits.PPRE = pri;				\
	SPICON1bits.SPRE = sec;				\
}	

/*
// clocks the SPI module at it's top speed
*/
#if defined(__dsPIC33FJ128GP802__)
#define SPI_SET_PRESCALER_MAX()			\
{										\
	SPICON1bits.PPRE = 0X2;				\
	SPICON1bits.SPRE = 0X7;				\
}

#else
#define SPI_SET_PRESCALER_MAX()			\
{										\
	SPICON1bits.PPRE = 0X2;				\
	SPICON1bits.SPRE = 0X7;				\
}
#endif

/*
// flush the spi receive buffer
*/
#define SPI_FLUSH_BUFFER( )			\
{									\
	/*if ( SPI_IS_RX_BUFFER_FULL( ) )*/	\
		__dummy__ = SPI_READ( );	\
}		

/*
// reads data from the SPI module
*/
#define SPI_READ_DATA( x )			\
{									\
	SPI_WRITE( 0xFF );				\
	SPI_WAIT_FOR_RECEIVE( );		\
	x = SPI_READ( );				\
}

/*
// writes data to the SPI module
*/
#define SPI_WRITE_DATA( x )			\
{						 			\
	SPI_WRITE( x );					\
	SPI_WAIT_FOR_RECEIVE( );		\
	SPI_FLUSH_BUFFER( );			\
}	

/*
// initializes the SPI module
*/
void spi_init( void );

#endif
