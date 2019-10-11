#ifndef _SPU_H_
#define _SPU_H_

/* We don't want to generate SPU samples at 4.2MHz so we only generate a sample
 * every GB_SPU_SAMPLE_RATE_DIVISOR cycles */
#define GB_SPU_SAMPLE_RATE_DIVISOR      64

/* Effective sample rate for the frontend */
#define GB_SPU_SAMPLE_RATE_HZ (GB_CPU_FREQ_HZ / GB_SPU_SAMPLE_RATE_DIVISOR)

/* Number of sample frames bufferized. Each frame contains two samples for the
 * left and right stereo channels */
#define GB_SPU_SAMPLE_BUFFER_LENGTH 2048

/* Number of entries in the sample buffer ring */
#define GB_SPU_SAMPLE_BUFFER_COUNT 2

/* Sound 3 RAM size in bytes */
#define GB_NR3_RAM_SIZE  16

struct gb_spu_sample_buffer {
     /* Buffer of pairs of stereo samples */
     int16_t samples[GB_SPU_SAMPLE_BUFFER_LENGTH][2];
     /* Semaphore set to 1 when the frontend is done sending the audio buffer
      * and reset to 0 when the SPU starts filling it with new samples. */
     sem_t free;
     /* Semaphore set to 1 when the SPU is done filling a buffer and it can be
      * sent by the frontend. Set to 0 by the frontend when it starts sending
      * the samples. */
     sem_t ready;
};

/* Duration works the same for all 4 sounds but the max values are different */
#define GB_SPU_NR3_T1_MAX 0xff

struct gb_spu_duration {
     /* True if the duration counter is enabled */
     bool enable;
     /* Counter keeping track of how much time has passed */
     uint32_t counter;
};

struct gb_spu_divider {
     /* We advance to the next step every 0x800 - `offset` */
     uint16_t offset;
     /* Counter to the next step */
     uint16_t counter;
};

/* Data concerning sound 3: user-defined waveform */
struct gb_spu_nr3 {
     /* True if sound 3 is enabled */
     bool enable;
     /* True if sound 3 is currently running */
     bool running;
     /* Sound 3's length counter */
     struct gb_spu_duration duration;
     /* Raw register value for the length counter config since it's readable */
     uint8_t t1;
     /* Sound 3's frequency divider */
     struct gb_spu_divider divider;
     /* Volume shift (1: full volume, 2: half or 3: quarter) or 0 if the sound
      * is muted */
     uint8_t volume_shift;
     /* RAM of 32 4bit sound samples, two samples per byte */
     uint8_t ram[GB_NR3_RAM_SIZE];
     /* Current sample index */
     uint8_t index;
};

struct gb_spu {
     /* Master enable. When false all SPU circuits are disabled and the SPU
      * configuration is reset. It's not possible to configure the other SPU
      * registers except for sound 3's RAM */
     bool enable;

     /* Leftover cycles from the previous sync if it left a number of cycles
      * less than GB_SPU_SAMPLE_RATE_DIVISOR */
     uint8_t sample_period_frac;

     /* NR50 register */
     uint8_t output_level;
     /* NR51 register */
     uint8_t sound_mux;

     /* Amplification factor for each sound for both stereo channels */
     int16_t sound_amp[4][2];

     /* Sound 3 state */
     struct gb_spu_nr3 nr3;

     /* Audio buffer exchanged with the frontend */
     struct gb_spu_sample_buffer buffers[GB_SPU_SAMPLE_BUFFER_COUNT];
     /* Buffer being currently filled up with fresh samples */
     unsigned buffer_index;
     /* Position within the current buffer */
     unsigned sample_index;
};

void gb_spu_reset(struct gb *gb);
void gb_spu_sync(struct gb *gb);
void gb_spu_update_sound_amp(struct gb *gb);
void gb_spu_nr3_start(struct gb *gb);
void gb_spu_duration_reload(struct gb_spu_duration *d,
                            unsigned duration_max,
                            uint8_t t1);

#endif /* _SPU_H_ */
