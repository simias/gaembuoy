#ifndef _GB_GPU_H_
#define _GB_GPU_H_

/* The GPU supports up to 40 sprites concurrently */
#define GB_GPU_MAX_SPRITES 40

enum gb_color {
     GB_COL_WHITE,
     GB_COL_LIGHTGREY,
     GB_COL_DARKGREY,
     GB_COL_BLACK
};

#define GB_LCD_WIDTH  160
#define GB_LCD_HEIGHT 144

union gb_gpu_color {
     /* DMG color: 4 shades */
     enum gb_color dmg_color;
     /* GBC color: xRGB 1555 */
     uint16_t gbc_color;
};

/* Palette used by the GBC */
struct gb_color_palette {
     /* 8 palettes of 4 colors. Each color is stored as xBGR 1555 */
     uint16_t colors[8][4];
     /* Index of the next write in this palette */
     uint8_t write_index;
     /* If true `write_index` auto-increments after a write */
     bool auto_increment;
};

struct gb_gpu {
     /* Background scroll X */
     uint8_t scx;
     /* Background scroll Y */
     uint8_t scy;
     /* Line counter interrupt enable */
     bool iten_lyc;
     /* Mode 0 interrupt enable */
     bool iten_mode0;
     /* Mode 1 interrupt enable */
     bool iten_mode1;
     /* Mode 2 interrupt enable */
     bool iten_mode2;
     /* True if the GPU is enabled */
     bool master_enable;
     /* True if the background is enabled */
     bool bg_enable;
     /* True if the window is enabled */
     bool window_enable;
     /* True if the sprites are enabled */
     bool sprite_enable;
     /* If true sprites are 8x16, otherwise 8x8 */
     bool tall_sprites;
     /* If true the background uses the "high" tile map */
     bool bg_use_high_tm;
     /* If true the window uses the "high" tile map */
     bool window_use_high_tm;
     /* If true the background and window use the sprite tile set */
     bool bg_window_use_sprite_ts;
     /* LY register */
     uint8_t ly;
     /* LYC register */
     uint8_t lyc;
     /* Background palette */
     uint8_t bgp;
     /* Sprite palette 0 */
     uint8_t obp0;
     /* Sprite palette 1 */
     uint8_t obp1;
     /* Window position X + 7 */
     uint8_t wx;
     /* Window position Y */
     uint8_t wy;
     /* Current position within the current line */
     uint16_t line_pos;
     /* Object Attribute Memory (sprite configuration). Each sprite uses 4 bytes
      * for attributes. */
     uint8_t oam[GB_GPU_MAX_SPRITES * 4];
     /* GBC-only: background color palettes */
     struct gb_color_palette bg_palettes;
     /* GBC-only: sprite color palettes */
     struct gb_color_palette sprite_palettes;
};

void gb_gpu_reset(struct gb *gb);
void gb_gpu_sync(struct gb *gb);
void gb_gpu_set_lcd_stat(struct gb *gb, uint8_t stat);
void gb_gpu_set_lcdc(struct gb *gb, uint8_t stat);
uint8_t gb_gpu_get_lcdc(struct gb *gb);
uint8_t gb_gpu_get_ly(struct gb *gb);
uint8_t gb_gpu_get_lcd_stat(struct gb *gb);

#endif /* _GB_GPU_H_ */
