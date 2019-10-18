#ifndef _GB_BOOTROM_H_
#define _GB_BOOTROM_H_

/* Expected length of the DMG boot ROM in bytes */
#define GB_BOOTROM_LEN_DMG  256
/* Expected length of the GBC boot ROM in bytes */
#define GB_BOOTROM_LEN_GBC  2304

struct gb_bootrom {
     /* True if the boot rom is currently mapped */
     bool active;
     /* Contents of the boot rom */
     uint8_t rom[GB_BOOTROM_LEN_GBC];
};

void gb_bootrom_load(struct gb *gb);

#endif /* _GB_BOOTROM_H_ */
