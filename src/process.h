#ifndef COMMON_H
#define COMMON_H

#include <sys/neutrino.h>
#include <stdint.h>

#define MAX_TASKS 8//maximum no.of processes
#define TASK_NAME_LEN 32//maximum lenghth of each task name

/* Pulses sent to health monitor channel */
#define PULSE_HEARTBEAT _PULSE_CODE_MINAVAIL//pulse code for heartbeat event from heartbeat last
#define PULSE_QUERY     (_PULSE_CODE_MINAVAIL + 1)//pulse code for cli query request
#define PULSE_TIMER     (_PULSE_CODE_MINAVAIL + 2)//pulse code used by perodic timer to trigger health

/* Timing policy (milliseconds) */
#define HEALTH_CHECK_INTERVAL_MS 500U//monitor perform perodic check every 500ms
#define HEARTBEAT_TIMEOUT_MS     2000U//if heartbeat is not seen till 2000ms it is unhealthy

typedef struct {
    int32_t task_id;
    uint64_t timestamp_ms;
    char name[TASK_NAME_LEN];
} heartbeat_msg_t;

typedef struct {
    int32_t task_id;
    char name[TASK_NAME_LEN];
    uint8_t alive;
    uint8_t fault_latched;
    uint64_t last_seen_ms;
    uint32_t missed_count;
    uint32_t recovery_count;
} task_status_t;

#endif
