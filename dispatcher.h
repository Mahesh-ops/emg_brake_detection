#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdint.h>
#include <stdbool.h>

// --- MASTER EVENT LIST ---
typedef enum {
    SYS_EVT_NONE = 0,
    SYS_EVT_TICK_1MS,         // Hardware timer tick
    SYS_EVT_SAMPLES_PARSED,   // Input Acq has a new raw float
    SYS_EVT_FEATURES_READY,   // Signal Proc has a new RMS float
    SYS_EVT_INTENT_STATE,     // Safety Manager changed the brake state
    SYS_EVT_CMD_BRAKE,         // Output Manager needs to assert pin
		SYS_EVT_CALIB_DONE
} EventType;

// --- EVENT PACKAGE STRUCTURE ---
typedef struct {
    EventType type;
    float float_payload;    // Used for EMG values
    float short_window_rms; // Short window RMS
    float long_window_rms;  // Long window RMS
    uint8_t int_payload;    // Used for states, commands, or errors
} SystemEvent;

// --- PUBLIC FUNCTIONS ---
void Dispatcher_Init(void);
bool Dispatcher_PostEvent(SystemEvent evt);
void Dispatcher_ProcessQueue(void);

#endif /* DISPATCHER_H */