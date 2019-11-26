#ifndef _GB_CART_H_
#define _GB_CART_H_

enum gb_cart_model {
     /* No mapper: 2 ROM banks, no RAM */
     GB_CART_SIMPLE = 0,
     /* MBC1 mapper, up to 64 ROM banks, 4 RAM banks */
     GB_CART_MBC1,
     /* MBC2 mapper, up to 16 ROM banks, one single 512 * 4bit RAM */
     GB_CART_MBC2,
     /* MBC3 mapper, up to 128 ROM banks, 4 RAM banks, optional RTC */
     GB_CART_MBC3,
     /* MBC5 mapper, up to 512 ROM banks, 16 RAM banks */
     GB_CART_MBC5,
};

struct gb_cart {
     /* Full ROM contents */
     uint8_t *rom;
     /* ROM length in bytes */
     unsigned rom_length;
     /* Number of ROM banks (each bank is 16KB) */
     unsigned rom_banks;
     /* Currently selected ROM bank */
     unsigned cur_rom_bank;
     /* Full cartrige ram contents */
     uint8_t *ram;
     /* RAM length in bytes */
     unsigned ram_length;
     /* Number of RAM banks (each bank is 8KB) */
     unsigned ram_banks;
     /* Currently selected RAM bank*/
     unsigned cur_ram_bank;
     /* True if RAM is write-protected (read-only) */
     bool ram_write_protected;
     /* Type of cartridge */
     enum gb_cart_model model;
     /* False if the MBC1 cartridge operates in 128 ROM banks/1 RAM bank
      * configuration otherwise it operates in 32 ROM banks/4 RAM banks
      * configuration. */
     bool mbc1_bank_ram;
     /* If we have a battery backup we save and restore the contents of the RAM
      * from this file */
     char *save_file;
     /* Dirty flag, set to true when the RAM has been written to */
     bool dirty_ram;
     /* True if the cartrige has a Real Time Clock */
     bool has_rtc;
     /* RTC state (if the cart has one) */
     struct gb_rtc rtc;
};

void gb_cart_load(struct gb *gb, const char *rom_path);
void gb_cart_unload(struct gb *gb);
void gb_cart_sync(struct gb *gb);
uint8_t gb_cart_rom_readb(struct gb *gb, uint16_t addr);
void gb_cart_rom_writeb(struct gb *gb, uint16_t addr, uint8_t v);
uint8_t gb_cart_ram_readb(struct gb *gb, uint16_t addr);
void gb_cart_ram_writeb(struct gb *gb, uint16_t addr, uint8_t v);

#endif /* _GB_CART_H_ */
