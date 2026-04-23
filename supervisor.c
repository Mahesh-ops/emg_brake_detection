#include "supervisor.h"
#include "dispatcher.h"

// 2.0 second cooldown (2000 milliseconds)
#define RECOVERY_COOLDOWN_MS 2000 

static SystemState current_state = STATE_STARTUP_SAFE;
static uint16_t recovery_timer = 0;

// ---------------------------------------------------------
void Supervisor_Init(void) {
    current_state = STATE_STARTUP_SAFE;
    recovery_timer = 0;
}

// ---------------------------------------------------------
// Process signals from the Safety Manager
// ---------------------------------------------------------
void Supervisor_ProcessIntentEvent(uint8_t intent_signal) {
    
    switch (current_state) {
        
        case STATE_STARTUP_SAFE:
            // Once calibration is done and signal is clear, we go to Idle
            if (intent_signal == INTENT_CLEAR) {
                current_state = STATE_IDLE;
                
                // Tell Output Manager to turn off the Orange LED
                SystemEvent evt;
                evt.type = SYS_EVT_CALIB_DONE;
                evt.int_payload = 0;
                Dispatcher_PostEvent(evt);
            }
            break;

        case STATE_IDLE:
            if (intent_signal == INTENT_ACTIVE) {
                // DRIVER SLAMMED THE BRAKES!
                current_state = STATE_INTENT_CONFIRMED;
                
                SystemEvent cmd;
                cmd.type = SYS_EVT_CMD_BRAKE;
                cmd.int_payload = 1; // 1 = Assert Brakes (Red/Green LEDs)
                Dispatcher_PostEvent(cmd);
            }
            break;

        case STATE_INTENT_CONFIRMED:
            if (intent_signal == INTENT_CLEAR) {
                // Driver released the pedal. Start the cooldown.
                current_state = STATE_RECOVERY;
                recovery_timer = RECOVERY_COOLDOWN_MS;
            }
            break;

        case STATE_RECOVERY:
            if (intent_signal == INTENT_ACTIVE) {
                // Driver slammed the brakes again during the cooldown!
                current_state = STATE_INTENT_CONFIRMED;
            }
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------
// This needs to be called every 1 millisecond
// ---------------------------------------------------------
void Supervisor_Tick1ms(void) {
    if (current_state == STATE_RECOVERY) {
        if (recovery_timer > 0) {
            recovery_timer--;
        } else {
            // Cooldown complete! Safe to release the physical brakes.
            current_state = STATE_IDLE;
            
            SystemEvent cmd;
            cmd.type = SYS_EVT_CMD_BRAKE;
            cmd.int_payload = 0; // 0 = De-assert Brakes
            Dispatcher_PostEvent(cmd);
        }
    }
}