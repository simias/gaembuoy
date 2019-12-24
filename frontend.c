#include "gb.h"

void gb_frontend_recenter(struct gb *gb) {
     gb->frontend.map_x = (GB_MAP_WIDTH - GB_LCD_WIDTH) / 2;
     gb->frontend.map_y = (GB_MAP_HEIGHT - GB_LCD_HEIGHT) / 2;
}
