#ifndef ILI9341
#define ILI9341

void ili9341_init();
void ili9341_paint();
void ili9341_paint_partial(int16_t x, int16_t y, int16_t width, int16_t height);
char ili9341_is_painting();
void ili9341_do_processing();

#endif
