#include <stdlib.h>
#include <stdio.h>

#include "main.h"
#include "commands/commands.h"
#include "buffers/ioBuffer.h"
#include "buffers/cmdBuffer.h"
#include "externalInterface.h"
#include "cycleHandler.h"
#include "ramEmulator.h"

#include "loop.h"

#define MAX_IO_BYTES_TO_SEND_IN_SINGLE_TRANSMIT 100

static void sendFinalEmulatorState() {
  // send results from execution
  sendExternalMessageSync("Program has been finished\r\n");
  printCyclerStats();
  sendExternalMessageSync("Output from i4040 (%d nibbles): \r\n", i4040IOBufferWritePtr);
  for (uint16_t ptr = 0; ptr < i4040IOBufferWritePtr; ) {
    char bytesStr[MAX_IO_BYTES_TO_SEND_IN_SINGLE_TRANSMIT * 2 + 1];
    for (uint16_t currentLen = 0, chunkEnd = ptr + MAX_IO_BYTES_TO_SEND_IN_SINGLE_TRANSMIT; ptr < chunkEnd && ptr < i4040IOBufferWritePtr; ptr++) {
      currentLen += snprintf(bytesStr + currentLen, sizeof(bytesStr) - currentLen, "%X ", i4040IOBuffer[ptr]);
    }

    sendExternalMessageSync("  %s\r\n", bytesStr);
  }

  // inform that emulation has been finished
  sendFinishSignal();

  // reset state
  i4040IOBufferWritePtr = 0;
  emulatorState = StateStale;
}

static void processInputCommands() {
  if (commandsBufferReadPtr == commandsBufferWritePtr) {
    return;
  }

  for (; commandsBufferReadPtr < commandsBufferWritePtr; commandsBufferReadPtr = (commandsBufferReadPtr + 1) % COMMANDS_BUFFER_LENGTH) {
    sendCommandAck();

    InCommand * cmd = &commandsBuffer[commandsBufferReadPtr];
    switch (cmd->type) {
      case CmdStart:
        cmdStart(cmd->data, cmd->size);
        break;
    }

    if (cmd->data) {
      free(cmd->data);
      cmd->data = NULL;
    }
  }
}

void loopTick(void) {
  // allow optimizer to save some cycles, we need them!
  if (emulatorState == StateFinished || emulatorState == StateStale) {
    if (emulatorState == StateFinished) {
      sendFinalEmulatorState();
    } else {
      processInputCommands();
    }

    return;
  }

  while ((OUT_4040_PHI1_GPIO_Port->IDR & OUT_4040_PHI1_Pin) == OUT_4040_PHI1_Pin);

  OUT_TEST1_GPIO_Port->ODR ^= OUT_TEST1_Pin;
  handleCyclePhi1Falling();
  OUT_TEST1_GPIO_Port->ODR ^= OUT_TEST1_Pin;
}
