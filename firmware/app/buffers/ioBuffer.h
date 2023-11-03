#ifndef STM32_MCS4_MEMORY_EMULATOR_IO_BUFFER_H
#define STM32_MCS4_MEMORY_EMULATOR_IO_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define I4040_IO_BUFFER_LENGTH 4096

extern uint8_t i4040IOBuffer[I4040_IO_BUFFER_LENGTH];

// could be updated in ISR
extern volatile uint16_t i4040IOBufferWritePtr;

#ifdef __cplusplus
}
#endif

#endif //STM32_MCS4_MEMORY_EMULATOR_IO_BUFFER_H
