#ifndef _GB_IRQ_H_
#define _GB_IRQ_H_

enum gb_irq_token {
     /* Triggered on LCD VSYNC */
     GB_IRQ_VSYNC = 0,
     /* Triggered based on LCD STAT value */
     GB_IRQ_LCD_STAT = 1,
     /* Triggered on timer overflow */
     GB_IRQ_TIMER = 2,
     /* Triggered on serial transfer completion */
     GB_IRQ_SERIAL = 3,
     /* Triggered on button press */
     GB_IRQ_INPUT = 4,
};

struct gb_irq {
     uint8_t irq_flags;
     uint8_t irq_enable;
};

void gb_irq_reset(struct gb *gb);
void gb_irq_trigger(struct gb *gb, enum gb_irq_token which);

#endif /* _GB_IRQ_H_ */
