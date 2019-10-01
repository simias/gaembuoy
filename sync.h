#ifndef _GB_SYNC_H_
#define _GB_SYNC_H_

enum gb_sync_token {
     GB_SYNC_GPU = 0,

     GB_SYNC_NUM
};

struct gb_sync {
     /* Smallest value in next_event */
     int32_t first_event;
     /* Value of the timestamp the last time this token was synchronized */
     int32_t last_sync[GB_SYNC_NUM];
     /* Value of the timestamp the next time this token must be synchronized */
     int32_t next_event[GB_SYNC_NUM];
};

void gb_sync_reset(struct gb *gb);

/* Resynchronize the given token and return the number of cycles since the last
 * synchronization */
int32_t gb_sync_resync(struct gb *gb, enum gb_sync_token token);
void gb_sync_next(struct gb *gb, enum gb_sync_token token, int32_t cycles);
void gb_sync_check_events(struct gb *gb);

#endif /* _GB_SYNC_H_ */