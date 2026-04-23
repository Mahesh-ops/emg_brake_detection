#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <stdint.h>
#include <stdbool.h>

// --- SYSTEM STATES ---
typedef enum {
    STATE_STARTUP_SAFE,
    STATE_IDLE,
    STATE_INTENT_CONFIRMED,
    STATE_RECOVERY,
    STATE_FAULT
} SystemState;

// --- INTENT SIGNALS ---
#define INTENT_CLEAR  0
#define INTENT_ACTIVE 1

void Supervisor_Init(void);
void Supervisor_ProcessIntentEvent(uint8_t intent_signal);
void Supervisor_Tick1ms(void); // For counting down the recovery timer

#endif /* SUPERVISOR_H */