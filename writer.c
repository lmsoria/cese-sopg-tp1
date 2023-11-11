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
#define INPUT_BUFFER_SIZE 1024
#define OUTPUT_BUFFER_SIZE 2048

static char output_buffer[OUTPUT_BUFFER_SIZE] = {0};
static int fifo_fd = 0;

static void write_to_fifo(int fd, const char* buffer);

void sigusr_handler(int signo)
{
    const char* SIGUSR1_MSG = "SIGN:1";
    const char* SIGUSR2_MSG = "SIGN:2";

    switch (signo)
    {
    case SIGUSR1:
        write(1, "Received SIGUSR1\n", sizeof("Received SIGUSR1\n"));
        strncpy(output_buffer, SIGUSR1_MSG, sizeof(output_buffer));
        break;
    case SIGUSR2:
        write(1, "Received SIGUSR2\n", sizeof("Received SIGUSR2\n"));
        strncpy(output_buffer, SIGUSR2_MSG, sizeof(output_buffer));
        break;
    default:
        write(1, "Unknown signal\n", sizeof("Unknown signal\n"));
        return;
    }

    write_to_fifo(fifo_fd, output_buffer);
}

void signals_handler(int signo) {

    int exit_status = 0;
    switch (signo)
    {
    case SIGPIPE:
        write(1, "Received SIGPIPE - We're writing on a pipe that nobody is reading!\n", sizeof("Received SIGPIPE - We're writing on a pipe that nobody is reading!\n"));
        exit_status = EXIT_FAILURE;
        break;
    case SIGINT:
        write(1, "Received SIGINT - User wants to quit application!\n", sizeof("Received SIGINT - User wants to quit application!\n"));
        exit_status = EXIT_SUCCESS;
        break;
    default:
        break;
    }

    if(remove(FIFO_NAME) == 0) {
        write(1, "FIFO deleted successfully\n", sizeof("FIFO deleted successfully\n"));
    } else {
        write(1, "Unable to delete the FIFO\n", sizeof("Unable to delete the FIFO\n"));
    }
    write(1, "Exiting...\n", sizeof("Exiting...\n"));
    exit(exit_status);
}

static void write_to_fifo(int fd, const char* buffer)
{
    int bytes_written = write(fd, buffer, strlen(buffer));
    if(bytes_written != -1) {
        printf("[%s] Written %d bytes\n", PROCESS_NAME, bytes_written);
    } else {
        printf("[%s] write error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
    }
}

int main(int argc, char *argv[])
{
    char input_buffer[INPUT_BUFFER_SIZE] = {0};
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

    sa.sa_handler = signals_handler;
    if(sigaction(SIGPIPE, &sa, NULL) != 0) {
        printf("[%s] sigaction error for SIGPIPE %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(sigaction(SIGINT, &sa, NULL) != 0) {
        printf("[%s] sigaction error for SIGINT %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
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
        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
            printf("[%s] fgets error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }

        // Replace '\n' with '\0' before formatting the output string
        size_t len = strlen(input_buffer);
        if(len > 0 && input_buffer[len-1] == '\n') {
            input_buffer[len-1] = '\0';
        }

        // Format the output string by prepending "DATA:" as header
        snprintf(output_buffer, sizeof(output_buffer), "DATA:%s", input_buffer);

        write_to_fifo(fifo_fd, output_buffer);
    }

    return 0;
}