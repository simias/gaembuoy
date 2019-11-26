#include <time.h>
#include "gb.h"

static uint64_t gb_rtc_system_time(void) {
     return time(NULL);
}

static bool gb_rtc_is_halted(struct gb *gb) {
     struct gb_rtc *rtc = &gb->cart.rtc;

     return rtc->latched_date.dh & 0x40;
}

static uint64_t gb_rtc_now_ts(struct gb *gb) {
     struct gb_rtc *rtc = &gb->cart.rtc;

     if (gb_rtc_is_halted(gb)) {
          return rtc->halt_date;
     } else {
          return gb_rtc_system_time();
     }
}

/* Measure the time elapsed since our `base` and return it as a struct
 * gb_rtc_date. If the RTC is halted we measure the time between `base` and
 * `halt_time`, otherwise we measure the time between `base` and `now`. */
static void gb_rtc_latch_date(struct gb *gb, struct gb_rtc_date *date) {
     struct gb_rtc *rtc = &gb->cart.rtc;
     uint64_t now = gb_rtc_now_ts(gb);

     if (now >= rtc->base) {
          /* Convert now to a number of seconds relative to the timer's base */
          now = now - rtc->base;
     } else {
          /* We're *before* the base time somehow. It probably means that the
           * system time changed, let's try to fix things as best as we can. */
          rtc->base = now;
          now = 0;
     }

     /* Seconds */
     date->s = now % 60;
     now /= 60;

     /* Minutes */
     date->m = now % 60;
     now /= 60;

     /* Hours */
     date->h = now % 24;
     now /= 24;

     /* Days */
     date->dl = now & 0xff;
     /* Keep halt bit unchanged but clear day MSB and carry */
     date->dh &= 0x40;
     /* Day MSB */
     date->dh |= (now >> 8) & 1;
     /* Day carry */
     if (now > 0x1ff) {
          /* If the day is greater than 511 we have an overflow, set the carry
           * bit */
          date->dh |= 0x80;
     }
}

/* Recompute the `base` value so that the current time (as returned by
 * `gb_rtc_latch_date`) matches the provided `date` */
void gb_rtc_set_date(struct gb *gb, const struct gb_rtc_date *date) {
     struct gb_rtc *rtc = &gb->cart.rtc;
     uint64_t base = gb_rtc_now_ts(gb);
     uint64_t days;

     days = date->dl;
     /* Day MSB */
     days += (date->dh & 1) * 0x100U;
     /* Day carry */
     days += ((date->dh >> 8) & 1) * 0x200U;

     base -= days * 60 * 60 * 24;
     base -= date->h * 60 * 60;
     base -= date->m * 60;
     base -= date->s;

     rtc->base = base;
}

void gb_rtc_init(struct gb *gb) {
     struct gb_rtc *rtc = &gb->cart.rtc;

     rtc->base = gb_rtc_system_time();
     rtc->halt_date = 0;
     rtc->latch = false;
     /* Make sure the HALT bit is 0 */
     rtc->latched_date.dh = 0;

     gb_rtc_latch_date(gb, &rtc->latched_date);
}

void gb_rtc_latch(struct gb *gb, bool latch) {
     struct gb_rtc *rtc = &gb->cart.rtc;

     if (!rtc->latch && latch) {
          gb_rtc_latch_date(gb, &rtc->latched_date);
     }

     rtc->latch = latch;
}

uint8_t gb_rtc_read(struct gb *gb, unsigned r) {
     struct gb_rtc *rtc = &gb->cart.rtc;

     switch (r) {
     case 0x08:
          return rtc->latched_date.s;
     case 0x09:
          return rtc->latched_date.m;
     case 0x0a:
          return rtc->latched_date.h;
     case 0x0b:
          return rtc->latched_date.dl;
     case 0x0c:
          return rtc->latched_date.dh;
     default:
          return 0xff;
     }
}

void gb_rtc_write(struct gb *gb, unsigned r, uint8_t v) {
     struct gb_rtc *rtc = &gb->cart.rtc;
     struct gb_rtc_date date;
     bool was_halted = gb_rtc_is_halted(gb);

     gb_rtc_latch_date(gb, &date);

     switch (r) {
     case 0x08:
          rtc->latched_date.s = v;
          date.s = v;
          break;
     case 0x09:
          rtc->latched_date.m = v;
          date.m = v;
          break;
     case 0x0a:
          rtc->latched_date.h = v;
          date.h = v;
          break;
     case 0x0b:
          rtc->latched_date.dl = v;
          date.dl = v;
          break;
     case 0x0c:
          rtc->latched_date.dh = v;
          date.dh = v;

          if (!was_halted && gb_rtc_is_halted(gb)) {
               rtc->halt_date = gb_rtc_system_time();
          }

          break;
     default:
          return;
     }

     gb_rtc_set_date(gb, &date);

     gb_rtc_latch_date(gb, &date);
}
