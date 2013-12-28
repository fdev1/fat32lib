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
 
#include "common.h"

#ifndef __LCD_H__
#define __LCD_H__

/*
// macros for LCD commands
*/
#define lcd_cursor_right( lcd )  		lcd_cmd( lcd, 0x14 )
#define lcd_cursor_left( lcd )   		lcd_cmd( lcd, 0x10 )
#define lcd_display_shift( lcd ) 		lcd_cmd( lcd, 0x1C )
#define lcd_home_clr( lcd )      		lcd_cmd( lcd, 0x01 ) 
#define lcd_line_1( lcd )       		lcd_cmd( lcd, 0x02 ) 
#define lcd_line_2( lcd )        		lcd_cmd( lcd, 0xC0 )

/*
// pointer to the LCD configuration structure
*/
typedef struct LCD_CONFIG {
	/*
	// pointer to the 8 bit data port used
	// for the LCD data line
	*/
	unsigned char* DataLine;
	/*
	// pointer to the RW line
	*/	
	PIN_POINTER RWLine;
	/*
	// pointer to the RS line
	*/
	PIN_POINTER RSLine;
	/*
	// pointer to the E line
	*/
	PIN_POINTER ELine;
	uint32_t last_update_s;
	uint32_t last_update_ms;
	unsigned char line_no;
	unsigned char line_pos;
	unsigned char write_pos;
	unsigned char lines[2][16];
	/*
	// VDD rise delay
	*/
	unsigned int VddRiseDelay;
}
LCD_CONFIG;

/*
// initialize the LCD driver
*/
void lcd_init(LCD_CONFIG* lcd);

/*
// sends a command to the LCD
*/
void lcd_cmd(LCD_CONFIG* lcd, unsigned char cmd);

/*
// sets the position of the lcd cursor
*/
void lcd_set_pos(LCD_CONFIG* lcd, unsigned char line, unsigned char pos);

/*
// writes a character to the LCD
*/
void lcd_write_char(LCD_CONFIG* lcd, unsigned char c);

/*
// writes a string to the LCD
*/
void lcd_write_string(LCD_CONFIG* lcd, unsigned char* str);

/*
// writes a string to the specified LCD line index
*/
void lcd_write_line(LCD_CONFIG* lcd, unsigned char line, unsigned char* str);

/*
// lcd driver background processing
*/
void lcd_idle_processing(LCD_CONFIG* lcd);

#endif
