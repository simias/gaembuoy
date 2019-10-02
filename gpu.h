#ifndef _GB_GPU_H_
#define _GB_GPU_H_

/* The GPU supports up to 40 sprites concurrently */
#define GB_GPU_MAX_SPRITES 40

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
     /* Background palette */
     uint8_t bgp;
     /* Sprite palette 0 */
     uint8_t obp0;
     /* Sprite palette 1 */
     uint8_t obp1;
     /* Current position within the current line */
     uint16_t line_pos;
     /* Object Attribute Memory (sprite configuration). Each sprite uses 4 bytes
      * for attributes. */
     uint8_t oam[GB_GPU_MAX_SPRITES * 4];
};

void gb_gpu_reset(struct gb *gb);
void gb_gpu_sync(struct gb *gb);
void gb_gpu_set_lcd_stat(struct gb *gb, uint8_t stat);
void gb_gpu_set_lcdc(struct gb *gb, uint8_t stat);
uint8_t gb_gpu_get_lcdc(struct gb *gb);
uint8_t gb_gpu_get_ly(struct gb *gb);
uint8_t gb_gpu_get_lcd_stat(struct gb *gb);

#endif /* _GB_GPU_H_ */
