

#include <stdio.h>
#include <string.h>

int health_monitor_main(void);
int heartbeat_main(int argc, char *argv[]);
int health_cli_main(int argc, char *argv[]);

static void print_usage(const char *app) {
    printf("Usage:\n");
    printf("  %s monitor\n", app);
    printf("  %s heartbeat <monitor_pid> <monitor_chid> <task_id> [hb_period_ms]\n", app);
    printf("  %s cli <monitor_pid> <monitor_chid>\n", app);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "monitor") == 0) {
        return health_monitor_main();
    }

    if (strcmp(argv[1], "heartbeat") == 0) {
        return heartbeat_main(argc - 1, &argv[1]);
    }

    if (strcmp(argv[1], "cli") == 0) {
        return health_cli_main(argc - 1, &argv[1]);
    }

    print_usage(argv[0]);
    return 1;
}


