#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/**
 * Client server fifo
 */
#define CLIENT_SERVER "/tmp/client_server_fifo"
/**
 * Relative position of configuration file argument in argv
 */
#define CONFIG_ARG(argv)(argv[1])
/**
 * Relative position of bin path in argv
 */
#define BIN_PATH_ARG(argv)(argv[2])

/**
 * Global variable that hold bin path for executing transformations.
 */
char *g_bin_path;

/**
 * Handles an error by exiting with code EXIT_FAILURE. Also unlinks fifo if opened.
 * @param error_number -1 triggers exit
 * @param error_message message to print
 * @return 0 if no error occurs
 */
int errorHandler(int error_number , char *error_message){
    if(error_number == -1){
        perror(error_message);
        unlink(CLIENT_SERVER);
        _exit(EXIT_FAILURE);
    }
    return 0;
}

int main(int argc , char *argv[]) {
    if(argc < 3){
        errorHandler(-1,"Invalid argument size.\n");
    }

    errorHandler(mkfifo(CLIENT_SERVER,0777),CLIENT_SERVER);

    unlink(CLIENT_SERVER);
    return 0;
}
