#ifndef _GB_FRONTEND_H_
#define _GB_FRONTEND_H_

enum gb_color {
     GB_COL_WHITE,
     GB_COL_LIGHTGREY,
     GB_COL_DARKGREY,
     GB_COL_BLACK
};

#define GB_LCD_WIDTH  160
#define GB_LCD_HEIGHT 144

struct gb_frontend {
     /* Draw a single line */
     void (*draw_line)(struct gb *gb, unsigned ly,
                       enum gb_color col[GB_LCD_WIDTH]);
     /* Called when we're done drawing a frame and it's ready to be displayed */
     void (*flip)(struct gb *gb);
     /* Handle user input */
     void (*refresh_input)(struct gb *gb);
     /* Called when the emulator wants to quit and the frontend should be
      * destroyed */
     void (*destroy)(struct gb *gb);
     /* Opaque pointer to hold frontend-specific data */
     void *data;
};

#endif /* _GB_FRONTEND_H_ */
