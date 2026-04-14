#include "cpu_load.h"

#include "error.h"
#include "stm32n6xx_hal.h"
#include "thread_config.h"
#include "timebase.h"
#include "tx_api.h"

typedef unsigned long long cpu_load_execution_time_t;
#ifdef TX_EXECUTION_PROFILE_ENABLE
extern UINT _tx_execution_idle_time_reset(void);
extern UINT _tx_execution_idle_time_get(cpu_load_execution_time_t *total_time);
#endif

#define CPU_LOAD_SAMPLE_TICKS MS_TO_TICKS(CPU_LOAD_SAMPLE_PERIOD_MS)

static TX_THREAD s_cpu_load_thread;
static UCHAR s_cpu_load_stack[CPU_LOAD_THREAD_STACK_SIZE];
static TX_EVENT_FLAGS_GROUP s_cpu_load_update_event_flags;

static volatile float s_usage_ratio = 0.0f;
static uint32_t s_start_cycle = 0U;
static bool s_started = false;
static volatile uint32_t s_sample_seq = 0U;
static cpu_load_sample_t s_latest_sample = {0};

#define CPU_LOAD_EVT_SAMPLE_READY 0x01UL

static void cpu_load_thread_entry(ULONG arg);
#ifdef TX_EXECUTION_PROFILE_ENABLE
static void cpu_load_publish_snapshot(float usage_ratio);
#endif

static void cpu_load_init(void) {
  s_usage_ratio = 0.0f;
  s_start_cycle = 0U;
  s_started = false;
  s_sample_seq = 0U;
  s_latest_sample.timestamp_ms = 0U;
  s_latest_sample.usage_ratio = 0.0f;

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

bool CPU_LoadGetLatestSample(cpu_load_sample_t *sample) {
  uint32_t seq_start;
  uint32_t seq_end;

  if (sample == NULL || s_sample_seq == 0U) {
    return false;
  }

  do {
    seq_start = s_sample_seq;
    if (seq_start & 1U) {
      continue;
    }
    __DMB();
    *sample = s_latest_sample;
    __DMB();
    seq_end = s_sample_seq;
  } while ((seq_start & 1U) || (seq_start != seq_end));

  return true;
}

TX_EVENT_FLAGS_GROUP *CPU_LoadGetUpdateEventFlags(void) {
  return &s_cpu_load_update_event_flags;
}

void CPU_LoadThreadStart(void) {
  UINT status;

  cpu_load_init();
  status =
      tx_event_flags_create(&s_cpu_load_update_event_flags, "cpu_load_update");
  APP_REQUIRE(status == TX_SUCCESS);
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

  s_sample_seq++;
  __DMB();
  s_latest_sample.timestamp_ms = HAL_GetTick();
  s_latest_sample.usage_ratio = usage_ratio;
  s_usage_ratio = usage_ratio;
  __DMB();
  s_sample_seq++;
  tx_event_flags_set(&s_cpu_load_update_event_flags, CPU_LOAD_EVT_SAMPLE_READY,
                     TX_OR);
}
#endif
