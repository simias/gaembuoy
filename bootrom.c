#include "gb.h"

void gb_bootrom_load(struct gb *gb) {
     struct gb_bootrom *bootrom = &gb->bootrom;
     const char *bootrom_file;
     size_t expected_size;
     size_t nread;
     FILE *f;

     bootrom->active = false;

     if (gb->gbc) {
          bootrom_file = "bootrom.gbc";
          expected_size = GB_BOOTROM_LEN_GBC;
     } else {
          bootrom_file = "bootrom.gb";
          expected_size = GB_BOOTROM_LEN_DMG;
     }

     f = fopen(bootrom_file, "rb");
     if (f == NULL) {
          fprintf(stderr, "Can't open '%s'\n", bootrom_file);
          goto norom;
     }

     nread = fread(bootrom->rom, 1, expected_size, f);
     if (nread != expected_size) {
          fprintf(stderr, "Boot ROM file '%s' is too small\n", bootrom_file);
          goto norom;
     }

     printf("Boot ROM '%s' loaded\n", bootrom_file);
     bootrom->active = true;
     fclose(f);

     return;

norom:
     if (f) {
          fclose(f);
     }

     fprintf(stderr, "Couldn't load boot ROM, skipping to cartridge\n");
     gb->cpu.pc = 0x100;
     if (gb->gbc) {
          /* In GBC mode the boot ROM sets A to 0x11 before starting the game.
           * The game can use this to detect whether it's running on DMG or GBC.
           */
          gb->cpu.a = 0x11;
     }
}
