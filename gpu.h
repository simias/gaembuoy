#ifndef _GB_GPU_H_
#define _GB_GPU_H_

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
     /* LCD Control register */
     uint8_t lcdc;
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
};

void gb_gpu_reset(struct gb *gb);
void gb_gpu_sync(struct gb *gb);
void gb_gpu_set_lcd_stat(struct gb *gb, uint8_t stat);
void gb_gpu_set_lcdc(struct gb *gb, uint8_t stat);
uint8_t gb_gpu_get_lcdc(struct gb *gb);
uint8_t gb_gpu_get_ly(struct gb *gb);
uint8_t gb_gpu_get_lcd_stat(struct gb *gb);

#endif /* _GB_GPU_H_ */
