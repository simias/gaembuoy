#ifndef _GB_DMA_H_
#define _GB_DMA_H_

struct gb_dma {
     bool running;
     /* Source address */
     uint16_t source;
     /* Number of bytes copied so far */
     uint8_t position;
};

void gb_dma_reset(struct gb *gb);
void gb_dma_sync(struct gb *gb);
void gb_dma_start(struct gb *gb, uint8_t source);

#endif /* _GB_DMA_H_ */
