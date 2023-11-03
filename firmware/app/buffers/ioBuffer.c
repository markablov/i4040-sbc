#include "ioBuffer.h"

uint8_t i4040IOBuffer[I4040_IO_BUFFER_LENGTH];

volatile uint16_t i4040IOBufferWritePtr = 0;