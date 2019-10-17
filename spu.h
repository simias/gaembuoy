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
#define GB_SPU_NR1_T1_MAX 0x3f
#define GB_SPU_NR2_T1_MAX 0x3f
#define GB_SPU_NR3_T1_MAX 0xff
#define GB_SPU_NR4_T1_MAX 0x3f

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

struct gb_spu_sweep {
     /* Frequency divider */
     struct gb_spu_divider divider;
     /* Frequency sweep amount */
     uint8_t shift;
     /* True if we subtract the offset, otherwise we add it */
     bool subtract;
     /* Delay between sweep steps in 1/128th of a second (or 0 if disabled) */
     uint8_t time;
     /* Counter to the next sweep step */
     uint32_t counter;
};

struct gb_spu_rectangle_wave {
     /* Current phase within the duty cycle */
     uint8_t phase;
     /* Duty cycle: 1/8, 1/4, 1/2, 3/4 */
     uint8_t duty_cycle;
};

struct gb_spu_envelope {
     /* Duration of each addition/subtraction step in multiples of 65536 steps
      * (1/64 s). A value of 0 means that the envelope is stopped. */
     uint8_t step_duration;
     /* Current value of the envelope */
     uint8_t value;
     /* True if we increment `value` at every step, false if decrement it */
     bool increment;
     /* Step counter */
     uint32_t counter;
};

/* Data concerning sound 1: rectangular wave with envelope and frequency sweep
 */
struct gb_spu_nr1 {
     /* True if sound 1 is currently running */
     bool running;
     /* Sound 1's length counter */
     struct gb_spu_duration duration;
     /* Sound 1's frequency divider and sweep function */
     struct gb_spu_sweep sweep;
     /* Sound 1's rectangular wave */
     struct gb_spu_rectangle_wave wave;
     /* Envelope register configuration */
     uint8_t envelope_config;
     /* Active envelope config */
     struct gb_spu_envelope envelope;
};

/* Data concerning sound 2: rectangular wave with envelope */
struct gb_spu_nr2 {
     /* True if sound 2 is currently running */
     bool running;
     /* Sound 2's length counter */
     struct gb_spu_duration duration;
     /* Sound 2's frequency divider */
     struct gb_spu_divider divider;
     /* Sound 2's rectangular wave */
     struct gb_spu_rectangle_wave wave;
     /* Envelope register configuration */
     uint8_t envelope_config;
     /* Active envelope config */
     struct gb_spu_envelope envelope;
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

/* Data concerning sound 4: LFSR noise generation with envelope */
struct gb_spu_nr4 {
     /* True if sound 4 is currently running */
     bool running;
     /* Sound 4's length counter */
     struct gb_spu_duration duration;
     /* Envelope register configuration */
     uint8_t envelope_config;
     /* Active envelope config */
     struct gb_spu_envelope envelope;
     /* Linear Feedback Shift Register */
     uint16_t lfsr;
     /* LFSR configuration register (NR43) */
     uint8_t lfsr_config;
     /* Counter to the next LFSR shift */
     uint32_t counter;
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

     /* Sound 1 state */
     struct gb_spu_nr1 nr1;
     /* Sound 2 state */
     struct gb_spu_nr2 nr2;
     /* Sound 3 state */
     struct gb_spu_nr3 nr3;
     /* Sound 4 state */
     struct gb_spu_nr4 nr4;

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
void gb_spu_nr1_start(struct gb *gb);
void gb_spu_nr2_start(struct gb *gb);
void gb_spu_nr3_start(struct gb *gb);
void gb_spu_nr4_start(struct gb *gb);
void gb_spu_duration_reload(struct gb_spu_duration *d,
                            unsigned duration_max,
                            uint8_t t1);
void gb_spu_sweep_reload(struct gb_spu_sweep *f, uint8_t conf);

#endif /* _SPU_H_ */
