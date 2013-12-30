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
 

#include "spi.h"
#include "rtc.h"

/*
// initializes the spi module
*/
void spi_init(char module) 
{
	switch (module)
	{
		case 1:
			SPI1STATbits.SPIEN = 0x0;
			SPI1STATbits.SPISIDL = 0x1;		/* stop when in idle */
		/*	SPI1STATbits.BUFELM = 0x0;*/		/* interrupt ( or dma ) after 1 byte */
			SPI1CON1bits.DISSCK = 0x0;		/* spi clock enabled */
			SPI1CON1bits.DISSDO = 0x0;		/* sdo pin enabled */
			SPI1CON1bits.MODE16 = 0x0;		/* buffer is 8 bits */
			SPI1CON1bits.CKE = 0x1;			/* 1 for sd, 0 for eeprom */
			SPI1CON1bits.SSEN = 0x0;			/* we're master */
			SPI1CON1bits.CKP = 0x0;			/* clock is high during idle time */
			SPI1CON1bits.MSTEN = 0x1;		/* we're master */
			SPI1CON1bits.SMP = 0x0;			/* data sampled at the end of output time 0=beggining */
			SPI1CON2bits.FRMEN = 0x0;		/* not using framed spi */
			SPI1CON2bits.SPIFSD = 0x0;		/* FS pin is output */
			SPI1CON2bits.FRMPOL = 0x1;		/* FS polarity is 1 */
			SPI1CON2bits.FRMDLY = 0x1;		/* FS pulse one cycle before data 1=same cycle */
			
			/*IPC8bits.SPI2IP = 0x6;*/
			/*IFS2bits.SPI2IF = 0x0;*/			/* clear int flag */
			/*IEC2bits.SPI2IE = 0x1;*/
			IPC2bits.SPI1IP = 0x6;
			IFS0bits.SPI1IF = 0x0;
			IEC0bits.SPI1IE = 0x0;
		
			SPI1STATbits.SPIEN = 0x1;
			break;
		#if (SPI_NO_OF_MODULES >= 2)
		case 2:
			SPI2STATbits.SPIEN = 0x0;
			SPI2STATbits.SPISIDL = 0x1;		/* stop when in idle */
		/*	SPI2STATbits.BUFELM = 0x0;*/		/* interrupt ( or dma ) after 1 byte */
			SPI2CON1bits.DISSCK = 0x0;		/* spi clock enabled */
			SPI2CON1bits.DISSDO = 0x0;		/* sdo pin enabled */
			SPI2CON1bits.MODE16 = 0x0;		/* buffer is 8 bits */
			SPI2CON1bits.CKE = 0x1;			/* 1 for sd, 0 for eeprom */
			SPI2CON1bits.SSEN = 0x0;			/* we're master */
			SPI2CON1bits.CKP = 0x0;			/* clock is high during idle time */
			SPI2CON1bits.MSTEN = 0x1;		/* we're master */
			SPI2CON1bits.SMP = 0x0;			/* data sampled at the end of output time 0=beggining */
			SPI2CON2bits.FRMEN = 0x0;		/* not using framed spi */
			SPI2CON2bits.SPIFSD = 0x0;		/* FS pin is output */
			SPI2CON2bits.FRMPOL = 0x1;		/* FS polarity is 1 */
			SPI2CON2bits.FRMDLY = 0x1;		/* FS pulse one cycle before data 1=same cycle */
			
			/*IPC8bits.SPI2IP = 0x6;*/
			/*IFS2bits.SPI2IF = 0x0;*/			/* clear int flag */
			/*IEC2bits.SPI2IE = 0x1;*/
			IPC2bits.SPI1IP = 0x6;
			IFS0bits.SPI1IF = 0x0;
			IEC0bits.SPI1IE = 0x0;
			SPI2STATbits.SPIEN = 0x1;
			break;
		#endif
	}	
	
}

/*
// Sets the SPI clock
*/
uint32_t spi_set_clock(char module, uint32_t bps)
{
	const unsigned char pri_lookup[] = { 64, 16, 4, 1 };

	uint32_t fcy = rtc_get_fcy();
	uint32_t bps_temp;
	uint32_t bps_max = 0;
	unsigned char pri, sec, pri_max, sec_max;
	
	for (pri = 0; pri < 4; pri++)
	{
		for (sec = 0; sec < 8; sec++)
		{
			bps_temp = fcy / (pri_lookup[pri] * (8 - sec));
			if (bps_temp == bps)
			{
				bps_max = bps_temp;
				pri_max = pri;
				sec_max = sec;
				goto _calc_complete;
			}
			else
			{
				if (bps > bps_temp && bps_temp > bps_max)
				{
					if (bps_temp <= SPI_MAX_SPEED)
					{
						bps_max = bps_temp;
						pri_max = pri;
						sec_max = sec;
					}	
				}
			}
		}
	}
_calc_complete:
	/*
	// set prescaler
	*/
	switch (module)
	{
		case 1:
			SPI1STATbits.SPIEN = 0x0;
			SPI1CON1bits.PPRE = pri_max;
			SPI1CON1bits.SPRE = sec_max;
			SPI1STATbits.SPIEN = 0x1;
			break;
	
		#if (SPI_NO_OF_MODULES >= 2)
		case 2:
			SPI2STATbits.SPIEN = 0x0;
			SPI2CON1bits.PPRE = pri_max;
			SPI2CON1bits.SPRE = sec_max;
			SPI2STATbits.SPIEN = 0x1;
		#endif
	}	
	/*
	// return the bps setting
	*/
	return bps_max;
}

/*
// writes a byte or word to spi bus
*/
uint16_t spi_write(char module, int data)
{
	switch (module)
	{
		case 1:
			SPI1BUF = data;
			while (!SPI1STATbits.SPIRBF);
			return SPI1BUF;
		#if (SPI_NO_OF_MODULES >= 2)
		case 2:
			SPI2BUF = data;
			while (!SPI2STATbits.SPIRBF);
			return SPI2BUF;
		#endif
		default:
			_ASSERT(0);
			return 0;
	}
}

/*
// writes a buffer to the spi bus/discards all incoming
// data while writing
*/
void spi_write_buffer(char module, unsigned char* data, int length)
{
	char temp;
	switch (module)
	{
		case 1:
			while (length--)
			{
				SPI1BUF = *data++;
				while (!SPI1STATbits.SPIRBF);
				temp = SPI1BUF;
			}	
			break;
		#if (SPI_NO_OF_MODULES >= 2)
		case 2:
			while (length--)
			{
				SPI2BUF = *data++;
				while (!SPI2STATbits.SPIRBF);
				temp = SPI2BUF;
			}	
			break;
		#endif
		default:
			_ASSERT(0);
			break;
	}
}

/*
// reads a byte or word to spi bus
*/
uint16_t spi_read(char module)
{
	switch (module)
	{
		case 1:
			SPI1BUF = 0xFF;
			while (!SPI1STATbits.SPIRBF);
			return SPI1BUF;
		#if (SPI_NO_OF_MODULES >= 2)
		case 2: 
			SPI2BUF = 0xFF;
			while (!SPI2STATbits.SPIRBF);
			return SPI2BUF;
		#endif
		default:
			_ASSERT(0);
			return 0;
	}
}

void spi_read_buffer(char module, unsigned char* buffer, int length)
{
	switch (module)
	{
		case 1:
			while (length--)
			{
				SPI1BUF = 0xFF;
				while (!SPI1STATbits.SPIRBF);
				*buffer++ = SPI1BUF;
			}	
			break;
		#if (SPI_NO_OF_MODULES >= 2)
		case 2: 
			while (length--)
			{
				SPI2BUF = 0xFF;
				while (!SPI2STATbits.SPIRBF);
				*buffer++ = SPI2BUF;
			}	
			break;
		#endif
		default:
			_ASSERT(0);
	}

}
