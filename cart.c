#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "gb.h"

/* I think the biggest licensed GB cartridge is 8MB but let's add a margin in
 * case there are homebrews with even bigger carts. */
#define GB_CART_MAX_SIZE (32U * 1024 * 1024)

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

     if (addr >= cart->rom_length) {
          fprintf(stderr,
                  "ROM read out of bounds (length: 0x%x, addr: 0x%04x)\n",
                  cart->rom_length, addr);
          die();
     }

     return cart->rom[addr];
}
