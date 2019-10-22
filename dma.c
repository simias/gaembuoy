#include "gb.h"

#define GB_DMA_LENGTH_BYTES (GB_GPU_MAX_SPRITES * 4)

void gb_dma_reset(struct gb *gb) {
     struct gb_dma *dma = &gb->dma;

     dma->running = false;
     dma->source = 0;
     dma->position = 0;
}

void gb_dma_sync(struct gb *gb) {
     struct gb_dma *dma = &gb->dma;
     int32_t elapsed = gb_sync_resync(gb, GB_SYNC_DMA);
     unsigned length;

     if (!dma->running) {
          /* Nothing to do */
          gb_sync_next(gb, GB_SYNC_DMA, GB_SYNC_NEVER);
          return;
     }

     /* CPU always increments the counter in multiples of 4 cycles (2 in
      * double-speed mode) so we know for sure that there won't be any remainder
      * here. */
     length = elapsed / (4 >> gb->double_speed);

     while (length && dma->position < GB_DMA_LENGTH_BYTES) {
          uint32_t b = gb_memory_readb(gb, dma->source + dma->position);

          gb->gpu.oam[dma->position] = b;

          length--;
          dma->position++;
     }

     if (dma->position >= GB_DMA_LENGTH_BYTES) {
          /* We're done */
          dma->running = false;
          gb_sync_next(gb, GB_SYNC_DMA, GB_SYNC_NEVER);
     } else {
          /* The DMA copies one byte ever 4 cycles (2 cycles in double-speed
           * mode) */
          gb_sync_next(gb, GB_SYNC_DMA, 4 >> gb->double_speed);
     }
}

void gb_dma_start(struct gb *gb, uint8_t source) {
     struct gb_dma *dma = &gb->dma;

     /* Sync our state in case we were already running */
     gb_dma_sync(gb);

     dma->source = (uint16_t)source << 8;
     dma->position = 0;

     /* The GBC can copy directly from the cartridge, DMG only from RAM */
     if ((!gb->gbc && dma->source < 0x8000U) || dma->source >= 0xe000U) {
          /* The DMA can't access this memory region */
          dma->running = false;
     } else {
          dma->running = true;
     }


     gb_dma_sync(gb);
}
