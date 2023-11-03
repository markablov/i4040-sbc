#ifndef STM32_MCS4_MEMORY_EMULATOR_CMD_BUFFER_H
#define STM32_MCS4_MEMORY_EMULATOR_CMD_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "commands.h"

#define COMMANDS_BUFFER_LENGTH 5

typedef struct {
  enum InCommandType type;
  uint8_t * data;
  uint16_t size;
} InCommand;

extern InCommand commandsBuffer[COMMANDS_BUFFER_LENGTH];

// could be updated in ISR
extern volatile uint8_t commandsBufferWritePtr;
extern volatile uint8_t commandsBufferReadPtr;

#ifdef __cplusplus
}
#endif

#endif //STM32_MCS4_MEMORY_EMULATOR_CMD_BUFFER_H
