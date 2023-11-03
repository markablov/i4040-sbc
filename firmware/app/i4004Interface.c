#include "main.h"
#include "i4004Interface.h"

void i4004_writeDataBus(uint8_t data) {
  // for optimization pins are sequential 3..7 (4 data pins + enable-bus-write)
  // also need to invert 4-bit data word, because data bus is active low
  OUT_4040_D0_GPIO_Port->BSRR = (uint32_t)(data << 26U) | OUT_4040_DBUS_Pin | (uint32_t)((uint8_t)(data ^ 0xFU) << 10U);
}

void i4004_freeDataBus() {
  OUT_4040_DBUS_GPIO_Port->BSRR = (uint32_t)OUT_4040_DBUS_Pin << 16U;
}

// A4 .. A7
volatile uint8_t i4004_readDataBus() {
  return (IN_4040_D0_GPIO_Port->IDR & 0b0000000011110000U) >> 4U;
}

// C0 ... C3
volatile uint8_t i4004_readCMRAM() {
  return IN_4040_CMRAM0_GPIO_Port->IDR & 0b0000000000001111U;
}

// C13 .. C14
volatile uint8_t i4004_readCMROM() {
  return (IN_4040_CMROM0_GPIO_Port->IDR & 0b0110000000000000U) >> 13U;
}

// A3
volatile uint8_t i4004_readSync() {
  return (IN_4040_SYNC_GPIO_Port->IDR & IN_4040_SYNC_Pin) != 0;
}
