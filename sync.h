#ifndef _GB_SYNC_H_
#define _GB_SYNC_H_

enum gb_sync_token {
     GB_SYNC_GPU = 0,

     GB_SYNC_NUM
};

struct gb_sync {
     int32_t last_sync[GB_SYNC_NUM];
};

void gb_sync_reset(struct gb *gb);

/* Resynchronize the given token and return the number of cycles since the last
 * synchronization */
int32_t gb_sync_resync(struct gb *gb, enum gb_sync_token token);

#endif /* _GB_SYNC_H_ */
