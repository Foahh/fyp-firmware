#include "cpu_load.h"

#include "error.h"
#include "stm32n6xx_hal.h"
#include "timebase.h"
#include "tx_api.h"

typedef unsigned long long cpu_load_execution_time_t;
#ifdef TX_EXECUTION_PROFILE_ENABLE
extern UINT _tx_execution_idle_time_reset(void);
extern UINT _tx_execution_idle_time_get(cpu_load_execution_time_t *total_time);
#endif

/* CPU monitor thread configuration */
#define CPU_LOAD_THREAD_STACK_SIZE 1024U
#define CPU_LOAD_THREAD_PRIORITY   10U
#define CPU_LOAD_SAMPLE_PERIOD_MS  100U
#define CPU_LOAD_SAMPLE_TICKS      MS_TO_TICKS(CPU_LOAD_SAMPLE_PERIOD_MS)

static TX_THREAD s_cpu_load_thread;
static UCHAR s_cpu_load_stack[CPU_LOAD_THREAD_STACK_SIZE];

static volatile float s_usage_ratio = 0.0f;
static uint32_t s_start_cycle = 0U;
static bool s_started = false;

static void cpu_load_thread_entry(ULONG arg);
#ifdef TX_EXECUTION_PROFILE_ENABLE
static void cpu_load_publish_snapshot(float usage_ratio);
#endif

static void cpu_load_init(void) {
  s_usage_ratio = 0.0f;
  s_start_cycle = 0U;
  s_started = false;

#ifdef TX_EXECUTION_PROFILE_ENABLE
  (void)_tx_execution_idle_time_reset();
#endif
}

bool CPU_LoadSample(void) {
#ifndef TX_EXECUTION_PROFILE_ENABLE
  return false;
#else
  const uint32_t current_cycle = DWT->CYCCNT;

  if (!s_started) {
    s_started = true;
    s_start_cycle = current_cycle;
    (void)_tx_execution_idle_time_reset();
    return false;
  }

  const uint32_t elapsed = current_cycle - s_start_cycle;
  if (elapsed == 0U) {
    return false;
  }

  cpu_load_execution_time_t idle_time = 0U;
  (void)_tx_execution_idle_time_get(&idle_time);
  (void)_tx_execution_idle_time_reset();

  uint32_t idle_cycles = (uint32_t)idle_time;
  if (idle_cycles > elapsed) {
    idle_cycles = elapsed;
  }

  const uint32_t busy_cycles = elapsed - idle_cycles;
  cpu_load_publish_snapshot((float)busy_cycles / (float)elapsed);

  s_start_cycle = current_cycle;
  return true;
#endif
}

float CPU_LoadGetUsageRatio(void) {
  return s_usage_ratio;
}

bool CPU_LoadGetPercent(float *usage_percent) {
  if (usage_percent == NULL) {
    return false;
  }

  *usage_percent = CPU_LoadGetUsageRatio() * 100.0f;
  return true;
}

void CPU_LoadThreadStart(void) {
  UINT status;

  cpu_load_init();
  status = tx_thread_create(&s_cpu_load_thread, "cpu_load", cpu_load_thread_entry, 0,
                            s_cpu_load_stack, CPU_LOAD_THREAD_STACK_SIZE,
                            CPU_LOAD_THREAD_PRIORITY, CPU_LOAD_THREAD_PRIORITY,
                            TX_NO_TIME_SLICE, TX_AUTO_START);
  APP_REQUIRE(status == TX_SUCCESS);
}

static void cpu_load_thread_entry(ULONG arg) {
  UNUSED(arg);

  while (1) {
    (void)CPU_LoadSample();
    tx_thread_sleep(CPU_LOAD_SAMPLE_TICKS);
  }
}

#ifdef TX_EXECUTION_PROFILE_ENABLE
static void cpu_load_publish_snapshot(float usage_ratio) {
  if (usage_ratio < 0.0f) {
    usage_ratio = 0.0f;
  } else if (usage_ratio > 1.0f) {
    usage_ratio = 1.0f;
  }
  s_usage_ratio = usage_ratio;
}
#endif
