#include <spi.h> 
#include <rtc.h>
#include "lcd.h"
#include <lg.h>

static char painting;
static int x;
static int y;
static int x_start;
static int x_end;
static int y_start;
static int y_end;


#define ILI9341_CMD_POWER_CONTROL_A							(0x3B)
#define ILI9341_CMD_POWER_CONTROL_B							(0xCF)
#define ILI9341_CMD_DRIVER_TIMING_CONTROL_A					(0xE8)
#define ILI9341_CMD_DRIVER_TIMING_CONTROL_B					(0xEA)
#define ILI9341_CMD_POWER_ON_SEQ_CONTROL					(0xED)
#define ILI9341_CMD_PUMP_RATIO_CONTROL						(0xF7)
#define ILI9341_CMD_POWER_CONTROL_1							(0xC0)
#define ILI9341_CMD_POWER_CONTROL_2							(0xC1)
#define ILI9341_CMD_VCOM_CONTROL_1							(0xC5)
#define ILI9341_CMD_VCOM_CONTROL_2							(0xC7)
#define ILI9341_CMD_MEMORY_ACCESS_CONTROL					(0x36)
#define ILI9341_CMD_COLMOD									(0x3A)
#define ILI9341_FRAME_RATE_CONTROL							(0xB1)
#define ILI9341_DISPLAY_FUNCTION_CONTROL					(0xB6)
#define ILI9341_ENTER_SLEEP_MODE							(0x10)
#define ILI9341_SLEEP_OUT									(0x11)
#define ILI9341_DISPLAY_OFF									(0x28)
#define ILI9341_DISPLAY_ON									(0x29)
#define ILI9341_COLUMN_ADDRESS_SET							(0x2A)
#define ILI9341_PAGE_ADDRESS_SET							(0x2B)
#define ILI9341_MEMORY_WRITE								(0x2C)
#define ILI9341_CMD_PARTIAL_AREA							(0x30)
#define ILI9341_CMD_PARTIAL_MODE_ON							(0x12)
#define ILI9341_CMD_NORMAL_DISPLAY_ON						(0x13)

#define ILI9341_CMD_MEMORY_ACCESS_INVERT_ROW_ORDER			(0b10000000)
#define ILI9341_CMD_MEMORY_ACCESS_INVERT_COL_ORDER			(0b01000000)
#define ILI9341_CMD_MEMORY_ACCESS_HORIZONTAL				(0b00100000)
#define ILI9341_CMD_MEMORY_ACCESS_INVERT_VERTICAL_REFRESH	(0b00010000)
#define ILI9341_CMD_MEMORY_ACCESS_BGR						(0b00001000)
#define ILI9341_CMD_MEMORY_ACCESS_INVERT_HORIZONTAL_REFRESH	(0b00000100)

#define ILI9341_COLMOD_16BIT								(0b01010000)
#define ILI9341_COLMOD_18BIT								(0b01100000)

#define LCD_SCREEN_WIDTH 320
#define LCD_SCREEN_HEIGHT 240
 
#define LCD_WRITE_DATA(data)				\
{											\
	IO_PIN_WRITE(B, 3, 1);					\
	spi_write(SPI_GET_MODULE(2), data);		\
}

#define LCD_WRITE_CMD(cmd)				\
{											\
	IO_PIN_WRITE(B, 3, 0);					\
	spi_write(SPI_GET_MODULE(2), cmd);		\
}

#define LCD_ASSERT_CS()			IO_PIN_WRITE(A, 3, 0)
#define LCD_DEASSERT_CS()		IO_PIN_WRITE(A, 3, 1)




void ili9341_set_address(uint16_t x1, uint16_t y1, uint16_t x2,uint16_t y2);

void ili9341_set_address(uint16_t x1, uint16_t y1, uint16_t x2,uint16_t y2)
{	
   LCD_WRITE_CMD(ILI9341_COLUMN_ADDRESS_SET);
   LCD_WRITE_DATA(x1 >> 8);
   LCD_WRITE_DATA(x1);
   LCD_WRITE_DATA(x2 >> 8);
   LCD_WRITE_DATA(x2);
 
   LCD_WRITE_CMD(ILI9341_PAGE_ADDRESS_SET);
   LCD_WRITE_DATA(y1 >> 8);
   LCD_WRITE_DATA(y1);
   LCD_WRITE_DATA(y2 >> 8);
   LCD_WRITE_DATA(y2);    				 
}

void ili9341_paint()
{
	if (!painting)
	{
		painting = 1;
		x = 0;
		y = 0;
		ili9341_set_address(x, y, x_end - 1, y_end - 1);
		LCD_WRITE_CMD(ILI9341_MEMORY_WRITE);
	}	
}

void ili9341_paint_partial(int16_t x_pos, int16_t y_pos, int16_t width, int16_t height)
{
	if (painting)
	{
		if ((x_pos < x_start || x_pos + width > x_end) ||
			(y_pos < y_start || y_pos + height > y_end))			
		{
			x_start = x = MIN(x_start, x_pos);
			y_start = y = MIN(y_start, y_pos);
			x_end = MAX(x_end, x + width);
			y_end = MAX(y_end, y + height);
			/*
			// set the partial area
			*/
			ili9341_set_address(x, y, x_end - 1, y_end - 1);
			LCD_WRITE_CMD(ILI9341_MEMORY_WRITE);
		}
	}
	else
	{
		painting = 1;
		x_start = x = x_pos;
		y_start = y = y_pos;
		x_end = x + width;
		y_end = y + height;
		/*
		// set the partial area
		*/
		ili9341_set_address(x, y, x_end - 1, y_end - 1);
		LCD_WRITE_CMD(ILI9341_MEMORY_WRITE);
	}
}

char ili9341_is_painting()
{
	return painting;
}

void ili9341_do_processing()
{
	
	static LG_RGB pixel_color;
	
	if (painting)
	{
		
		pixel_color = lg_get_pixel(x, y);
		
		LCD_WRITE_DATA(pixel_color.r);
		LCD_WRITE_DATA(pixel_color.g);
		LCD_WRITE_DATA(pixel_color.b);
		
		x++;
		if (x >= x_end) 
		{
			x = x_start;
			y++;
			if (y >= y_end)
			{
				painting = 0;
				x_start = x = 0;
				y_start = y = 0;
				x_end = LCD_SCREEN_WIDTH;
				y_end = LCD_SCREEN_WIDTH;
				
				ili9341_set_address(0, 0, LCD_SCREEN_WIDTH - 1, LCD_SCREEN_HEIGHT - 1);
	

			}
		}
	}

}


 void ili9341_init()
 {
	int i;
	painting = 0;
	/*
	// initialize IO pins
	*/
	IO_PIN_WRITE(A, 3, 1);
	IO_PIN_WRITE(A, 4, 1);
	IO_PIN_SET_AS_OUTPUT(A, 3);	
	IO_PIN_SET_AS_OUTPUT(A, 2);
	/*
	// initialize spi module
	*/
	spi_init(SPI_GET_MODULE(2));
	spi_set_clock(SPI_GET_MODULE(2), 15000000L);
	
	/*
	// reset
	*/
	IO_PIN_WRITE(A, 2, 0);
	for (i = 0; i < 0xFFF; i++);
	IO_PIN_WRITE(A, 2, 1);	
	
	LCD_ASSERT_CS();
	
	LCD_WRITE_CMD(ILI9341_CMD_POWER_CONTROL_A);  
	LCD_WRITE_DATA(0x39); 
	LCD_WRITE_DATA(0x2C); 
	LCD_WRITE_DATA(0x00); 
	LCD_WRITE_DATA(0x34); 
	LCD_WRITE_DATA(0x02); 
	
	LCD_WRITE_CMD(ILI9341_CMD_POWER_CONTROL_B);  
	LCD_WRITE_DATA(0x00); 
	LCD_WRITE_DATA(0XC1); 
	LCD_WRITE_DATA(0X30); 
	
	LCD_WRITE_CMD(ILI9341_CMD_DRIVER_TIMING_CONTROL_A);  
	LCD_WRITE_DATA(0x85); 
	LCD_WRITE_DATA(0x00); 
	LCD_WRITE_DATA(0x78); 
	
	LCD_WRITE_CMD(ILI9341_CMD_DRIVER_TIMING_CONTROL_B);  
	LCD_WRITE_DATA(0x00); 
	LCD_WRITE_DATA(0x00); 
	
	LCD_WRITE_CMD(ILI9341_CMD_POWER_ON_SEQ_CONTROL);  
	LCD_WRITE_DATA(0x64); 
	LCD_WRITE_DATA(0x03); 
	LCD_WRITE_DATA(0X12); 
	LCD_WRITE_DATA(0X81); 
	
	LCD_WRITE_CMD(ILI9341_CMD_PUMP_RATIO_CONTROL);  // PUMP RATIO
	LCD_WRITE_DATA(/*0x20*/ 0b00110000); 
	
	LCD_WRITE_CMD(ILI9341_CMD_POWER_CONTROL_1);    //Power control 
	LCD_WRITE_DATA(/*0x23*/ 0b00000011);   //VRH[5:0] 
	
	LCD_WRITE_CMD(ILI9341_CMD_POWER_CONTROL_2);    //Power control 
	LCD_WRITE_DATA(/*0x10*/ 0b00000011);   //SAP[2:0];BT[3:0] 
	
	LCD_WRITE_CMD(ILI9341_CMD_VCOM_CONTROL_1);    //VCM control 
	LCD_WRITE_DATA(/*0x3e*/ 0b00001100);   //Contrast
	LCD_WRITE_DATA(/*0x28*/ 0b001100100); 
	
	LCD_WRITE_CMD(ILI9341_CMD_VCOM_CONTROL_1);    //VCM control2 
	LCD_WRITE_DATA(0x86);   //--
	

	LCD_WRITE_CMD(ILI9341_CMD_MEMORY_ACCESS_CONTROL);
	LCD_WRITE_DATA(ILI9341_CMD_MEMORY_ACCESS_HORIZONTAL | ILI9341_CMD_MEMORY_ACCESS_BGR);
	
	LCD_WRITE_CMD(ILI9341_CMD_COLMOD);    
	LCD_WRITE_DATA(ILI9341_COLMOD_18BIT); 
	
	LCD_WRITE_CMD(ILI9341_FRAME_RATE_CONTROL);    /* this is just for MCU interface?? */
	LCD_WRITE_DATA(0x00);  
	LCD_WRITE_DATA(0x18); 
	
	LCD_WRITE_CMD(ILI9341_DISPLAY_FUNCTION_CONTROL);    /* ??? */
	LCD_WRITE_DATA(0x08); 
	LCD_WRITE_DATA(0x82);
	LCD_WRITE_DATA(0x27);  
	
	/* 
	LCD_WRITE_CMD(0xF2);    // 3Gamma Function Disable 
	LCD_WRITE_DATA(0x00); 
	
	LCD_WRITE_CMD(0x26);    //Gamma curve selected 
	LCD_WRITE_DATA(0x01); 
	
	LCD_WRITE_CMD(0xE0);    //Set Gamma 
	LCD_WRITE_DATA(0x0F); 
	LCD_WRITE_DATA(0x31); 
	LCD_WRITE_DATA(0x2B); 
	LCD_WRITE_DATA(0x0C); 
	LCD_WRITE_DATA(0x0E); 
	LCD_WRITE_DATA(0x08); 
	LCD_WRITE_DATA(0x4E); 
	LCD_WRITE_DATA(0xF1); 
	LCD_WRITE_DATA(0x37); 
	LCD_WRITE_DATA(0x07); 
	LCD_WRITE_DATA(0x10); 
	LCD_WRITE_DATA(0x03); 
	LCD_WRITE_DATA(0x0E); 
	LCD_WRITE_DATA(0x09); 
	LCD_WRITE_DATA(0x00); 
	
	LCD_WRITE_CMD(0XE1);    //Set Gamma 
	LCD_WRITE_DATA(0x00); 
	LCD_WRITE_DATA(0x0E); 
	LCD_WRITE_DATA(0x14); 
	LCD_WRITE_DATA(0x03); 
	LCD_WRITE_DATA(0x11); 
	LCD_WRITE_DATA(0x07); 
	LCD_WRITE_DATA(0x31); 
	LCD_WRITE_DATA(0xC1); 
	LCD_WRITE_DATA(0x48); 
	LCD_WRITE_DATA(0x08); 
	LCD_WRITE_DATA(0x0F); 
	LCD_WRITE_DATA(0x0C); 
	LCD_WRITE_DATA(0x31); 
	LCD_WRITE_DATA(0x36); 
	LCD_WRITE_DATA(0x0F); 
	*/
	
	LCD_WRITE_CMD(ILI9341_SLEEP_OUT); 
	rtc_sleep(100);
	
	LCD_WRITE_CMD(ILI9341_DISPLAY_ON);    //Display on 

	ili9341_set_address(0, 0, LCD_SCREEN_WIDTH - 1, LCD_SCREEN_HEIGHT - 1);
	
	x_start = x = 0;
	y_start = y = 0;
	x_end = LCD_SCREEN_WIDTH;
	y_end = LCD_SCREEN_HEIGHT;
	
	
	/*
	LCD_WRITE_CMD(ILI9341_CMD_PARTIAL_AREA);
	LCD_WRITE_DATA(0);
	LCD_WRITE_DATA(0);
	LCD_WRITE_DATA(0);
	LCD_WRITE_DATA(50);
	*/
	
	//LCD_WRITE_CMD(0x2c);  
	
}
