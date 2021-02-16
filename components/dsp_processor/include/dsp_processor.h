#ifndef _DSP_PROCESSOR_H_
#define _DSP_PROCESSOR_H_

enum dspFlows {
  dspfStereo,
  dspfBiamp,
  dspf2DOT1,
  dspfFunkyHonda,
  dspfBassBoost
};

size_t write_ringbuf(const uint8_t *data, size_t size);

void dsp_i2s_task_init(uint32_t sample_rate, bool slave);

void dsp_i2s_task_deinit(void);

enum filtertypes {
  LPF,
  HPF,
  BPF,
  BPF0DB,
  NOTCH,
  ALLPASS360,
  ALLPASS180,
  PEAKINGEQ,
  LOWSHELF,
  HIGHSHELF
};

// Process node
typedef struct ptype {
  int filtertype;
  float freq;
  float gain;
  float q;
  float *in, *out;
  float coeffs[5];
  float w[2];
} ptype_t;

// Process flow
typedef struct pnode {
  ptype_t process;
  struct pnode *next;
} pnode_t;

void dsp_setup_flow(double freq, uint32_t samplerate);
void dsp_set_xoverfreq(uint8_t, uint8_t, uint32_t);

#endif /* _DSP_PROCESSOR_H_  */
