#ifndef STM32_MCS4_MEMORY_EMULATOR_EXTERNALINTERFACE_H
#define STM32_MCS4_MEMORY_EMULATOR_EXTERNALINTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define sendExternalMessage(...) sendExternalMessageEx(0, __VA_ARGS__)
#define sendExternalMessageSync(...) sendExternalMessageEx(1, __VA_ARGS__)

void processInputData(uint8_t * data, uint16_t dataLen);
void sendCommandAck();
void sendFinishSignal();
void sendExternalMessageEx(uint8_t sync, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif //STM32_MCS4_MEMORY_EMULATOR_EXTERNALINTERFACE_H
