#ifndef STM32_MCS4_MEMORY_EMULATOR_CYCLE_HANDLER_H
#define STM32_MCS4_MEMORY_EMULATOR_CYCLE_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { StateStale = 0, StateFinished = 1, StateResetting = 2, StateRunning = 3 } CyclerState;

void handleCyclePhi1Falling();
void printCyclerStats();
void runCycler();

extern volatile CyclerState emulatorState;

#ifdef __cplusplus
}
#endif

#endif // STM32_MCS4_MEMORY_EMULATOR_CYCLE_HANDLER_H
