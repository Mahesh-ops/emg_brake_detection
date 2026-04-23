#include "dispatcher.h"
#include "logger.h"
#include "output_mgr.h"


// Include the modules that need to RECEIVE events
#include "safety_manager.h"
#include "signal_proc.h"
#include "supervisor.h"


#define EVENT_QUEUE_SIZE 32

// --- Private Event Queue Variables ---
static SystemEvent event_queue[EVENT_QUEUE_SIZE];
static uint16_t head = 0;
static uint16_t tail = 0;

// ---------------------------------------------------------
void Dispatcher_Init(void) {
  head = 0;
  tail = 0;
}

// ---------------------------------------------------------
// Push an event into the queue
// ---------------------------------------------------------
bool Dispatcher_PostEvent(SystemEvent evt) {

  uint16_t next_head = (head + 1) % EVENT_QUEUE_SIZE;

  if (next_head != tail) {
    event_queue[head] = evt;
    head = next_head;
    return true;
  }
  // ERROR: Event Queue Overflow!
  return false;
}

typedef void (*EventHandler)(SystemEvent);

// --- Event Handler Functions ---
static void Handle_None(SystemEvent evt) { (void)evt; }
static void Handle_Tick1ms(SystemEvent evt) { (void)evt; }

static void Handle_SamplesParsed(SystemEvent evt) {
  SignalProc_ProcessSample(evt.float_payload);
}

static void Handle_FeaturesReady(SystemEvent evt) {
  SafetyManager_EvaluateRMS(evt.short_window_rms, evt.long_window_rms);
}

static void Handle_IntentState(SystemEvent evt) {
  Supervisor_ProcessIntentEvent(evt.int_payload);
  Logger_LogEvent(evt);
}

static void Handle_CmdBrake(SystemEvent evt) {
  OutputMgr_SetBrakes(evt.int_payload);
  Logger_LogEvent(evt);
}

static void Handle_CalibDone(SystemEvent evt) {
  OutputMgr_SetCalibrationLED(0);
  Logger_LogEvent(evt);
}

// --- Function Pointer Array ---
static const EventHandler event_handlers[] = {
    Handle_None,          // 0: SYS_EVT_NONE
    Handle_Tick1ms,       // 1: SYS_EVT_TICK_1MS
    Handle_SamplesParsed, // 2: SYS_EVT_SAMPLES_PARSED
    Handle_FeaturesReady, // 3: SYS_EVT_FEATURES_READY
    Handle_IntentState,   // 4: SYS_EVT_INTENT_STATE
    Handle_CmdBrake,      // 5: SYS_EVT_CMD_BRAKE
    Handle_CalibDone      // 6: SYS_EVT_CALIB_DONE
};

#define NUM_EVENTS (sizeof(event_handlers) / sizeof(event_handlers[0]))

// ---------------------------------------------------------
// Pull events from the queue and route them
// ---------------------------------------------------------
void Dispatcher_ProcessQueue(void) {
  while (tail != head) {
    // Pop the oldest event
    SystemEvent current_evt = event_queue[tail];
    tail = (tail + 1) % EVENT_QUEUE_SIZE;

    // --- THE ROUTER SWITCHBOARD (Array of Function Pointers) ---
    if (current_evt.type < NUM_EVENTS &&
        event_handlers[current_evt.type] != NULL) {
      event_handlers[current_evt.type](current_evt);
    }
  }
}