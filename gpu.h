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
};

void gb_gpu_set_lcd_stat(struct gb *gb, uint8_t stat);

#endif /* _GB_GPU_H_ */
