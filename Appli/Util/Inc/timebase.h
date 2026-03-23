#ifndef TIMEBASE_H
#define TIMEBASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint32_t Timebase_CyclesToMs(uint32_t cycles);

#ifdef __cplusplus
}
#endif

#endif /* TIMEBASE_H */
