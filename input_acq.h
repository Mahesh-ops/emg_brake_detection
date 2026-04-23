#ifndef INPUT_ACQ_H
#define INPUT_ACQ_H

#include <stdint.h>
#include <stdbool.h>

// Ring Buffer Configuration
#define RX_BUFFER_SIZE 256

// Expose the initialization and update functions
void InputAcq_Init(void);
void InputAcq_Update(void);

// Expose the function that the UART ISR will call to push data
void InputAcq_PushChar(uint8_t data);

#endif /* INPUT_ACQ_H */