#include "signal_proc.h"
#include "dispatcher.h"
#include <math.h> // For sqrtf()

// --- Private Variables ---
static float emg_window[WINDOW_SIZE_SAMPLES];
static uint16_t window_index = 0;
static uint16_t samples_since_last_calc = 0;
static float running_sum_of_squares = 0.0f;

static float long_emg_window[LONG_WINDOW_SIZE_SAMPLES];
static uint16_t long_window_index = 0;
static uint16_t long_window_count = 0;
static float running_sum_long_window = 0.0f;

// ---------------------------------------------------------
// Initialization
// ---------------------------------------------------------
void SignalProc_Init(void) {
  window_index = 0;
  samples_since_last_calc = 0;
  running_sum_of_squares = 0.0f;
  for (int i = 0; i < WINDOW_SIZE_SAMPLES; i++) {
    emg_window[i] = 0.0f;
  }

  long_window_index = 0;
  long_window_count = 0;
  running_sum_long_window = 0.0f;
  for (int i = 0; i < LONG_WINDOW_SIZE_SAMPLES; i++) {
    long_emg_window[i] = 0.0f;
  }
}

// ---------------------------------------------------------
// Process incoming float data
// ---------------------------------------------------------
void SignalProc_ProcessSample(float emg_value) {

  // 1. Add the new sample to the sliding window (overwriting the oldest)
  float old_val = emg_window[window_index];

  // Update the running sum of squares
  running_sum_of_squares -= (old_val * old_val);
  running_sum_of_squares += (emg_value * emg_value);

  // Guard against floating point drift
  if (running_sum_of_squares < 0.0f) {
    running_sum_of_squares = 0.0f;
  }

  emg_window[window_index] = emg_value;
  window_index = (window_index + 1) % WINDOW_SIZE_SAMPLES;
  samples_since_last_calc++;

  // 2. Check if it's time to calculate the RMS (based on Step Size)
  if (samples_since_last_calc >= STEP_SIZE_SAMPLES) {

    // Calculate the mean, then the square root using the running sum
    float mean_of_squares = running_sum_of_squares / WINDOW_SIZE_SAMPLES;
    float rms_result = sqrtf((double)mean_of_squares);

    // --- Dual Window Logic for Better Stability ---
    // Get the old RMS value that is about to be overwritten
    float old_rms = long_emg_window[long_window_index];

    // Update the long window running sum
    running_sum_long_window -= old_rms;
    running_sum_long_window += rms_result;

    // Guard against floating point drift
    if (running_sum_long_window < 0.0f) {
      running_sum_long_window = 0.0f;
    }

    // Add the short-window RMS result to the long window
    long_emg_window[long_window_index] = rms_result;
    long_window_index = (long_window_index + 1) % LONG_WINDOW_SIZE_SAMPLES;

    if (long_window_count < LONG_WINDOW_SIZE_SAMPLES) {
      long_window_count++;
    }

    // Calculate the moving average of the long window using the running sum
    float smoothed_rms = running_sum_long_window / long_window_count;

    // Package the smoothed RMS data and post it to the queue
    SystemEvent evt;
    evt.type = SYS_EVT_FEATURES_READY;
    evt.short_window_rms = rms_result;
    evt.long_window_rms = smoothed_rms;
    evt.float_payload = smoothed_rms; // Keep for logger compatibility
    evt.int_payload = 0;
    Dispatcher_PostEvent(evt);

    // Reset the step counter
    samples_since_last_calc = 0;
  }
}