#include "externalInterface.h"
#include "cycleHandler.h"
#include "romEmulator.h"
#include "ramEmulator.h"

#include "commands.h"

void cmdStart(uint8_t *data, uint16_t dataLen) {
  sendExternalMessageSync("START command has been received, RAM dump size is %d bytes\r\n", dataLen);
  initROM(data, dataLen);
  clearRAM();
  runCycler();
}