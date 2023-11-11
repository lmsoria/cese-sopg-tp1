#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PROCESS_NAME "reader"
#define FIFO_NAME "TP_FIFO"
#define INPUT_BUFFER_SIZE 1024

#define LOG_FILENAME "log.txt"
#define SIGN_FILENAME "sign.txt"

#define LOG_PREFIX "DATA:"
#define SIGN_PREFIX "SIGN:"

static int fifo_fd = -1;

static FILE* flog = NULL;
static FILE* fsign = NULL;

static void process_input(const char* buffer)
{
    if(buffer == NULL) {
        printf("[%s] Received NULL buffer. Returning...\n", PROCESS_NAME);
        return;
    }

    if (strncmp(buffer, LOG_PREFIX, sizeof(LOG_PREFIX) - 1) == 0) {
        // Cosmetic decision here: don't write the prefix for DATA messages, thus the buffer offset
        fprintf(flog, "%s\n", buffer + sizeof(LOG_PREFIX) - 1);
        fflush(flog);
    } else if (strncmp(buffer, SIGN_PREFIX, sizeof(SIGN_PREFIX) - 1) == 0) {
        // For the signal messages is prettier to store the full message though
        fprintf(fsign, "%s\n", buffer);
        fflush(fsign);
    } else {
        printf("[%s] The buffer does not have a recognized prefix.\n", PROCESS_NAME);
    }
}

int main(int argc, char *argv[])
{
    int bytes_read = -1;
    char input_buffer[INPUT_BUFFER_SIZE] = {0};

    printf("[%s] Application starts here. PID: %d\n", PROCESS_NAME, getpid());

    flog = fopen(LOG_FILENAME, "a");

    if(flog == NULL) {
        printf("[%s] fopen error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("[%s] Created/Opened file %s\n", PROCESS_NAME, LOG_FILENAME);

    fsign = fopen(SIGN_FILENAME, "a");

    if(fsign == NULL) {
        printf("[%s] fopen error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("[%s] Created/Opened file %s\n", PROCESS_NAME, SIGN_FILENAME);

    if(mknod(FIFO_NAME, S_IFIFO | 0666, 0) != 0) {
        switch (errno)
        {
        case EEXIST:
            printf("[%s] FIFO node already exists. Continue...\n", PROCESS_NAME);
            break;
        default:
            printf("[%s] mknod error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    printf("[%s] Created FIFO node %s\n", PROCESS_NAME, FIFO_NAME);

    printf("[%s] Waiting for writers...\n", PROCESS_NAME);

    if((fifo_fd = open(FIFO_NAME, O_RDONLY) ) < 0) {
        printf("[%s] Error opening FIFO %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
    } else {
        printf("[%s] Got a writer process on the FD %d\n", PROCESS_NAME, fifo_fd);
    }

    do
    {
        bytes_read = read(fifo_fd, input_buffer, INPUT_BUFFER_SIZE);
        if(bytes_read == -1) {
            printf("[%s] read error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        } else {
            input_buffer[bytes_read] = '\0';
            printf("[%s] Read %d bytes\n", PROCESS_NAME, bytes_read);
            process_input(input_buffer);
        }
    }
    while (bytes_read > 0);

    return 0;
}