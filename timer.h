#ifndef _GB_TIMER_H_
#define _GB_TIMER_H_

enum gb_timer_divider {
     /* Timer frequency: 4096Hz */
     GB_TIMER_DIV_1024 = 0,
     /* Timer frequency: 262144Hz */
     GB_TIMER_DIV_16 = 1,
     /* Timer frequency: 65535Hz */
     GB_TIMER_DIV_64 = 2,
     /* Timer frequency: 16384Hz */
     GB_TIMER_DIV_256 = 3,
};

struct gb_timer {
     uint16_t divider_counter;
     uint8_t counter;
     uint8_t modulo;
     enum gb_timer_divider divider;
     bool started;
};

void gb_timer_reset(struct gb *gb);
void gb_timer_sync(struct gb *gb);
void gb_timer_set_config(struct gb *gb, uint8_t config);
uint8_t gb_timer_get_config(struct gb *gb);

#endif /* _GB_TIMER_H_ */
