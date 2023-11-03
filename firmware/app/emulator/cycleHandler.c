#include <stdint.h>
#include <stdlib.h>

#include "main.h"
#include "externalInterface.h"
#include "i4004Interface.h"
#include "romEmulator.h"
#include "ramEmulator.h"
#include "../buffers/ioBuffer.h"

#include "cycleHandler.h"

#define DELAY_IN_CYCLES_AFTER_INTERRUPTS_CONTROL  10000

typedef enum { StageA1, StageA2, StageA3, StageM1, StageM2, StageX1, StageX2, StageX3 } CycleStage;

volatile CyclerState emulatorState = StateStale;

static volatile uint64_t instructionCount = 0;

static volatile CycleStage currentStage = StageX3;

static volatile uint16_t currentAddress = 0;
static volatile uint8_t currentROMByte = 0;

static volatile uint8_t srcInstructionIsExecuting = 0;
static volatile uint8_t srcInstructionRegNo = 0;
static volatile uint8_t ioInstructionIsExecuting = 0;

/*
 * Print some statistics and logs about current run of i4004
 */
void printCyclerStats() {
  sendExternalMessageSync("Instruction cycles processed = %08X%08X\r\n", (uint32_t)(instructionCount >> 32U), (uint32_t)(instructionCount & 0xFFFFFFFF));
}

/*
 * Just marks cycler that we want to run it (need to set RESET signal at determined stage)
 */
void runCycler() {
  __disable_irq();

  emulatorState = StateResetting;
}

/*
 * Stop i4004 (via RESET pin)
 */
void stopCycler() {
  HAL_GPIO_WritePin(OUT_4040_RESET_GPIO_Port, OUT_4040_RESET_Pin, GPIO_PIN_RESET);
  emulatorState = StateFinished;

  __enable_irq();
}

/*
 * Actually run i4004 (by setting RESET pin)
 */
static void startCycler() {
  HAL_GPIO_WritePin(OUT_4040_RESET_GPIO_Port, OUT_4040_RESET_Pin, GPIO_PIN_SET);
  instructionCount = 0;
  emulatorState = StateRunning;
}

// odd numbers (when 1st bit of CM-RAM is set) all maps to bank #0
static uint8_t cmramToBankNumberMap[16] = {
  0, 0, 1, 0, 2, 0, 4, 0,
  3, 0, 5, 0, 6, 0, 7, 0
};

/*
 * Emulate execution of read (read data by CPU from RAM) RAM/IO instructions (done by 4001/4002 in real systems)
 */
static void executeRAMReadInstruction() {
  switch (currentROMByte) {
    // RDM / SBM / ADM
    case 0xE8:
    case 0xE9:
    case 0xEB:
      i4004_writeDataBus(*selectedCharacter);
      break;
    // RD0
    case 0xEC:
      i4004_writeDataBus(selectedStatusCharacters[0]);
      break;
    // RD1
    case 0xED:
      i4004_writeDataBus(selectedStatusCharacters[1]);
      break;
    // RD2
    case 0xEE:
      i4004_writeDataBus(selectedStatusCharacters[2]);
      break;
    // RD3
    case 0xEF:
      i4004_writeDataBus(selectedStatusCharacters[3]);
      break;
    default:
      break;
  }
}

/*
 * Emulate execution of write (write data by CPU to RAM) RAM/IO instructions (done by 4001/4002 in real systems)
 */
static void executeRAMWriteInstruction() {
  uint8_t data = i4004_readDataBus();

  switch (currentROMByte) {
      // WRM
    case 0xE0:
      *selectedCharacter = data;
      break;
      // WR0
    case 0xE4:
      selectedStatusCharacters[0] = data;
      break;
      // WR1
    case 0xE5:
      selectedStatusCharacters[1] = data;
      break;
      // WR2
    case 0xE6:
      selectedStatusCharacters[2] = data;
      break;
      // WR3
    case 0xE7:
      selectedStatusCharacters[3] = data;
      break;
      // WMP
    case 0xE1:
      i4040IOBuffer[i4040IOBufferWritePtr++] = data;
      break;

    default:
      break;
  }
}

// actually send data to bus at the end of current stage, because i40xx need it very early at next stage, while interrupt could have some delay
static void waitEndOfCycle() {
  while ((OUT_4040_PHI2_GPIO_Port->IDR & OUT_4040_PHI2_Pin) == OUT_4040_PHI2_Pin);
  while ((OUT_4040_PHI2_GPIO_Port->IDR & OUT_4040_PHI2_Pin) == 0);
}

/*
 * First phase when we want to proceed some actions (read data pins / emulate instruction execution / ...)
 */
void handleCyclePhi1Falling() {
  if (i4004_readSync()) {
    instructionCount++;
    // current stage is X3
    if (srcInstructionIsExecuting) {
      uint8_t charNo = i4004_readDataBus();
      volatile RAMRegister * selectedRegister = &selectedBank->registers[srcInstructionRegNo];
      selectedCharacter = &selectedRegister->mainCharacters[charNo];
      selectedStatusCharacters = selectedRegister->statusCharacters;
      selectedBank->selectedCharacter = selectedCharacter;
      selectedBank->selectedStatusCharacters = selectedStatusCharacters;
    }

    i4004_freeDataBus();
    currentStage = StageA1;
    return;
  }

  switch (currentStage) {
    case StageA1:
      currentStage = StageA2;

      // for stability need to read data bus on phi2 low stage
      while ((OUT_4040_PHI2_GPIO_Port->IDR & OUT_4040_PHI2_Pin) == OUT_4040_PHI2_Pin);
      currentAddress = i4004_readDataBus();
      break;
    case StageA2:
      currentStage = StageA3;

      // for stability need to read data bus on phi2 low stage
      while ((OUT_4040_PHI2_GPIO_Port->IDR & OUT_4040_PHI2_Pin) == OUT_4040_PHI2_Pin);
      currentAddress = currentAddress | (uint16_t) (i4004_readDataBus() << 4U);
      break;
    case StageA3: {
      uint8_t cmrom = i4004_readCMROM();
      if (cmrom == 0x2) {
        // if 2nd bank is selected, then we just need to adjust address
        currentAddress = currentAddress + 0x1000;
      }

      currentStage = StageM1;

      while ((OUT_4040_PHI2_GPIO_Port->IDR & OUT_4040_PHI2_Pin) == OUT_4040_PHI2_Pin);

      currentAddress = currentAddress | (uint16_t) (i4004_readDataBus() << 8U);
      currentROMByte = readROM(currentAddress);

      // don't write anything to data bus if CPU is in resetting state
      if (emulatorState != StateResetting) {
        uint8_t nibbleToSend = currentROMByte >> 4U;
        while ((OUT_4040_PHI2_GPIO_Port->IDR & OUT_4040_PHI2_Pin) == 0);
        // should be as less instruction as possible to output data into bus in time
        i4004_writeDataBus(nibbleToSend);
      }

      break;
    }
    case StageM1: {
      // if there is CMRAM lines active due M1 it means that first instruction is executing
      uint8_t cmram = i4004_readCMRAM();
      if (cmram) {
        // HLT
        if (currentROMByte == 0x01) {
          stopCycler();
          return;
        }
      }

      // we want to exit right after sending data to bus to be able to catch next tick, so we don't want to execute any statements before return
      currentStage = StageM2;

      // don't write anything to data bus
      if (emulatorState != StateResetting) {
        uint8_t nibbleToSend = currentROMByte & 0xFU;
        waitEndOfCycle();
        // should be as less instruction as possible to output data into bus in time
        i4004_writeDataBus(nibbleToSend);
      }

      break;
    }
    case StageM2: {
      // if there is CMRAM lines active due M2 it means that IO/RAM instruction is executing
      uint8_t cmram = i4004_readCMRAM();
      if (cmram) {
        uint8_t bankNo = cmramToBankNumberMap[cmram];
        selectedBank = &banks[bankNo];
        selectedStatusCharacters = selectedBank->selectedStatusCharacters;
        selectedCharacter = selectedBank->selectedCharacter;
        ioInstructionIsExecuting = 1;
      } else {
        ioInstructionIsExecuting = 0;
      }
      currentStage = StageX1;
      break;
    }
    case StageX1:
      i4004_freeDataBus();
      if (ioInstructionIsExecuting) {
        executeRAMReadInstruction();
      }
      currentStage = StageX2;
      break;
    case StageX2: {
      currentStage = StageX3;
      // turn on CPU on predictable stage
      if (emulatorState == StateResetting) {
        startCycler();
        return;
      }

      // if there is CMRAM lines active due X2 it means that SRC instruction is executing
      uint8_t cmram = i4004_readCMRAM();
      if (cmram) {
        srcInstructionRegNo = i4004_readDataBus();
        uint8_t bankNo = cmramToBankNumberMap[cmram];
        selectedBank = &banks[bankNo];
        srcInstructionIsExecuting = 1;
        return;
      }

      // there is IO/RAM operation, need to process that by RAM
      if (ioInstructionIsExecuting) {
        executeRAMWriteInstruction();
      }

      srcInstructionIsExecuting = 0;
      break;
    }
    default:
      break;
  }
}
