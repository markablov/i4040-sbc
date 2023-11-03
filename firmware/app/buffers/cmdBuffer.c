#include "cmdBuffer.h"

InCommand commandsBuffer[COMMANDS_BUFFER_LENGTH];

volatile uint8_t commandsBufferWritePtr = 0;
volatile uint8_t commandsBufferReadPtr = 0;