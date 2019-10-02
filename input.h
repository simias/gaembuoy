#ifndef _GB_INPUT_H_
#define _GB_INPUT_H_

#define GB_INPUT_RIGHT  0
#define GB_INPUT_LEFT   1
#define GB_INPUT_UP     2
#define GB_INPUT_DOWN   3
#define GB_INPUT_A      4
#define GB_INPUT_B      5
#define GB_INPUT_SELECT 6
#define GB_INPUT_START  7

struct gb_input {
     /* State of the D-pad (right, left, up, down), active low */
     uint8_t dpad_state;
     /* True if the D-pad is selected */
     bool dpad_selected;
     /* State of the buttons (A, B, select, start), active low */
     uint8_t buttons_state;
     /* True if the buttons are selected */
     bool buttons_selected;
};

void gb_input_reset(struct gb *gb);
void gb_input_set(struct gb *gb, unsigned button, bool pressed);
void gb_input_select(struct gb *gb, uint8_t selection);
uint8_t gb_input_get_state(struct gb *gb);

#endif /* _GB_INPUT_H_ */
