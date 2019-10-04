#include "gb.h"

void gb_irq_reset(struct gb *gb) {
     struct gb_irq *irq = &gb->irq;

     irq->irq_flags = 0xE0;
     irq->irq_enable = 0;
}

void gb_irq_trigger(struct gb *gb, enum gb_irq_token which) {
     struct gb_irq *irq = &gb->irq;

     irq->irq_flags |= (1U << which);
}
