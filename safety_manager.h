#ifndef SAFETY_MANAGER_H
#define SAFETY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// --- SAFETY PARAMETERS ---
// Based on Ju et al. 2021: Threshold = Baseline * 2.5
#define RMS_THRESHOLD_MULTIPLIER 2.5f

// Require sustained contraction across multiple windows to filter out noise
#define REQUIRED_CONSECUTIVE_SPIKES 3

// How many initial windows to average to find the resting baseline
#define CALIBRATION_WINDOWS 10

void SafetyManager_Init(void);
void SafetyManager_EvaluateRMS(float short_rms, float long_rms);

#endif /* SAFETY_MANAGER_H */