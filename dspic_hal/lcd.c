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
 
#include "lcd.h"
#include "rtc.h"

#define Fcy  40000000 

void Delay( unsigned int delay_count );
void Delay_Us( unsigned int delayUs_count );

#define Delay200uS_count  (Fcy * 0.0002) / 1080
#define Delay_1mS_Cnt	  (Fcy * 0.001) / 2950
#define Delay_2mS_Cnt	  (Fcy * 0.002) / 2950
#define Delay_5mS_Cnt	  (Fcy * 0.005) / 2950
#define Delay_15mS_Cnt 	  (Fcy * 0.015) / 2950
#define Delay_1S_Cnt	  (Fcy * 1) / 2950 

#define LCD_LINE_LENGTH		16
#define LCD_LINE_COUNT		2


/*
// generates a 25 ns delay
// ( at 40 mhz each instruction takes 25 nsecs )
*/
#define WAIT_25_NS( )			\
	asm("nop")

/*
// send data to the lcd
*/
#define SEND_DATA( data )		\
{								\
	*( lcd->DataLine ) = data;	\
	SEND_E_PULSE( );			\
}	

/*
// sends a 12 ns pulse on the E line
*/
#define SEND_E_PULSE( )			\
{								\
	BP_SET( lcd->ELine );		\
	WAIT_25_NS( );				\
	BP_CLR( lcd->ELine );		\
}	

/*
// initializes the lcd
*/
void lcd_init(LCD_CONFIG* lcd) 
{
	int i;
	/*
	// 15mS delay after Vdd reaches nnVdc
	//
	//if (lcd->VddRiseDelay)
	//	sleep(lcd->VddRiseDelay);
	*/
	Delay(Delay_5mS_Cnt);
	/*
	// set initial states for the data and
	// control pins
	*/
	*lcd->DataLine = 0x00;			/* assert data lines */
	BP_CLR( lcd->RWLine );			/* assert the R/W line */
	BP_CLR( lcd->RSLine );			/* assert the RS line */
	BP_CLR( lcd->ELine );			/* assert the E line */
	/*
	// send initialization sequence
	*/
	SEND_DATA(0x38);
	Delay(Delay_5mS_Cnt);
	SEND_DATA(0x38);
	Delay_Us(Delay200uS_count);
	SEND_DATA(0x38);
	Delay_Us(Delay200uS_count);
	/*
	// send initialization commands
	*/
    lcd_cmd( lcd, 0x38 );          	/* function set */
    lcd_cmd( lcd, 0x0C );          	/* Display on/off control, cursor blink off (0x0C) */
    lcd_cmd( lcd, 0x06 );		  	/* entry mode set (0x06) */
    lcd_cmd( lcd, 0x01 );			/* clear the LCD */
    lcd->line_no = 0;
    lcd->line_pos = 0;
    lcd->write_pos = 0;
    lcd->last_update_s = 0;
    lcd->last_update_ms = 0;
    /*
    // clear lcd buffers
    */    
    for (i = 0; i < LCD_LINE_LENGTH; i++)
    {
	    lcd->lines[0][i] = 0x20;
	    lcd->lines[1][i] = 0x20;
    }
}

/*
// sends a command to the lcd
*/
void lcd_cmd(LCD_CONFIG* lcd, unsigned char cmd) 
{
    /*
    // make sure that the R/W and RS lines
    // are asserted
    */
    BP_CLR( lcd->RWLine );
    BP_CLR( lcd->RSLine );
	/*
	// send the command to the LCD
	*/    
    SEND_DATA( cmd );
    /*
    // microchip sample uses 5ms but
    // 400 us works for me
    */
	Delay_Us( Delay200uS_count );
	Delay_Us( Delay200uS_count );
}

void lcd_set_pos(LCD_CONFIG* lcd, unsigned char line, unsigned char pos)
{
	lcd->write_pos = (LCD_LINE_LENGTH * line) + pos;
}

/*
// writes a character to the lcd
*/
void lcd_write_char(LCD_CONFIG* lcd, unsigned char data) 
{
	if (lcd->write_pos >= LCD_LINE_LENGTH * LCD_LINE_COUNT)
	{
		lcd->write_pos = 0;
	}
	/*
	// write the char and update pos
	*/
	((unsigned char*) lcd->lines)[lcd->write_pos++] = data;
}

/*
// writes a string to the lcd
*/
void lcd_write_string(LCD_CONFIG* lcd, unsigned char* str)
{
	while (*str)
		lcd_write_char(lcd, *str++);
}

/*
// writes a string to the lcd at the specified line index
// and moves the cursor to the end of the line
*/
void lcd_write_line(LCD_CONFIG* lcd, unsigned char line, unsigned char* str) 
{
	static unsigned char c;
	c = 0;
	/*
	// write the 1st 16 string characters to the LCD buffer
	*/
  	while (*str != 0x0 && c < LCD_LINE_LENGTH)	
	{
	  	lcd->lines[line][c++] = *str++;
	}
	/*
	// if less than 16 fill with spaces
	*/
	while (c < LCD_LINE_LENGTH)
	{
	  	lcd->lines[line][c++] = 0x20;
	}
	/*
	// place the cursor at the end of the line
	*/
	lcd->write_pos = LCD_LINE_LENGTH * (line + 1);
}

void lcd_idle_processing(LCD_CONFIG* lcd)
{
	static uint32_t s, ms;
	s = lcd->last_update_s;
	ms = lcd->last_update_ms;
	rtc_timer_elapsed(&s, &ms);
	
	if (s + ms >= 2)
	{
		if (lcd->line_pos == LCD_LINE_LENGTH)
		{
			/*
			// change line # and pos on lcd driver state
			*/
			lcd->line_pos = 0;
			lcd->line_no = ++(lcd->line_no) % LCD_LINE_COUNT;
			/*
			// send change line command to lcd
			*/
			BP_CLR(lcd->RWLine);
			BP_CLR(lcd->RSLine);
			SEND_DATA((lcd->line_no == 0) ? 0x2 : 0xc0);
		}
		else
		{
			/*
			// write next char
			*/
			BP_CLR(lcd->RWLine);
			BP_SET(lcd->RSLine);
			SEND_DATA(lcd->lines[lcd->line_no][lcd->line_pos++]);
			BP_CLR(lcd->RSLine);
		}
		/*
		// remember the time of the last update
		*/
		rtc_timer(&lcd->last_update_s, &lcd->last_update_ms);
	}
}

unsigned int temp_count;

void Delay( unsigned int delay_count ) 
{
	temp_count = delay_count +1;
	while (temp_count--)
	{
		asm volatile("do #3200, inner" );	
		asm volatile("nop");
		asm volatile("inner: nop");
	}	
}
	

void Delay_Us( unsigned int delayUs_count )
{
	temp_count = delayUs_count +1;
	while (temp_count--)	
	{
		asm volatile("do #1500, inner1" );	
		asm volatile("nop");
		asm volatile("inner1: nop");
	}	
}		

