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

int main(int argc, char *argv[])
{
    printf("[%s] Application starts here. PID: %d\n", PROCESS_NAME, getpid());

    return 0;
}