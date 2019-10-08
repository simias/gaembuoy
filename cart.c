#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "gb.h"

/* GB ROMs are at least 32KB */
#define GB_CART_MIN_SIZE (32 * 1024)

/* I think the biggest licensed GB cartridge is 8MB but let's add a margin in
 * case there are homebrews with even bigger carts. */
#define GB_CART_MAX_SIZE (32U * 1024 * 1024)

#define GB_CART_OFF_TYPE      0x147

void gb_cart_load(struct gb *gb, const char *rom_path) {
     struct gb_cart *cart = &gb->cart;
     FILE *f = fopen(rom_path, "rb");
     long l;
     size_t nread;

     cart->rom = NULL;

     if (f == NULL) {
          perror("Can't open ROM file");
          goto error;
     }

     if (fseek(f, 0, SEEK_END) == -1 ||
         (l = ftell(f)) == -1 ||
         fseek(f, 0, SEEK_SET) == -1) {
          fclose(f);
          perror("Can't get ROM file length");
          goto error;
     }

     if (l == 0) {
          fprintf(stderr, "ROM file is empty!\n");
          goto error;
     }

     if (l > GB_CART_MAX_SIZE) {
          fprintf(stderr, "ROM file is too big!\n");
          goto error;
     }

     if (l < GB_CART_MIN_SIZE) {
          fprintf(stderr, "ROM file is too small!\n");
          goto error;
     }

     cart->rom_length = l;
     cart->rom = calloc(1, cart->rom_length);
     if (cart->rom == NULL) {
          perror("Can't allocate ROM buffer");
          goto error;
     }

     nread = fread(cart->rom, 1, cart->rom_length, f);
     if (nread < cart->rom_length) {
          fprintf(stderr,
                  "Failed to load ROM file (read %u bytes, expected %u)\n",
                  (unsigned)nread, cart->rom_length);
          goto error;
     }

     switch (cart->rom[GB_CART_OFF_TYPE]) {
     case 0x00:
          cart->model = GB_CART_SIMPLE;
          break;
     default:
          fprintf(stderr, "Unsupported cartridge type %x\n",
                  cart->rom[GB_CART_OFF_TYPE]);
          goto error;
     }

     /* Success */
     fclose(f);
     return;

error:
     if (cart->rom) {
          free(cart->rom);
          cart->rom = NULL;
     }

     if (f) {
          fclose(f);
     }

     die();
}

void gb_cart_unload(struct gb *gb) {
     struct gb_cart *cart = &gb->cart;

     if (cart->rom) {
          free(cart->rom);
          cart->rom = NULL;
     }
}

uint8_t gb_cart_rom_readb(struct gb *gb, uint16_t addr) {
     struct gb_cart *cart = &gb->cart;
     unsigned rom_off = addr;

     switch (cart->model) {
     case GB_CART_SIMPLE:
          /* No mapper */
          break;
     default:
          /* Should not be reached */
          die();
     }

     return cart->rom[rom_off];
}

void gb_cart_rom_writeb(struct gb *gb, uint16_t addr, uint8_t v) {
     struct gb_cart *cart = &gb->cart;

     switch (cart->model) {
     case GB_CART_SIMPLE:
          /* Nothing to be done */
          break;
     default:
          /* Should not be reached */
          die();
     }
}

uint8_t gb_cart_ram_readb(struct gb *gb, uint16_t addr) {
     struct gb_cart *cart = &gb->cart;

     switch (cart->model) {
     case GB_CART_SIMPLE:
          /* No RAM */
          return 0xff;
     default:
          /* Should not be reached */
          die();
          return 0xff;
     }
}

void gb_cart_ram_writeb(struct gb *gb, uint16_t addr, uint8_t v) {
     struct gb_cart *cart = &gb->cart;

     switch (cart->model) {
     case GB_CART_SIMPLE:
          /* No RAM */
          return;
     default:
          /* Should not be reached */
          die();
     }
}
