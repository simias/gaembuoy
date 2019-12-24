#ifndef _GB_FRONTEND_H_
#define _GB_FRONTEND_H_

#define GB_MAP_WIDTH     1024U
#define GB_MAP_HEIGHT    1024U

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
     /* Global background offset for map hack */
     int map_x;
     int map_y;
};

void gb_frontend_recenter(struct gb *gb);

#endif /* _GB_FRONTEND_H_ */
