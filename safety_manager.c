#include "safety_manager.h"
#include "dispatcher.h"
#include "supervisor.h"

// --- Private State Variables ---
static float baseline_rms = 0.0f;
static uint16_t calibration_count = 0;
static uint8_t consecutive_spikes = 0;
static bool is_calibrated = false;

// ---------------------------------------------------------
// Initialization
// ---------------------------------------------------------
void SafetyManager_Init(void) {
  baseline_rms = 0.0f;
  calibration_count = 0;
  consecutive_spikes = 0;
  is_calibrated = false;
}

// ---------------------------------------------------------
// Evaluate incoming RMS feature against the baseline
// ---------------------------------------------------------
void SafetyManager_EvaluateRMS(float short_rms, float long_rms) {

  // 1. CALIBRATION PHASE: Learn the driver's resting noise
  if (!is_calibrated) {
    baseline_rms += long_rms; // Use the more stable long window for baseline
    calibration_count++;

    // Once we have enough samples, compute the average
    if (calibration_count >= CALIBRATION_WINDOWS) {
      baseline_rms /= CALIBRATION_WINDOWS;
      is_calibrated = true;
    }
    return; // Exit early; don't evaluate braking until calibrated
  }

  // 2. EVALUATION PHASE: Calculate current dynamic threshold
  float dynamic_threshold = baseline_rms * RMS_THRESHOLD_MULTIPLIER;

  // 3. INTENT DETECTION
  // Intent is active if BOTH short and long windows exceed the threshold
  if (short_rms > dynamic_threshold && long_rms > dynamic_threshold) {
    consecutive_spikes++;

    if (consecutive_spikes >= REQUIRED_CONSECUTIVE_SPIKES) {
      // *** INTENT CONFIRMED! ***
      // Dispatch intent ACTIVE to Supervisor
      SystemEvent evt;
      evt.type = SYS_EVT_INTENT_STATE;
      evt.int_payload = INTENT_ACTIVE;
      Dispatcher_PostEvent(evt);

      // Cap the counter so it doesn't overflow during a long brake
      consecutive_spikes = REQUIRED_CONSECUTIVE_SPIKES;
    }
  } else {
    // Signal dropped below threshold, reset the consecutive counter
    consecutive_spikes = 0;

    // Dispatch intent CLEAR to Supervisor
    SystemEvent evt;
    evt.type = SYS_EVT_INTENT_STATE;
    evt.int_payload = INTENT_CLEAR;
    Dispatcher_PostEvent(evt);
  }
}