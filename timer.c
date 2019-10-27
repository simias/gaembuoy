#include <stdio.h>
#include "gb.h"

void gb_timer_reset(struct gb *gb) {
     struct gb_timer *timer = &gb->timer;

     timer->divider_counter = 0;
     timer->counter = 0;
     timer->modulo = 0;
     timer->divider = GB_TIMER_DIV_1024;
     timer->started = false;
}

void gb_timer_sync(struct gb *gb) {
     struct gb_timer *timer = &gb->timer;
     int32_t elapsed = gb_sync_resync(gb, GB_SYNC_TIMER);
     int32_t next;
     uint32_t count;
     unsigned div;

     switch (timer->divider) {
     case GB_TIMER_DIV_16:
          div = 16;
          break;
     case GB_TIMER_DIV_64:
          div = 64;
          break;
     case GB_TIMER_DIV_256:
          div = 256;
          break;
     case GB_TIMER_DIV_1024:
          div = 1024;
          break;
     default:
          /* Unreachable */
          die();
     }

     /* Timer runs twice as fast in double-speed mode */
     elapsed <<= gb->double_speed;

     /* Number of counter ticks since last sync */
     count = (elapsed + timer->divider_counter % div) / div;

     /* New value of the divider */
     timer->divider_counter = (timer->divider_counter + elapsed) & 0xffff;

     if (!timer->started) {
          /* Timer isn't running */
          gb_sync_next(gb, GB_SYNC_TIMER, GB_SYNC_NEVER);
          return;
     }

     count += timer->counter;
     while (count > 0xff) {
          /* Timer saturated, reload it with the modulo */
          count -= 0x100;
          count += timer->modulo;
          gb_irq_trigger(gb, GB_IRQ_TIMER);
     }

     timer->counter = count;

     /* Compute remaining number of cycles to the next overflow */
     next = (0x100 - count) * div;
     /* Subtract the remainder in the divider */
     next -= timer->divider_counter % div;

     next >>= gb->double_speed;

     gb_sync_next(gb, GB_SYNC_TIMER, next);
}

void gb_timer_set_config(struct gb *gb, uint8_t config) {
     struct gb_timer *timer = &gb->timer;

     gb_timer_sync(gb);

     timer->started = config & 4;
     timer->divider = config & 3;

     gb_timer_sync(gb);
}

uint8_t gb_timer_get_config(struct gb *gb) {
     struct gb_timer *timer = &gb->timer;
     uint8_t cfg = 0;

     cfg |= timer->divider;

     if (timer->started) {
          cfg |= 4;
     }

     return cfg;
}
