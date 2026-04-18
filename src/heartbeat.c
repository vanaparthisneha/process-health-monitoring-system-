#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>
#include <time.h>
#include <unistd.h>

#include "process.h"
//function to make this task real-time
static int set_task_priority(int prio) {
    pthread_t self = pthread_self();//get current thread
    struct sched_param param;//scheduling parameters
    memset(&param, 0, sizeof(param));
    param.sched_priority = prio;
    return pthread_setschedparam(self, SCHED_FIFO, &param);
}
//sleep function
static void sleep_ms(uint32_t ms) {
    struct timespec req;//Time structure
    req.tv_sec = ms / 1000U;
    req.tv_nsec = (ms % 1000U) * 1000000U;
    nanosleep(&req, NULL);
}

int heartbeat_main(int argc, char *argv[]) {///
    int coid;//Connection id to monitor
    int monitor_pid;//process id of the monitor(to identify which process to connect)
    int monitor_chid;//channel id of the monitor
    int task_id;//different id for the heartbeat task.
    uint32_t heartbeat_period_ms = 300U;//time intervel between tasks

    if (argc < 4) {
        printf("Usage: %s <monitor_pid> <monitor_chid> <task_id> [hb_period_ms]\n", argv[0]);
        return EXIT_FAILURE;
    }

    monitor_pid = atoi(argv[1]);
    monitor_chid = atoi(argv[2]);
    task_id = atoi(argv[3]);
    if (argc >= 5) {
        heartbeat_period_ms = (uint32_t)atoi(argv[4]);
    }

    coid = ConnectAttach(ND_LOCAL_NODE, monitor_pid, monitor_chid, _NTO_SIDE_CHANNEL, 0);//establish connection to monitor
    if (coid == -1) {
        perror("ConnectAttach");
        return EXIT_FAILURE;
    }

    if (set_task_priority(15) != 0) {
        perror("pthread_setschedparam");
        printf("[INFO] Continuing without RT priority adjustment\n");
    }//continue even if it fails
      //prints task id and heart beat interval
    printf("[INFO] Heartbeat task started: id=%d, period=%u ms\n", task_id, heartbeat_period_ms);
    while (1) {
        if (MsgSendPulse(coid, SIGEV_PULSE_PRIO_INHERIT, PULSE_HEARTBEAT, task_id) != 0) {
            perror("MsgSendPulse");
        }
        sleep_ms(heartbeat_period_ms);//wait before next heartbeat
    }

    return EXIT_SUCCESS;
}
