/*
 * health_cli.c
 *
 *  Created on: Apr 17, 2026
 *      Author: Dell
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/neutrino.h>

#include "process.h"

int health_cli_main(int argc, char *argv[]) {
    int coid;
    int monitor_pid;
    int monitor_chid;

    if (argc < 3) {
        printf("Usage: %s <monitor_pid> <monitor_chid>\n", argv[0]);
        return EXIT_FAILURE;
    }

    monitor_pid = atoi(argv[1]);
    monitor_chid = atoi(argv[2]);

    coid = ConnectAttach(ND_LOCAL_NODE, monitor_pid, monitor_chid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1) {
        perror("ConnectAttach");
        return EXIT_FAILURE;
    }

    if (MsgSendPulse(coid, SIGEV_PULSE_PRIO_INHERIT, PULSE_QUERY, 0) != 0) {
        perror("MsgSendPulse");
        return EXIT_FAILURE;
    }

    printf("[INFO] Health query pulse sent.\n");
    printf("[INFO] Check monitor console for current process health table.\n");
    return EXIT_SUCCESS;
}
