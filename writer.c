#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PROCESS_NAME "writer"

void sigusr_handler(int signo)
{
    switch (signo)
    {
    case SIGUSR1:
        write(1, "SIGUSR1\n", sizeof("SIGUSR1\n"));
        break;
        case SIGUSR2:
        write(1, "SIGUSR2\n", sizeof("SIGUSR2\n"));
        break;
    default:
        write(1, "Unknown signal\n", sizeof("Unknown signal\n"));
        break;
    }
}

int main(int argc, char *argv[])
{
    struct sigaction sa;

    printf("[%s] Application starts here\n", PROCESS_NAME);

    sa.sa_handler = sigusr_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if(sigaction(SIGUSR1, &sa, NULL) != 0) {
        printf("[%s] sigaction error for SIGUSR1 %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(sigaction(SIGUSR2, &sa, NULL) != 0) {
        printf("[%s] sigaction error for SIGUSR2 %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    while(1) {
        sleep(2);
    }

    return 0;
}