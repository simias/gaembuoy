#include "gb.h"

void gb_input_reset(struct gb *gb) {
     struct gb_input *input = &gb->input;

     input->dpad_state = ~0x10;
     input->dpad_selected = false;
     input->buttons_state = ~0x20;
     input->buttons_selected = false;
}

void gb_input_set(struct gb *gb, unsigned button, bool pressed) {
     struct gb_input *input = &gb->input;
     uint8_t *state;
     uint8_t prev_state;
     unsigned bit;

     prev_state = gb_input_get_state(gb);

     if (button <= GB_INPUT_DOWN) {
          state = &input->dpad_state;
          bit = button;

     } else {
          state = &input->buttons_state;
          bit = button - 4;
     }

     /* All input is active low: the bit is set to 0 when pressed, 1 otherwise
      */
     if (pressed) {
          *state &= ~(1U << bit);
     } else {
          *state |= 1U << bit;
     }

     if (pressed && prev_state != gb_input_get_state(gb)) {
          /* A button was pressed and it's currently selected, that means that
           * we have a negative edge on one of the input terminal which triggers
           * the interrupt. This should also get us out of a STOP state. */
          gb_irq_trigger(gb, GB_IRQ_INPUT);
     }
}

void gb_input_select(struct gb *gb, uint8_t selection) {
     struct gb_input *input = &gb->input;

     input->dpad_selected = ((selection & 0x10) == 0);
     input->buttons_selected = ((selection & 0x20) == 0);
}

uint8_t gb_input_get_state(struct gb *gb) {
     struct gb_input *input = &gb->input;
     uint8_t v = 0xff;

     if (input->dpad_selected) {
          v &= input->dpad_state;
     }

     if (input->buttons_selected) {
          v &= input->buttons_state;
     }

     return v;
}
