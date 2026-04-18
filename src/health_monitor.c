#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/netmgr.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <time.h>
#include <unistd.h>

#include "process.h"

typedef struct {
    task_status_t table[MAX_TASKS];
    uint32_t count;
} monitor_state_t;
//creates timer which converts into milliseconds
static uint64_t now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0U;
    }
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}
//to find or create task
static task_status_t *find_or_add_task(monitor_state_t *state, int32_t task_id, const char *name) {
    uint32_t i;
    for (i = 0U; i < state->count; ++i) {//checks all stored task
        if (state->table[i].task_id == task_id) {//task id matches found &return
            return &state->table[i];
        }
    }

    if (state->count >= MAX_TASKS) {//no space for add new task
        return NULL;
    }
     //Allocating new task
    task_status_t *slot = &state->table[state->count];
    memset(slot, 0, sizeof(*slot));
    slot->task_id = task_id;
    strncpy(slot->name, name, TASK_NAME_LEN - 1U);//initalize memory
    slot->name[TASK_NAME_LEN - 1U] = '\0';
    state->count++;
    return slot;
}
//printing task id,name,alive/dead,last seen ,miss count
static void print_status_table(const monitor_state_t *state) {
    uint32_t i;
    printf("\n==== HEALTH STATUS TABLE ====\n");
    printf("%-8s %-24s %-8s %-12s %-8s %-10s\n", "TaskID", "Name", "Alive", "LastSeen(ms)", "Missed", "Recovered");
    for (i = 0U; i < state->count; ++i) {
        const task_status_t *s = &state->table[i];
        printf("%-8d %-24s %-8s %-12llu %-8u %-10u\n",
               s->task_id,
               s->name,
               s->alive ? "YES" : "NO",
               (unsigned long long)s->last_seen_ms,
               s->missed_count,
               s->recovery_count);
    }
    printf("=============================\n");
    fflush(stdout);//forces immediate output
}

static void execute_fault_action(task_status_t *s) {
    if (s->fault_latched) {
        return;
    }
    s->fault_latched = 1U;
    printf("[ACTION] Fault latched for task %s (id=%d). Escalating alert.\n", s->name, s->task_id);
}

static void execute_recovery_action(task_status_t *s) {
    if (!s->fault_latched) {
        return;
    }
    s->fault_latched = 0U;
    s->recovery_count++;
    printf("[ACTION] Task %s (id=%d) recovered. Clearing fault latch.\n", s->name, s->task_id);
}
//Health evaluation-checks each task and decides alive/dead
static void evaluate_health(monitor_state_t *state) {
    uint32_t i;
    const uint64_t now = now_ms();

    for (i = 0U; i < state->count; ++i) {
        task_status_t *s = &state->table[i];
        if (s->last_seen_ms == 0U) {
            s->alive = 0U;
            continue;
        }

        if ((now - s->last_seen_ms) > HEARTBEAT_TIMEOUT_MS) {
            if (s->alive) {
                printf("[WARN] Task %s (id=%d) missed heartbeat timeout\n", s->name, s->task_id);
                s->missed_count++;
            }
            s->alive = 0U;
            execute_fault_action(s);
        } else {
            s->alive = 1U;
            execute_recovery_action(s);
        }
    }
}
//Setting real time priority
static int set_realtime_priority(int prio) {
    pthread_t self = pthread_self();
    struct sched_param param;//structure used to store scheduling info
    memset(&param, 0, sizeof(param));//clear memory
    param.sched_priority = prio;//assingn priority
    return pthread_setschedparam(self, SCHED_FIFO, &param);//schedule policy FIFO
}
//Time Setup
static int setup_periodic_timer(int chid, timer_t *timer_id) {
    struct sigevent sev;
    struct itimerspec its;

    SIGEV_PULSE_INIT(&sev, ConnectAttach(ND_LOCAL_NODE, 0, chid, _NTO_SIDE_CHANNEL, 0), SIGEV_PULSE_PRIO_INHERIT,
                     PULSE_TIMER, 0);//Configure pulse event and connect to channel

    if (timer_create(CLOCK_MONOTONIC, &sev, timer_id) != 0) {//create timer
        perror("timer_create");
        return -1;
    }

    memset(&its, 0, sizeof(its));
    //set perodic timer
    its.it_value.tv_sec = HEALTH_CHECK_INTERVAL_MS / 1000U;
    its.it_value.tv_nsec = (HEALTH_CHECK_INTERVAL_MS % 1000U) * 1000000U;
    its.it_interval = its.it_value;

    if (timer_settime(*timer_id, 0, &its, NULL) != 0) {//Start timer
        perror("timer_settime");
        return -1;
    }
    return 0;
}
//Main function
int health_monitor_main(void) {
    int rcvid;
    int chid;
    monitor_state_t state;//holds all tasks
    timer_t timer_id;
    struct _pulse pulse;

    memset(&state, 0, sizeof(state));//initializing

    chid = ChannelCreate(0);//create communication channel
    if (chid == -1) {
        perror("ChannelCreate");
        return EXIT_FAILURE;
    }

    if (set_realtime_priority(25) != 0) {//Setting  high priority
        perror("pthread_setschedparam");
        printf("[INFO] Continuing without RT priority adjustment\n");
    }

    if (setup_periodic_timer(chid, &timer_id) != 0) {//start periodic health check
        ChannelDestroy(chid);
        return EXIT_FAILURE;
    }

    printf("[INFO] Health monitor started. PID: %d, Channel ID: %d\n", (int)getpid(), chid);
    printf("[INFO] Pulse codes: heartbeat=%d, timer=%d\n", PULSE_HEARTBEAT, PULSE_TIMER);
    printf("[INFO] Check interval=%u ms, timeout=%u ms\n", HEALTH_CHECK_INTERVAL_MS, HEARTBEAT_TIMEOUT_MS);
//main loop
    while (1) {
        rcvid = MsgReceive(chid, &pulse, sizeof(pulse), NULL);//wait for pulses
        if (rcvid == -1) {//error occurs
            perror("MsgReceive");
            continue;
        }

        if (rcvid == 0) {//pulse recived
            if (pulse.code == PULSE_HEARTBEAT) {//identify heartbeat pulse
                char fallback_name[TASK_NAME_LEN];
                task_status_t *slot;
                snprintf(fallback_name, sizeof(fallback_name), "task_%d", pulse.value.sival_int);//converts task id ->name
                slot = find_or_add_task(&state, pulse.value.sival_int, fallback_name);//if task exit->return/if not->create new task
                if (slot != NULL) {//ensure table/slot is valid
                    slot->last_seen_ms = now_ms();
                    slot->alive = 1U;
                }
            } else if (pulse.code == PULSE_TIMER) {//mark task alive
                evaluate_health(&state);
                print_status_table(&state);
            } else if (pulse.code == PULSE_QUERY) {
                print_status_table(&state);
            }
        }
    }

    return EXIT_SUCCESS;
}
