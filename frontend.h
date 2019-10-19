#ifndef _GB_FRONTEND_H_
#define _GB_FRONTEND_H_

struct gb_frontend {
     /* Draw a single line in DMG mode */
     void (*draw_line_dmg)(struct gb *gb, unsigned ly,
                           union gb_gpu_color col[GB_LCD_WIDTH]);
     /* Draw a single line in GBC mode */
     void (*draw_line_gbc)(struct gb *gb, unsigned ly,
                           union gb_gpu_color col[GB_LCD_WIDTH]);
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
