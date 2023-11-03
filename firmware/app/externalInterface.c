#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "usbd_cdc_if.h"

#include "commands.h"
#include "externalInterface.h"
#include "cmdBuffer.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

#define MAX_OUTPUT_MSG_LEN 1024

void sendFinishSignal() {
  uint8_t buf[3] = { (uint8_t)CmdFinish, 0, 0};
  while (CDC_Transmit_FS(buf, 3) != USBD_OK);

  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  while (hcdc->TxState != 0);
}

void sendCommandAck() {
  uint8_t buf[3] = { (uint8_t)CmdAck, 0, 0};
  while (CDC_Transmit_FS(buf, 3) != USBD_OK);
}

void sendExternalMessageEx(uint8_t sync, const char *format, ...) {
  va_list args;
  va_start(args, format);

  char buf[MAX_OUTPUT_MSG_LEN];

  int dataSize = vsnprintf(&buf[3], sizeof(buf) - 3, format, args);

  // command
  buf[0] = (char)CmdOutput;

  // data size
  buf[1] = (char)(dataSize & 0xFF);
  buf[2] = (char)((dataSize >> 8) & 0xFF);

  while (CDC_Transmit_FS((uint8_t *)buf, (uint16_t)(dataSize + 3)) != USBD_OK);

  if (sync) {
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
    while (hcdc->TxState != 0);
  }

  va_end(args);
}

/*
 *  INPUT PROCESSING
 */

enum InputStage { InputCommand, InputDataLengthLow, InputDataLengthHigh, InputData };

static enum InputStage inputStage = InputCommand;

static enum InCommandType cmdType;
static uint8_t * cmdData = NULL;
static uint16_t cmdDataLen = 0;
static uint16_t cmdDataPos = 0;

void writeNewCommandToBuffer() {
  InCommand * newCmd = &commandsBuffer[commandsBufferWritePtr];
  if (newCmd->data) {
    free(newCmd->data);
  }
  newCmd->data = cmdData;
  newCmd->type = cmdType;
  newCmd->size = cmdDataLen;
  commandsBufferWritePtr = (commandsBufferWritePtr + 1) % COMMANDS_BUFFER_LENGTH;
  inputStage = InputCommand;
}

void processInputData(uint8_t * data, uint16_t dataLen) {
  for (uint8_t * current = data, * end = data + dataLen; current < end; current++) {
    switch (inputStage) {
      case InputCommand:
        cmdType = *current;
        inputStage = InputDataLengthLow;
        break;

      case InputDataLengthLow:
        cmdDataLen = *current;
        inputStage = InputDataLengthHigh;
        break;

      case InputDataLengthHigh:
        cmdDataLen = cmdDataLen | (*current << 8);
        inputStage = InputData;
        if (cmdDataLen == 0) {
          cmdData = NULL;
          writeNewCommandToBuffer();
        } else {
          cmdDataPos = 0;
          cmdData = malloc((size_t)cmdDataLen);
        }
        break;

      case InputData:
        cmdData[cmdDataPos] = *current;
        cmdDataPos++;
        if (cmdDataPos == cmdDataLen) {
          writeNewCommandToBuffer();
        }
        break;
    }
  }
}
