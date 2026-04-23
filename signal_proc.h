#ifndef SIGNAL_PROC_H
#define SIGNAL_PROC_H

#include <stdint.h>
#include <stdbool.h>

// --- TWEAKABLE DSP PARAMETERS ---
// Assuming a simulated sampling rate of 20 Hz for now
#define SAMPLE_RATE_HZ 20 

// 1-second window = 20 samples
#define WINDOW_SIZE_SAMPLES (1 * SAMPLE_RATE_HZ) 

// 2-second long window = 40 samples (for dual window logic)
#define LONG_WINDOW_SIZE_SAMPLES (2 * SAMPLE_RATE_HZ)

// 60ms step size = ~1 sample at 20Hz (We can increase this at higher sample rates)
#define STEP_SIZE_SAMPLES 1 

// Expose the public functions
void SignalProc_Init(void);
void SignalProc_ProcessSample(float emg_value);

#endif /* SIGNAL_PROC_H */