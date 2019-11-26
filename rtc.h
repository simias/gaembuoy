#ifndef _GB_RTC_H_
#define _GB_RTC_H_

struct gb_rtc_date {
     /* Second counter value (0-59) */
     uint8_t s;
     /* Minute counter value (0-59) */
     uint8_t m;
     /* Hour counter value (0-23) */
     uint8_t h;
     /* Day counter value, low 8 bits (0-255) */
     uint8_t dl;
     /* Day counter value MSB + HALT (bit 6) + day carry (bit 7) */
     uint8_t dh;
};

struct gb_rtc {
     /* System time corresponding to 00:00:00 day 0 in the emulated RTC time */
     uint64_t base;
     /* If we're halted this variable contains the date at the time of the halt
      */
     uint64_t halt_date;
     /* Current latch value. Date is latched when this variable transitions from
      * 0 to 1 */
     bool latch;
     /* Currently latched date */
     struct gb_rtc_date latched_date;
};

void gb_rtc_init(struct gb *gb);
void gb_rtc_latch(struct gb *gb, bool latch);
uint8_t gb_rtc_read(struct gb *gb, unsigned r);
void gb_rtc_write(struct gb *gb, unsigned r, uint8_t v);
void gb_rtc_dump(struct gb *gb, FILE *f);
void gb_rtc_load(struct gb *gb, FILE *f);

#endif /* _GB_RTC_H_ */
