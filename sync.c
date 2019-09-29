#include <stdio.h>
#include "gb.h"

void gb_sync_reset(struct gb *gb) {
     struct gb_sync *sync = &gb->sync;
     unsigned i;

     for (i = 0; i < GB_SYNC_NUM; i++) {
          sync->last_sync[i] = 0;
     }

     gb->timestamp = 0;
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
