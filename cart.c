#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "gb.h"

/* 16KB ROM banks */
#define GB_ROM_BANK_SIZE (16 * 1024)
/* 8KB RAM banks */
#define GB_RAM_BANK_SIZE (8 * 1024)

/* GB ROMs are at least 32KB (2 banks) */
#define GB_CART_MIN_SIZE (GB_ROM_BANK_SIZE * 2)

/* I think the biggest licensed GB cartridge is 8MB but let's add a margin in
 * case there are homebrews with even bigger carts. */
#define GB_CART_MAX_SIZE (32U * 1024 * 1024)

#define GB_CART_OFF_TITLE     0x134
#define GB_CART_OFF_TYPE      0x147
#define GB_CART_OFF_ROM_BANKS 0x148
#define GB_CART_OFF_RAM_BANKS 0x149

static void gb_cart_get_rom_title(struct gb *gb, char title[17]) {
     struct gb_cart *cart = &gb->cart;
     unsigned i;

     for (i = 0; i < 16; i++) {
          char c = cart->rom[GB_CART_OFF_TITLE + i];

          if (c == 0) {
               /* End-of-title */
               break;
          }

          if (!isprint(c)) {
               c = '?';
          }

          title[i] = c;
     }

     /* End of string */
     title[i] = '\0';
}

void gb_cart_load(struct gb *gb, const char *rom_path) {
     struct gb_cart *cart = &gb->cart;
     FILE *f = fopen(rom_path, "rb");
     long l;
     size_t nread;
     char rom_title[17];

     cart->rom = NULL;
     cart->cur_rom_bank = 1;
     cart->ram = NULL;
     cart->cur_ram_bank = 0;
     cart->ram_write_protected = true;
     cart->mbc1_bank_ram = false;

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

     /* Figure out the number of ROM banks for this cartridge */
     switch (cart->rom[GB_CART_OFF_ROM_BANKS]) {
     case 0:
          cart->rom_banks = 2;
          break;
     case 1:
          cart->rom_banks = 4;
          break;
     case 2:
          cart->rom_banks = 8;
          break;
     case 3:
          cart->rom_banks = 16;
          break;
     case 4:
          cart->rom_banks = 32;
          break;
     case 5:
          cart->rom_banks = 64;
          break;
     case 6:
          cart->rom_banks = 128;
          break;
     case 7:
          cart->rom_banks = 256;
          break;
     case 8:
          cart->rom_banks = 512;
          break;
     case 0x52:
          cart->rom_banks = 72;
          break;
     case 0x53:
          cart->rom_banks = 80;
          break;
     case 0x54:
          cart->rom_banks = 96;
          break;
     default:
          fprintf(stderr, "Unknown ROM size configuration: %x\n",
                  cart->rom[GB_CART_OFF_ROM_BANKS]);
          goto error;
     }

     /* Make sure the ROM file size is coherent with the declared number of ROM
      * banks */
     if (cart->rom_length < cart->rom_banks * GB_ROM_BANK_SIZE) {
          fprintf(stderr, "ROM file is too small to hold the declared"
                  " %d ROM banks\n", cart->rom_banks);
          goto error;
     }

     /* Figure out the number of RAM banks for this cartridge */
     switch (cart->rom[GB_CART_OFF_RAM_BANKS]) {
     case 0: /* No RAM */
          cart->ram_banks = 0;
          cart->ram_length = 0;
          break;
     case 1:
          /* One bank but only 2kB (so really 1/4 of a bank) */
          cart->ram_banks = 1;
          cart->ram_length = GB_RAM_BANK_SIZE / 4;
          break;
     case 2:
          cart->ram_banks = 1;
          cart->ram_length = GB_RAM_BANK_SIZE;
          break;
     case 3:
          cart->ram_banks = 4;
          cart->ram_length = GB_RAM_BANK_SIZE * 4;
          break;
     case 4:
          cart->ram_banks = 16;
          cart->ram_length = GB_RAM_BANK_SIZE * 16;
          break;
     default:
          fprintf(stderr, "Unknown RAM size configuration: %x\n",
                  cart->rom[GB_CART_OFF_RAM_BANKS]);
          goto error;
     }

     switch (cart->rom[GB_CART_OFF_TYPE]) {
     case 0x00:
          cart->model = GB_CART_SIMPLE;
          break;
     case 0x01: /* MBC1, no RAM */
     case 0x02: /* MBC1, with RAM */
     case 0x03: /* MBC1, with RAM and battery backup */
          cart->model = GB_CART_MBC1;
          break;
     case 0x05: /* MBC2 */
     case 0x06: /* MBC2 with battery backup */
          cart->model = GB_CART_MBC2;
          /* MBC2 always has 512 * 4bits of RAM available */
          cart->ram_banks = 1;
          /* Allocate 512 bytes for convenience, but only the low 4 bits should
           * be used */
          cart->ram_length = 512;
          break;
     default:
          fprintf(stderr, "Unsupported cartridge type %x\n",
                  cart->rom[GB_CART_OFF_TYPE]);
          goto error;
     }

     /* Allocate RAM buffer */
     if (cart->ram_length > 0) {
          cart->ram = calloc(1, cart->ram_length);
          if (cart->ram == NULL) {
               perror("Can't allocate RAM buffer");
               goto error;
          }
     }

     /* Success */
     fclose(f);

     gb_cart_get_rom_title(gb, rom_title);

     printf("Succesfully Loaded %s\n", rom_path);
     printf("Title: '%s'\n", rom_title);
     printf("ROM banks: %u (%uKiB)\n", cart->rom_banks,
            cart->rom_banks * GB_ROM_BANK_SIZE / 1024);
     printf("RAM banks: %u (%uKiB)\n", cart->ram_banks,
            cart->ram_length / 1024);
     return;

error:
     if (cart->rom) {
          free(cart->rom);
          cart->rom = NULL;
     }

     if (cart->ram) {
          free(cart->ram);
          cart->ram = NULL;
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

     if (cart->ram) {
          free(cart->ram);
          cart->ram = NULL;
     }
}

uint8_t gb_cart_rom_readb(struct gb *gb, uint16_t addr) {
     struct gb_cart *cart = &gb->cart;
     unsigned rom_off = addr;

     switch (cart->model) {
     case GB_CART_SIMPLE:
          /* No mapper */
          break;
     case GB_CART_MBC1:
          if (addr >= GB_ROM_BANK_SIZE) {
               /* Bank 1 can be remapped through this controller */
               unsigned bank = cart->cur_rom_bank;

               if (cart->mbc1_bank_ram) {
                    /* When MBC1 is configured to bank RAM it can only address
                     * 16 ROM banks */
                    bank %= 32;
               } else {
                    bank %= 128;
               }

               if (bank == 0) {
                    /* Bank 0 can't be mirrored that way, using a bank of 0 is
                     * the same thing as using 1 */
                    bank = 1;
               }

               bank %= cart->rom_banks;

               rom_off += (bank - 1) * GB_ROM_BANK_SIZE;
          }
          break;
     case GB_CART_MBC2:
          if (addr >= GB_ROM_BANK_SIZE) {
               rom_off += (cart->cur_rom_bank - 1) * GB_ROM_BANK_SIZE;
          }
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
     case GB_CART_MBC1:
          if (addr < 0x2000) {
               cart->ram_write_protected = ((v & 0xf) != 0xa);
          } else if (addr < 0x4000) {
               /* Set ROM bank, bits [4:0] */
               cart->cur_rom_bank &= ~0x1f;
               cart->cur_rom_bank |= v & 0x1f;
          } else if (addr < 0x6000) {
               /* Set RAM bank *or* ROM bank [6:5] depending on the mode */
               cart->cur_rom_bank &= 0x1f;
               cart->cur_rom_bank |= (v & 3) << 5;

               if (cart->ram_banks > 0) {
                    cart->cur_ram_bank = (v & 3) % cart->ram_banks;
               }
          } else {
               /* Change MBC1 banking mode */
               cart->mbc1_bank_ram = v & 1;
          }
          break;
     case GB_CART_MBC2:
          if (addr < 0x2000) {
               cart->ram_write_protected = ((v & 0xf) != 0xa);
          } else if (addr < 0x4000) {
               cart->cur_rom_bank = v & 0xf;
               if (cart->cur_rom_bank == 0) {
                    cart->cur_rom_bank = 1;
               }
          }
          break;
     default:
          /* Should not be reached */
          die();
     }
}

unsigned gb_cart_mbc1_ram_off(struct gb *gb, uint16_t addr) {
     struct gb_cart *cart = &gb->cart;
     unsigned bank;

     if (cart->ram_banks == 1) {
          /* Cartridges which only have one RAM bank can have only a
           * partial 2KB RAM chip that's mirrored 4 times. */
          return addr % cart->ram_length;
     }

     bank = cart->cur_ram_bank;

     if (cart->mbc1_bank_ram) {
          bank %= 4;
     } else {
          /* In this mode we only support one bank */
          bank = 0;
     }

     return bank * GB_RAM_BANK_SIZE + addr;
}

uint8_t gb_cart_ram_readb(struct gb *gb, uint16_t addr) {
     struct gb_cart *cart = &gb->cart;
     unsigned ram_off;

     switch (cart->model) {
     case GB_CART_SIMPLE:
          /* No RAM */
          return 0xff;
     case GB_CART_MBC1:
          if (cart->ram_banks == 0) {
               /* No RAM */
               return 0xff;
          }

          ram_off = gb_cart_mbc1_ram_off(gb, addr);
          break;
     case GB_CART_MBC2:
          ram_off = addr % 512;
          break;
     default:
          /* Should not be reached */
          die();
          return 0xff;
     }

     return cart->ram[ram_off];
}

void gb_cart_ram_writeb(struct gb *gb, uint16_t addr, uint8_t v) {
     struct gb_cart *cart = &gb->cart;
     unsigned ram_off;

     if (cart->ram_write_protected) {
          return;
     }

     switch (cart->model) {
     case GB_CART_SIMPLE:
          /* No RAM */
          return;
     case GB_CART_MBC1:
          if (cart->ram_banks == 0) {
               /* No RAM */
               return;
          }

          ram_off = gb_cart_mbc1_ram_off(gb, addr);
          break;
     case GB_CART_MBC2:
          ram_off = addr % 512;
          /* MBC2 only has 4 bits per address, so the high nibble is unusable */
          v |= 0xf0;
          break;
     default:
          /* Should not be reached */
          die();
     }

     cart->ram[ram_off] = v;
}
