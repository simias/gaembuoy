#include <stdio.h>
#include "gb.h"

void gb_spu_update_sound_amp(struct gb *gb) {
     struct gb_spu *spu = &gb->spu;
     unsigned sound;
     /* The maximum value a sample can take while summing the raw values */
     unsigned max_amplitude;
     unsigned scaling;

     /* Each sound generates values 4bit unsigned values */
     max_amplitude = 15;
     /* Which can then be amplified up to 8 times by the `output_level` setting
      */
     max_amplitude *= 8;
     /* Finally we sum up to 4 sounds */
     max_amplitude *= 4;

     /* Linear scaling to saturate the output at max amplitude */
     scaling = 0x7fff / max_amplitude;

     for (sound = 0; sound < 4; sound++) {
          unsigned channel;

          for (channel = 0; channel < 2; channel++) {
               bool enabled = spu->sound_mux & (1 << (sound + channel * 4));
               int16_t amp;

               if (enabled) {
                    amp = 1;
                    amp += (spu->output_level >> (channel * 4)) & 7;
                    amp *= scaling;
               } else {
                    amp = 0;
               }

               spu->sound_amp[sound][channel] = amp;
          }
     }
}

static void gb_spu_frequency_reload(struct gb_spu_divider *f) {
     f->counter = 2 * (0x800U - f->offset);
}

static void gb_spu_lfsr_counter_reload(struct gb_spu_nr4 *nr4) {
     /* The LFSR clock has a divider and a shifter */
     uint8_t div = nr4->lfsr_config & 7;
     uint8_t shift = (nr4->lfsr_config >> 4) + 1;

     if (div == 0) {
          nr4->counter = 4;
     } else {
          nr4->counter = 8 * div;
     }

     nr4->counter <<= shift;
}

void gb_spu_sweep_reload(struct gb_spu_sweep *f, uint8_t conf) {
     f->shift = conf & 0x7;
     f->subtract = (conf >> 3) & 1;
     f->time = (conf >> 4) & 0x7;

     f->counter = 0x8000 * f->time;
}

void gb_spu_reset(struct gb *gb) {
     struct gb_spu *spu = &gb->spu;

     spu->enable = true;
     spu->output_level = 0;
     spu->sound_mux = 0;

     gb_spu_update_sound_amp(gb);

     /* NR1 reset */
     spu->nr1.running = false;
     spu->nr1.duration.enable = false;
     spu->nr1.wave.duty_cycle = 0;
     spu->nr1.envelope_config = 0;

     spu->nr1.sweep.divider.offset = 0;
     gb_spu_frequency_reload(&spu->nr1.sweep.divider);
     gb_spu_sweep_reload(&spu->nr1.sweep, 0);

     /* NR2 reset */
     spu->nr2.running = false;
     spu->nr2.duration.enable = false;
     spu->nr2.wave.duty_cycle = 0;
     spu->nr2.envelope_config = 0;

     spu->nr2.divider.offset = 0;
     gb_spu_frequency_reload(&spu->nr2.divider);

     /* NR3 reset */
     spu->nr3.enable = false;
     spu->nr3.running = false;
     spu->nr3.duration.enable = false;
     spu->nr3.volume_shift = 0;
     spu->nr3.divider.offset = 0;
     spu->nr3.t1 = 0;
     spu->nr3.index = 0;

     spu->nr3.divider.offset = 0;
     gb_spu_frequency_reload(&spu->nr3.divider);

     /* NR4 reset */
     spu->nr4.running = false;
     spu->nr4.duration.enable = false;
     spu->nr4.envelope_config = 0;
     spu->nr4.lfsr_config = 0;
     spu->nr4.lfsr = 0x7fff;
}

void gb_spu_duration_reload(struct gb_spu_duration *d,
                            unsigned duration_max,
                            uint8_t t1) {
     d->counter = (duration_max + 1 - t1) * 0x4000U;
}

/* Run the duration counter if it's enabled. Returns true if the counter reached
 * zero and the channel should be disabled. */
static bool gb_spu_duration_update(struct gb_spu_duration *d,
                                   unsigned duration_max,
                                   unsigned cycles) {
     bool elapsed = false;

     if (!d->enable) {
          return false;
     }

     while (cycles) {
          if (d->counter > cycles) {
               d->counter -= cycles;
               cycles = 0;
          } else {
               /* Counter reached 0 */
               elapsed = true;
               cycles -= d->counter;
               /* I'm not entirely sure about this but apparently when the
                * counter elapses it's reloaded with the max possible value
                * (maybe because it wraps around? */
               gb_spu_duration_reload(d, duration_max, 0);
          }
     }

     return elapsed;
}

/* Update the frequency counter and return the number of times it ran out */
static unsigned gb_spu_frequency_update(struct gb_spu_divider *f,
                                        unsigned cycles) {
     unsigned count = 0;

     while (cycles) {
          if (f->counter > cycles) {
               f->counter -= cycles;
               cycles = 0;
          } else {
               count++;
               cycles -= f->counter;
               /* Reload counter */
               gb_spu_frequency_reload(f);
          }
     }

     return count;
}

/* Update the sweep function and the frequency counter and return the number of
 * times it ran out */
static unsigned gb_spu_sweep_update(struct gb_spu_sweep *s,
                                    unsigned cycles,
                                    bool *disable) {
     unsigned count = 0;
     *disable = false;

     if (s->time == 0) {
          /* Sweep is disabled */
          return gb_spu_frequency_update(&s->divider, cycles);
     }

     /* We need to step the sweep function and the frequency function alongside
      * since the frequency changes with the sweep */
     while (cycles) {
          unsigned to_run = cycles;

          if (s->counter < to_run) {
               to_run = s->counter;
          }

          if (s->divider.counter < to_run) {
               to_run = s->divider.counter;
          }

          s->counter -= to_run;
          if (s->counter == 0) {
               /* Sweep step elapsed */
               uint16_t delta = s->divider.offset >> s->shift;

               if (s->subtract) {
                    /* If we're subtracting and the shift value is zero or it
                     * would overflow we do nothing and the divider offset is
                     * not changed */
                    if (s->shift != 0 && delta <= s->divider.offset) {
                         s->divider.offset -= delta;
                    }
               } else {
                    uint32_t o = s->divider.offset;

                    o += delta;

                    if (o > 0x7ff) {
                         /* If the addition overflows the sound is disabled */
                         *disable = true;
                         break;
                    }

                    s->divider.offset = o;
               }

               /* Reload counter */
               s->counter = 0x8000 * s->time;
          }

          count += gb_spu_frequency_update(&s->divider, to_run);
          cycles -= to_run;
     }

     return count;
}

#define GB_SPU_NPHASES 16
static uint8_t gb_spu_next_wave_sample(struct gb_spu_rectangle_wave *wave,
                                       unsigned phase_steps) {
     static const uint8_t waveforms[4][GB_SPU_NPHASES / 2] = {
          /* 1/8 */
          { 1, 0, 0, 0, 0, 0, 0, 0},
          /* 1/4 */
          { 1, 1, 0, 0, 0, 0, 0, 0},
          /* 1/2 */
          { 1, 1, 1, 1, 0, 0, 0, 0},
          /* 3/4 */
          { 1, 1, 1, 1, 1, 1, 0, 0},
     };

     wave->phase = (wave->phase + phase_steps) % GB_SPU_NPHASES;

     return waveforms[wave->duty_cycle][wave->phase / 2];
}

static void gb_spu_envelope_reload_counter(struct gb_spu_envelope *e) {
     e->counter = e->step_duration * 0x10000;
}

/* Reload the envelope config from the register value */
static void gb_spu_envelope_init(struct gb_spu_envelope *e, uint8_t config) {
     e->value = config >> 4;
     e->increment = (config & 8);
     e->step_duration = config & 7;

     gb_spu_envelope_reload_counter(e);
}

static bool gb_spu_envelope_active(struct gb_spu_envelope *e) {
     /* The envelope is stopped if the value is 0 and we're set to decrement */
     return e->value != 0 || e->increment;
}

/* Run the envelope if it's enabled. Returns true if the envelope reached an
 * inactive state and the channel should be disabled. */
static bool gb_spu_envelope_update(struct gb_spu_envelope *e, unsigned cycles) {
     if (e->step_duration != 0) {
          while (cycles) {
               if (e->counter > cycles) {
                    e->counter -= cycles;
                    cycles = 0;
               } else {
                    /* Step counter elapsed, apply envelope function */
                    cycles -= e->counter;

                    if (e->increment) {
                         if (e->value < 0xf) {
                              e->value++;
                         }
                    } else {
                         if (e->value > 0) {
                              e->value--;
                         }
                    }

                    gb_spu_envelope_reload_counter(e);
               }
          }
     }

     return !gb_spu_envelope_active(e);
}

static uint8_t gb_spu_next_nr1_sample(struct gb *gb, unsigned cycles) {
     struct gb_spu *spu = &gb->spu;
     uint8_t sample;
     unsigned sound_cycles;
     bool disable;

     /* The duration counter runs even if the sound itself is not running */
     if (gb_spu_duration_update(&spu->nr1.duration,
                                GB_SPU_NR1_T1_MAX,
                                cycles)) {
          spu->nr1.running = false;
     }

     if (!spu->nr1.running) {
          return 0;
     }

     if (gb_spu_envelope_update(&spu->nr1.envelope, cycles)) {
          spu->nr1.running = false;
     }

     if (!spu->nr1.running) {
          return 0;
     }

     sound_cycles = gb_spu_sweep_update(&spu->nr1.sweep, cycles, &disable);
     if (disable) {
          spu->nr1.running = false;
          return 0;
     }

     sample = gb_spu_next_wave_sample(&spu->nr1.wave, sound_cycles);

     sample *= spu->nr1.envelope.value;

     return sample;
}

static uint8_t gb_spu_next_nr2_sample(struct gb *gb, unsigned cycles) {
     struct gb_spu *spu = &gb->spu;
     uint8_t sample;
     unsigned sound_cycles;

     /* The duration counter runs even if the sound itself is not running */
     if (gb_spu_duration_update(&spu->nr2.duration,
                                GB_SPU_NR2_T1_MAX,
                                cycles)) {
          spu->nr2.running = false;
     }

     if (!spu->nr2.running) {
          return 0;
     }

     if (gb_spu_envelope_update(&spu->nr2.envelope, cycles)) {
          spu->nr2.running = false;
     }

     if (!spu->nr2.running) {
          return 0;
     }

     sound_cycles = gb_spu_frequency_update(&spu->nr2.divider, cycles);

     sample = gb_spu_next_wave_sample(&spu->nr2.wave, sound_cycles);

     sample *= spu->nr2.envelope.value;

     return sample;
}

static uint8_t gb_spu_next_nr3_sample(struct gb *gb, unsigned cycles) {
     struct gb_spu *spu = &gb->spu;
     uint8_t sample;
     unsigned sound_cycles;

     /* The duration counter runs even if the sound itself is not running */
     if (gb_spu_duration_update(&spu->nr3.duration,
                                GB_SPU_NR3_T1_MAX,
                                cycles)) {
          spu->nr3.running = false;
     }

     if (!spu->nr3.running) {
          return 0;
     }

     sound_cycles = gb_spu_frequency_update(&spu->nr3.divider, cycles);

     spu->nr3.index = (spu->nr3.index + sound_cycles) % (GB_NR3_RAM_SIZE * 2);

     if (spu->nr3.volume_shift == 0) {
          /* Sound is muted */
          return 0;
     }

     /* We pack two samples per byte */
     sample = spu->nr3.ram[spu->nr3.index / 2];

     if (spu->nr3.index & 1) {
          sample &= 0xf;
     } else {
          sample >>= 4;
     }

     return sample >> (spu->nr3.volume_shift - 1);
}

static void gb_spu_lfsr_step(struct gb_spu_nr4 *nr4) {
     /* If true the lfsr only uses 7 bits for the effective register period */
     bool period_7bits = nr4->lfsr_config & 0x8;
     uint16_t shifted;
     uint16_t carry;

     shifted = nr4->lfsr >> 1;
     carry = (nr4->lfsr ^ shifted) & 1;

     nr4->lfsr = shifted;
     nr4->lfsr |= carry << 14;

     if (period_7bits) {
          /* Carry is also copied to bit 6 */
          nr4->lfsr &= ~(1U << 6);
          nr4->lfsr |= carry << 6;
     }
}

static uint8_t gb_spu_next_nr4_sample(struct gb *gb, unsigned cycles) {
     struct gb_spu *spu = &gb->spu;
     uint8_t sample;

     /* The duration counter runs even if the sound itself is not running */
     if (gb_spu_duration_update(&spu->nr4.duration,
                                GB_SPU_NR4_T1_MAX,
                                cycles)) {
          spu->nr4.running = false;
     }

     if (!spu->nr4.running) {
          return 0;
     }

     if (gb_spu_envelope_update(&spu->nr4.envelope, cycles)) {
          spu->nr4.running = false;
     }

     if (!spu->nr4.running) {
          return 0;
     }

     while (cycles) {
          if (spu->nr4.counter > cycles) {
               spu->nr4.counter -= cycles;
               cycles = 0;
          } else {
               cycles -= spu->nr4.counter;
               gb_spu_lfsr_counter_reload(&spu->nr4);
               gb_spu_lfsr_step(&spu->nr4);
          }
     }

     /* Sample is 0 if the LFSR's LSB is 0, otherwise it's the envelope's value
      */
     sample = spu->nr4.lfsr & 1;
     sample *= spu->nr4.envelope.value;

     return sample;
}

/* Send a pair of left/right samples to the frontend */
static void gb_spu_send_sample_to_frontend(struct gb *gb,
                                           int16_t sample_l, int16_t sample_r) {
     struct gb_spu *spu = &gb->spu;
     struct gb_spu_sample_buffer *buf;

     buf = &spu->buffers[spu->buffer_index];

     if (spu->sample_index == 0) {
          /* We're about to fill the first sample, make sure that the
           * buffer is free. If it's not this will pause the thread until
           * the frontend frees it, effectively synchronizing us with audio
           */
          sem_wait(&buf->free);
     }

     buf->samples[spu->sample_index][0] = sample_l;
     buf->samples[spu->sample_index][1] = sample_r;

     spu->sample_index++;
     if (spu->sample_index == GB_SPU_SAMPLE_BUFFER_LENGTH) {
          /* We're done with this buffer */
          sem_post(&buf->ready);
          /* Move on to the next one */
          spu->buffer_index = (spu->buffer_index + 1)
               % GB_SPU_SAMPLE_BUFFER_COUNT;
          spu->sample_index = 0;
     }
}

void gb_spu_sync(struct gb *gb) {
     struct gb_spu *spu = &gb->spu;
     int32_t elapsed = gb_sync_resync(gb, GB_SYNC_SPU);
     int32_t frac;
     int32_t nsamples;
     int32_t next_sync;

     frac = spu->sample_period_frac;
     elapsed += frac;

     nsamples = elapsed / GB_SPU_SAMPLE_RATE_DIVISOR;

     while (nsamples--) {
          int32_t next_sample_delay = GB_SPU_SAMPLE_RATE_DIVISOR - frac;
          unsigned sound;
          int16_t sound_samples[4];
          int16_t sample_l = 0;
          int16_t sample_r = 0;

          sound_samples[0] = gb_spu_next_nr1_sample(gb, next_sample_delay);
          sound_samples[1] = gb_spu_next_nr2_sample(gb, next_sample_delay);
          sound_samples[2] = gb_spu_next_nr3_sample(gb, next_sample_delay);
          sound_samples[3] = gb_spu_next_nr4_sample(gb, next_sample_delay);

          for (sound = 0; sound < 4; sound++) {
               sample_l += sound_samples[sound] * spu->sound_amp[sound][0];
               sample_r += sound_samples[sound] * spu->sound_amp[sound][1];
          }

          gb_spu_send_sample_to_frontend(gb, sample_l, sample_r);

          frac = 0;
     }

     /* See if we have any leftover fractional sample */
     frac = elapsed % GB_SPU_SAMPLE_RATE_DIVISOR;

     /* Advance the SPU state even if we don't want the sample yet in order to
      * have the correct value for the `running` flags */
     gb_spu_next_nr1_sample(gb, frac);
     gb_spu_next_nr2_sample(gb, frac);
     gb_spu_next_nr3_sample(gb, frac);
     gb_spu_next_nr4_sample(gb, frac);

     spu->sample_period_frac = frac;

     /* Schedule a sync to fill the current buffer */
     next_sync = (GB_SPU_SAMPLE_BUFFER_LENGTH - spu->sample_index) *
          GB_SPU_SAMPLE_RATE_DIVISOR;
     next_sync -= frac;
     gb_sync_next(gb, GB_SYNC_SPU, next_sync);
}

void gb_spu_nr1_start(struct gb *gb) {
     struct gb_spu *spu = &gb->spu;

     spu->nr1.wave.phase = 0;
     gb_spu_frequency_reload(&spu->nr1.sweep.divider);
     gb_spu_envelope_init(&spu->nr1.envelope, spu->nr1.envelope_config);

     spu->nr1.running = gb_spu_envelope_active(&spu->nr1.envelope);
}

void gb_spu_nr2_start(struct gb *gb) {
     struct gb_spu *spu = &gb->spu;

     spu->nr2.wave.phase = 0;
     gb_spu_frequency_reload(&spu->nr2.divider);
     gb_spu_envelope_init(&spu->nr2.envelope, spu->nr2.envelope_config);

     spu->nr2.running = gb_spu_envelope_active(&spu->nr2.envelope);
}

void gb_spu_nr3_start(struct gb *gb) {
     struct gb_spu *spu = &gb->spu;

     if (!spu->nr3.enable) {
          /* We can't start if we're not enabled */
          return;
     }

     spu->nr3.index = 0;
     spu->nr3.running = true;
     gb_spu_frequency_reload(&spu->nr3.divider);
}

void gb_spu_nr4_start(struct gb *gb) {
     struct gb_spu *spu = &gb->spu;

     gb_spu_envelope_init(&spu->nr4.envelope, spu->nr4.envelope_config);
     gb_spu_lfsr_counter_reload(&spu->nr4);
     spu->nr4.running = true;
}
