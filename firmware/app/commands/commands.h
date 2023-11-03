#ifndef STM32_MCS4_MEMORY_EMULATOR_COMMANDS_H
#define STM32_MCS4_MEMORY_EMULATOR_COMMANDS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

enum InCommandType { CmdStart = 0 };
enum OutCommandType { CmdAck, CmdOutput, CmdFinish };

void cmdStart(uint8_t *data, uint16_t dataLen);

#ifdef __cplusplus
}
#endif

#endif //STM32_MCS4_MEMORY_EMULATOR_COMMANDS_H
