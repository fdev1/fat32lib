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

typedef char SPI_MODULE;

/*
// dma macros
*/
#define SPI1_DMA_REQ				(0x0A)
#define SPI2_DMA_REQ				(0x21)

/*
// no of spi modules on device
*/
#define SPI_NO_OF_MODULES			(2)

/*
// max speed of spi module in bps
*/
#define SPI_MAX_SPEED				(10000000)

#define SPI_GET_MODULE(module)		(module)

/*
// gets the address of the SPI buffer for the specified
// module
*/
#define SPI_BUFFER_ADDRESS(module)	((module == 1) ? &SPI1BUF : &SPI2BUF)
/*
// gets the DMA req for the module
*/
#define SPI_GET_DMA_REQ(module)		((module == 1) ? SPI1_DMA_REQ : SPI2_DMA_REQ)

/*
// initializes the SPI module
*/
void spi_init(char module);

/*
// sets the SPI bus speed
*/
uint32_t spi_set_clock(char module, uint32_t bps);

/*
// reads the spi buffer
*/
uint16_t spi_read(char module);

/*
// writes a byte or word to the spi buffer
// and waits for the data to be transmitted and
// returns the value of the spi buffer after
// the transmission is complete
*/
uint16_t spi_write(char module, int data);

/*
// writes a buffer to the spi bus/discards all incoming
// data while writing
*/
void spi_write_buffer(char module, unsigned char* buffer, int length);

/*
// reads data from the spi bus into a buffer
*/
void spi_read_buffer(char module, unsigned char* buffer, int length);

#endif
