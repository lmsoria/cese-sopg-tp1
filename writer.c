#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PROCESS_NAME "writer"
#define FIFO_NAME "TP_FIFO"
#define BUFFER_SIZE 1024

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

void sigpipe_handler(int signo) {
    if(signo == SIGPIPE) {
        write(1, "SIGPIPE - We're writing on a pipe that nobody is reading!\n", sizeof("SIGPIPE - We're writing on a pipe that nobody is reading!\n"));
        if(remove(FIFO_NAME) == 0) {
            write(1, "FIFO deleted successfully\n", sizeof("FIFO deleted successfully\n"));
        } else {
            write(1, "Unable to delete the FIFO\n", sizeof("Unable to delete the FIFO\n"));
        }
        write(1, "Exiting...\n", sizeof("Exiting...\n"));
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    char output_buffer[BUFFER_SIZE] = {0};
    struct sigaction sa;

    int fifo_fd = 0;
    int bytes_written = 0;

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

    sa.sa_handler = sigpipe_handler;
    if(sigaction(SIGPIPE, &sa, NULL) != 0) {
        printf("[%s] sigaction error for SIGPIPE %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(mknod(FIFO_NAME, S_IFIFO | 0666, 0) != 0) {
        printf("[%s] mknod error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("[%s] Waiting for readers...\n", PROCESS_NAME);

    if((fifo_fd = open(FIFO_NAME, O_WRONLY) ) < 0) {
        printf("[%s] Error opening FIFO %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
    } else {
        printf("[%s] Got a reader process on fd %d. Type some stuff\n", PROCESS_NAME, fifo_fd);
    }

    while(1) {
        fgets(output_buffer, BUFFER_SIZE, stdin);

        bytes_written = write(fifo_fd, output_buffer, strlen(output_buffer)-1);
        if(bytes_written != -1) {
			printf("[%s] Written %d bytes\n", PROCESS_NAME, bytes_written);
        } else {
            printf("[%s] write error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        }
    }

    return 0;
}