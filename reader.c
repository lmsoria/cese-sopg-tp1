// Some fancy copyright message here

// Includes. Please keep them in alpha order
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Defines
#define PROCESS_NAME "reader"
#define FIFO_NAME "TP_FIFO"
#define INPUT_BUFFER_SIZE 2048 // Should be equal (at least) to writer.c's OUTPUT_BUFFER_SIZE

#define LOG_FILENAME "log.txt"
#define SIGN_FILENAME "sign.txt"

#define LOG_PREFIX "DATA:"
#define SIGN_PREFIX "SIGN:"

// Private function protoypes

/// @brief Cleanup handler called whenever exit() is being called.
static void exit_handler();

/// @brief SIGINT handler. The aim of this function is to notify the user
/// what's happening and exit in a clean way.
/// @param signo Should be SIGINT.
static void signals_handler(int signo);

/// @brief Helper function that initializes all the signal/exit handlers.
/// @return 0 on success, -1 otherwise.
static int initialize_signal_handlers();

/// @brief Process the data received over the named FIFO and stores it on
/// their corresponding files.
/// @param buffer data to be processed. Must be NULL-terminated.
static void process_input(const char* buffer);

// Private global variables
static int fifo_fd = -1;
static FILE* flog = NULL;
static FILE* fsign = NULL;

static void exit_handler()
{
    printf("[%s] Attempt to close the files before exit.\n", PROCESS_NAME);

    // Closing a NULL file results in UB, so let's check their status first

    if(flog) {
        if(fclose(flog) != 0) {
            fprintf(stderr, "[%s] flose error %d for file %s (%s)\n", PROCESS_NAME, errno, LOG_FILENAME, strerror(errno));
        } else {
            printf("[%s] File %s closed successfuly.\n", PROCESS_NAME, LOG_FILENAME);
        }
    }

    if(fsign) {
        if(fclose(fsign) != 0) {
            fprintf(stderr, "[%s] flose error %d for file %s (%s)\n", PROCESS_NAME, errno, SIGN_FILENAME, strerror(errno));
        } else {
            printf("[%s] File %s closed successfuly.\n", PROCESS_NAME, SIGN_FILENAME);
        }
    }
}

static void signals_handler(int signo)
{
    write(1, "\nReceived SIGINT - User wants to quit application!\n", sizeof("\nReceived SIGINT - User wants to quit application!\n"));
    exit(EXIT_SUCCESS);
}

static int initialize_signal_handlers()
{
    int ret = -1;
    struct sigaction sa;

    // First register a handler for the exit() call. Unfortunately an error won't update errno :(
    if ((ret = atexit(exit_handler)) != 0) {
        fprintf(stderr, "[%s] atexit error %d\n", PROCESS_NAME, ret);
        return ret;
    }

    sa.sa_handler = signals_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    // For the reader process we only care about SIGINT signal (for the moment)
    if((ret = sigaction(SIGINT, &sa, NULL)) != 0) {
        fprintf(stderr, "[%s] sigaction error for SIGINT %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
    }

    return ret;
}

static void process_input(const char* buffer)
{
    // Create a timestamp, just for fun
    time_t current_time;
    char time_buffer[24] = {0};

    if(buffer == NULL) {
        printf("[%s] Received NULL buffer. Returning...\n", PROCESS_NAME);
        return;
    }

    // Format the time using strftime
    time(&current_time);
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", localtime(&current_time));

    if (strncmp(buffer, LOG_PREFIX, sizeof(LOG_PREFIX) - 1) == 0) {
        // Cosmetic decision here: don't write the prefix for DATA messages, thus the buffer offset
        fprintf(flog, "[%s] %s\n", time_buffer, buffer + sizeof(LOG_PREFIX) - 1);
        fflush(flog);
    } else if (strncmp(buffer, SIGN_PREFIX, sizeof(SIGN_PREFIX) - 1) == 0) {
        // For the signal messages is prettier to store the full message though
        fprintf(fsign, "[%s] %s\n", time_buffer, buffer);
        fflush(fsign);
    } else {
        printf("[%s] The buffer does not have a recognized prefix.\n", PROCESS_NAME);
    }
}

/// @brief Main entry
/// @param argc
/// @param argv
/// @return
int main(int argc, char *argv[])
{
    int bytes_read = -1;
    char input_buffer[INPUT_BUFFER_SIZE] = {0};

    printf("[%s] Application starts here. PID: %d\n", PROCESS_NAME, getpid());

    if(initialize_signal_handlers() == -1) {
        exit(EXIT_FAILURE);
    }

    printf("[%s] Signal handlers registered successfully\n", PROCESS_NAME);

    // Open log file. I decided to use append mode, to act more as a datalogger
    flog = fopen(LOG_FILENAME, "a");
    if(flog == NULL) {
       fprintf(stderr, "[%s] fopen error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("[%s] Created/Opened file %s\n", PROCESS_NAME, LOG_FILENAME);

    // Open sign file. I decided to use append mode, to act more as a datalogger.
    fsign = fopen(SIGN_FILENAME, "a");
    if(fsign == NULL) {
        fprintf(stderr, "[%s] fopen error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("[%s] Created/Opened file %s\n", PROCESS_NAME, SIGN_FILENAME);

    // Create the named FIFO. If it already exist just inform it and continue execution.
    if(mknod(FIFO_NAME, S_IFIFO | 0666, 0) != 0) {
        switch (errno)
        {
        case EEXIST:
            printf("[%s] FIFO node already exists. Continue...\n", PROCESS_NAME);
            break;
        default:
            fprintf(stderr, "[%s] mknod error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    printf("[%s] Created FIFO node %s\n", PROCESS_NAME, FIFO_NAME);

    printf("[%s] Waiting for writers...\n", PROCESS_NAME);

    // Open the named FIFO. The program will block here until other process opens it.
    if((fifo_fd = open(FIFO_NAME, O_RDONLY) ) < 0) {
        fprintf(stderr, "[%s] Error opening FIFO %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
    } else {
        printf("[%s] Got a writer process on the FD %d\n", PROCESS_NAME, fifo_fd);
    }

    // Loop until the read syscal returns a value <= 0
    do
    {
        bytes_read = read(fifo_fd, input_buffer, INPUT_BUFFER_SIZE);
        if(bytes_read == -1) {
            fprintf(stderr, "[%s] read error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        } else {
            input_buffer[bytes_read] = '\0';
            printf("[%s] Read %d bytes\n", PROCESS_NAME, bytes_read);
            process_input(input_buffer);
        }
    }
    while (bytes_read > 0);

    return 0;
}