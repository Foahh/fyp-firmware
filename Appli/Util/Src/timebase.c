#include "timebase.h"
#include "stm32n6xx_hal.h"

void Timebase_EnableDWT(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t Timebase_CyclesToMs(uint32_t cycles) {
  if (SystemCoreClock == 0U) {
    return 0U;
  }
  return (uint32_t)(((uint64_t)cycles * 1000ULL + ((uint64_t)SystemCoreClock / 2ULL)) /
                    (uint64_t)SystemCoreClock);
}
