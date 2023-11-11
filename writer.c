// Some fancy copyright message here

// Includes. Please keep them in alpha order
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Defines
#define PROCESS_NAME "writer"
#define FIFO_NAME "TP_FIFO"

#define INPUT_BUFFER_SIZE 1024  // Maximum characters read from input terminal
#define OUTPUT_BUFFER_SIZE 2048 // Yes, a few extra bytes should be enough. But it won't hurt

#define LOG_PREFIX "DATA:"
#define SIGN_PREFIX "SIGN:"

// Private function protoypes

/// @brief Write a chunk of characters to the named FIFO. The buffer must be NULL-terminated.
/// @param fd Named FIFO file descriptor.
/// @param buffer Data to be written.
static void write_to_fifo(int fd, const char* buffer);

/// @brief Helper function that initializes all the signal/exit handlers.
/// @return 0 on success, -1 otherwise.
static int initialize_signal_handlers();

/// @brief Cleanup handler called whenever exit() is being called.
static void exit_handler();

/// @brief SIGUSR signal handler. Originally all the signal parsing and FIFO writing
/// was made in here (and it worked!), but since snprintf() is not signal-safe, I decided
/// setting a sig_atomic_t flag and process it later on a background thread.
/// @param signo Should be SIGUSR1 or SIGUSR2.
static void sigusr_handler(int signo);

/// @brief SIGINT/SIGPIPE handler. The aim of this function is to notify the user
/// what's happening and exit in a clean way.
/// @param signo Should be SIGPIPE or SIGINT.
static void signals_handler(int signo);

/// @brief Background thread that will monitor sigusr_flag periodically.
/// Yes, adding a thread may introduce shared resources issues (the output_buffer in this case),
/// but for the scope of this project (wich will be run on a terminal by a human) it shouldn't bother.
/// @param unused
/// @return NULL
static void* background_thread(void* unused);

// Private global variables
static char output_buffer[OUTPUT_BUFFER_SIZE] = {0};
static int fifo_fd = -1;
volatile sig_atomic_t sigusr_flag = 0;

static void exit_handler()
{
    printf("[%s] Attempt to delete the FIFO before exit\n", PROCESS_NAME);

    if(remove(FIFO_NAME) == 0) {
        printf("[%s] FIFO deleted successfully\n", PROCESS_NAME);
    } else {
        printf("[%s] remove error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
    }

    // In theory the C runtime system will cleanup the threads at exit, so there's no need
    // to call pthread_join here.

    printf("[%s] Exiting...\n", PROCESS_NAME);
}

static void sigusr_handler(int signo)
{
    switch (signo)
    {
    case SIGUSR1:
        sigusr_flag = 1;
        break;
    case SIGUSR2:
        sigusr_flag = 2;
        break;
    default:
        write(1, "Unknown signal\n", sizeof("Unknown signal\n"));
        return;
    }
}

static void signals_handler(int signo)
{

    int exit_status = 0;

    const char* SIGPIPE_MSG = "Received SIGPIPE - We're writing on a pipe that nobody is reading!\n";
    const char* SIGINT_MSG = "\nReceived SIGINT - User wants to quit application!\n";

    switch (signo)
    {
    case SIGPIPE:
        write(1, SIGPIPE_MSG, sizeof(SIGPIPE_MSG));
        exit_status = EXIT_FAILURE;
        break;
    case SIGINT:
        write(1, SIGINT_MSG, sizeof(SIGINT_MSG));
        exit_status = EXIT_SUCCESS;
        break;
    default:
        break;
    }

    // In any case just exit.
    exit(exit_status);
}

static void write_to_fifo(int fd, const char* buffer)
{
    if(buffer == NULL) {
        printf("[%s] Received NULL buffer. Returning...\n", PROCESS_NAME);
        return;
    }

    int bytes_written = write(fd, buffer, strlen(buffer));
    if(bytes_written != -1) {
        printf("[%s] Written %d bytes\n", PROCESS_NAME, bytes_written);
    } else {
        printf("[%s] write error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
    }
}

static int initialize_signal_handlers()
{
    int ret = -1;
    struct sigaction sa;

    // First stage: register a handler for the exit() call. Unfortunately an error won't update errno :(
    if ((ret = atexit(exit_handler)) != 0) {
        fprintf(stderr, "[%s] atexit error %d\n", PROCESS_NAME, ret);
        return ret;
    }

    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    // Second stage: register the handler for SIGUSR signals
    sa.sa_handler = sigusr_handler;
    if((ret = sigaction(SIGUSR1, &sa, NULL)) != 0) {
        printf("[%s] sigaction error for SIGUSR1 %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        return ret;
    }

    if((ret = sigaction(SIGUSR2, &sa, NULL)) != 0) {
        printf("[%s] sigaction error for SIGUSR2 %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        return ret;
    }

    // Third stage: register the handler for SIGINT/SIGPIPE signals
    sa.sa_handler = signals_handler;
    if((ret = sigaction(SIGPIPE, &sa, NULL)) != 0) {
        printf("[%s] sigaction error for SIGPIPE %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        return ret;
    }

    if((ret = sigaction(SIGINT, &sa, NULL)) != 0) {
        printf("[%s] sigaction error for SIGINT %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
        return ret;
    }

    return ret;
}

static void* background_thread(void* unused)
{
    while(1) {
        if (sigusr_flag) {
            // Perform the required actions
            printf("[%s] Received SIGUSR%d\n", PROCESS_NAME, sigusr_flag);

            snprintf(output_buffer, sizeof(output_buffer), "%s%d", SIGN_PREFIX, sigusr_flag);
            write_to_fifo(fifo_fd, output_buffer);

            // Reset sigusr_flag. Must be the latest thing to do.
            sigusr_flag = 0;
        }

        // Introduce a delay to avoid high CPU usage
        usleep(1000);
    }
    return NULL;
}

/// @brief Main entry
/// @param argc
/// @param argv
/// @return
int main(int argc, char *argv[])
{
    char input_buffer[INPUT_BUFFER_SIZE] = {0};
    pthread_t sigusr_thread_id;

    printf("[%s] Application starts here. PID: %d\n", PROCESS_NAME, getpid());

    // Create the monitor thread for SIGUSR signals.
    if (pthread_create(&sigusr_thread_id, NULL, background_thread, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    printf("[%s] SIGUSR monitor thread started successfully\n", PROCESS_NAME);

    if(initialize_signal_handlers() == -1) {
        exit(EXIT_FAILURE);
    }
    printf("[%s] Signal handlers registered successfully\n", PROCESS_NAME);

    // Create the named FIFO. If it already exist just inform it and continue execution.
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

    printf("[%s] Created FIFO node %s \n", PROCESS_NAME, FIFO_NAME);

    printf("[%s] Waiting for readers...\n", PROCESS_NAME);

    // Open the named FIFO. The program will block here until other process opens it.
    if((fifo_fd = open(FIFO_NAME, O_WRONLY) ) < 0) {
        printf("[%s] Error opening FIFO %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
    } else {
        printf("[%s] Got a reader process on the FD %d. Type some stuff\n", PROCESS_NAME, fifo_fd);
    }

    while(1) {
        // Read some text from the console
        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
            printf("[%s] fgets error %d (%s)\n", PROCESS_NAME, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }

        // Replace '\n' with '\0' before formatting the output string
        size_t len = strlen(input_buffer);
        if(len > 0 && input_buffer[len-1] == '\n') {
            input_buffer[len-1] = '\0';
        }

        // Format the output string by prepending "DATA:" as prefix
        snprintf(output_buffer, sizeof(output_buffer), "%s%s", LOG_PREFIX, input_buffer);

        write_to_fifo(fifo_fd, output_buffer);
    }

    // The program should never reach here, but the join call is added just to be safe.
    int result = pthread_join(sigusr_thread_id, NULL);
    if (result == 0) {
        printf("[%s] Monitor thread %ld ended successfully\n", PROCESS_NAME, sigusr_thread_id);
    } else {
        printf("[%s] phtread_join error %d\n", PROCESS_NAME, result);
    }

    return 0;
}