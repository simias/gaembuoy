#include <stdio.h>
#include "gb.h"

void gb_sync_reset(struct gb *gb) {
     struct gb_sync *sync = &gb->sync;
     unsigned i;

     for (i = 0; i < GB_SYNC_NUM; i++) {
          sync->last_sync[i] = 0;
          sync->next_event[i] = 0;
     }

     gb->timestamp = 0;
     sync->first_event = 0;
}

int32_t gb_sync_resync(struct gb *gb, enum gb_sync_token token) {
     struct gb_sync *sync = &gb->sync;
     int32_t elapsed = gb->timestamp - sync->last_sync[token];

     if (elapsed < 0) {
          fprintf(stderr, "Got negative sync %d for token %u",
                    elapsed, token);
     }

     sync->last_sync[token] = gb->timestamp;

     return elapsed;
}

void gb_sync_next(struct gb *gb, enum gb_sync_token token, int32_t cycles) {
     struct gb_sync *sync = &gb->sync;
     unsigned i;

     sync->next_event[token] = gb->timestamp + cycles;

     /* Recompute the date of the first event to come */
     sync->first_event = sync->next_event[0];

     for (i = 1; i < GB_SYNC_NUM; i++) {
          int32_t e = sync->next_event[i];
          if (e < sync->first_event) {
               sync->first_event = e;
          }
     }
}

void gb_sync_check_events(struct gb *gb) {
     struct gb_sync *sync = &gb->sync;

     /* It's possible for an event to actually "freeze" the CPU and increase the
      * timestamp counter (in particular the HDMA running on HSYNC). Therefore
      * we have to recheck for a potential event in a loop to make sure we only
      * return control to the caller when all events have been processed. */
     while (gb->timestamp >= gb->sync.first_event) {
          int32_t ts = gb->timestamp;

          if (ts >= sync->next_event[GB_SYNC_GPU]) {
               gb_gpu_sync(gb);
          }

          if (ts >= sync->next_event[GB_SYNC_DMA]) {
               gb_dma_sync(gb);
          }

          if (ts >= sync->next_event[GB_SYNC_TIMER]) {
               gb_timer_sync(gb);
          }

          if (ts >= sync->next_event[GB_SYNC_SPU]) {
               gb_spu_sync(gb);
          }

          if (ts >= sync->next_event[GB_SYNC_CART]) {
               gb_cart_sync(gb);
          }
     }
}

/* Subtract the current value of the timestamp from all last_sync and next_event
 * dates, therefore avoiding potential overflows while keeping everything in
 * sync */
void gb_sync_rebase(struct gb *gb) {
     struct gb_sync *sync = &gb->sync;
     unsigned i;

     for (i = 0; i < GB_SYNC_NUM; i++) {
          sync->last_sync[i] -= gb->timestamp;
          sync->next_event[i] -= gb->timestamp;
     }

     sync->first_event -= gb->timestamp;
     gb->timestamp = 0;
}
