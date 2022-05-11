#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <string.h>
#include <signal.h>
#include "request.h"

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
 * Executable maximum capacity position in monitor
 */
#define EXEC_MAX 0
/**
 * Executable current capacity position in monitor
 */
#define EXEC_CURR 1
/**
 * Maximum capacity of global buffer
 */
#define MAX_BUFFER 1024
/**
 * Global variable that contains information on executables
 */
int g_exec_monitor[7][2];

/**
 * Global variable that contains information of current tasks
 */
int g_task_monitor[32][2];

/**
 * Global variable representing the current number of next running task
 */
int g_curr_task;

/**
 * Global variable that contains paths to the executables
 */
char *g_exec_commands[7];

/**
 * Global variable that serves as buffer for reads
 */
char g_buffer[MAX_BUFFER];

/**
 * Global variable indicating server status
 */
int g_running_status;

pid_t g_manager;

/**
 * Enum representing transformations
 */
typedef enum {
    NOP,
    B_COMPRESS,
    B_DECOMPRESS,
    G_COMPRESS,
    G_DECOMPRESS,
    ENCRYPT,
    DECRYPT,
    INVALID_TC = -1
}TRANSFORM;

typedef enum {
    PROC_FILE,
    STATUS,
    INVALID_OP = -1
}OPTION;

typedef enum {
    DONE,
    PENDING,
    PROCESSING
}TASK_STATUS;

/**
 * Enum representing server status.
 */
typedef enum {
    SHUTDOWN,
    ONLINE
}SERVER_STATUS;

void sigtermHandler(int signum){
    g_running_status = SHUTDOWN;
}

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

/**
 * @brief A character from a input file.
 *
 * @param fd file descriptor
 * @param c character read
 * @return int 1 : successful read; 0 : no more characters to read
 */
ssize_t readc(int fd, char *c)
{
    /* initializes current position higher than end position to force reading input file on first instance */
    static ssize_t buffer_current_pos = 0, buffer_end_pos = 0;

    /* if current is past the end */
    if (buffer_current_pos == buffer_end_pos)
    {
        /* read from file again */
        errorHandler((int)(buffer_end_pos = read(fd, &g_buffer, MAX_BUFFER)),"Error reading to buffer");
        buffer_current_pos = 0;
    }

    /* checks if its is safe to read from the buffer */
    if (buffer_current_pos < buffer_end_pos)
    {
        *c = g_buffer[buffer_current_pos++];
    }

    return buffer_end_pos;
}

/**
 * @brief Reads a '\n' terminated sequence of characters from an input file and retrns the number of bytes read.
 * User must ensure adequate size of line buffer to prevent overflow.
 * @param fd file descriptor
 * @param line line buffer
 * @param size size of buffer
 * @return size_t bytes read into line buffer
 */
ssize_t readln(int fd, char *line, size_t size)
{
    ssize_t i = 0 , r = 0;
    char c = '\0';
    /* keeps reading from a file one character at a time */
    while (i < size - 2 && readc(fd, &c) > 0 && c != '\n')
    {
        line[i++] = c;
    }
    if(c == '\n'){
        line[i++] = '\n';
        r = i;
    }
    line[i] = '\0';

    return r;
}

/**
 * Opens a file descriptor and handles an error if necessary.
 * @param path path to file
 * @param mode access mode
 * @return returns a valid file descriptor
 */
int openFileDescriptor(char *path , int mode){
    int fd = open(path,mode);
    errorHandler(fd,path);
    return fd;
}

/**
 * Read a string from a file
 * @param fd file descriptor
 * @param buffer buffer to write
 * @param buffer_size size of buffer
 * @return bytes written to file
 */
ssize_t readFromFile(int fd, char *buffer , size_t buffer_size){
    ssize_t bytes_read = read(fd,buffer,buffer_size);
    errorHandler((int)bytes_read,"Error reading from file.");
    return bytes_read;
}

/**
 * Writes a string to a file
 * @param fd file descriptor
 * @param str string to write
 * @return bytes written to file
 */
ssize_t writeToFile(int fd, char *str){
    ssize_t bytes_wrote = write(fd,str,strlen(str)+1);
    errorHandler((int)bytes_wrote,"Error writing to file.");
    return bytes_wrote;
}

/**
 *
 * @param dest
 * @param src
 * @return
 */
int initBinPath(const char *path){
    char *exec_name[7] ={
            "nop",
            "bcompress",
            "bdecompress",
            "gcompress",
            "gdecompress",
            "encrypt",
            "decrypt"
    };

    for (int i = 0; i < 7; ++i) {
        g_exec_commands[i] = malloc(strlen(strlen(path) + exec_name[i]) + 1);
        errorHandler(g_exec_commands[i] == NULL ? -1 : 0,"Malloc error");
        errorHandler(sprintf(g_exec_commands[i],"%s/%s",path,exec_name[i]),"sprintf error.\n");
    }

    return 0;
}

TRANSFORM parseTransform(char *str){
    TRANSFORM t_code = INVALID_TC;

    if(!strcmp(str,"nop")){
        t_code = NOP;
    }else if(!strcmp(str,"bcompress")){
        t_code = B_COMPRESS;
    }else if(!strcmp(str,"bdecompress")){
        t_code = B_DECOMPRESS;
    }else if(!strcmp(str,"gcompress")){
        t_code = G_COMPRESS;
    }else if(!strcmp(str,"gdecompress")){
        t_code = G_DECOMPRESS;
    }else if(!strcmp(str,"encrypt")){
        t_code = ENCRYPT;
    }else if(!strcmp(str,"decrypt")){
        t_code = DECRYPT;
    }else{
        t_code = INVALID_TC;
    }
    errorHandler(t_code,"Invalid transformation.\n");
    return t_code;
}

int initExecMonitor(char *path_to_config){
    int fd = openFileDescriptor(path_to_config,O_RDONLY);

    char *line = calloc(MAX_BUFFER,sizeof(char));
    errorHandler(line == NULL ? -1 : 0,"Malloc error");
    while (readln(fd,line,MAX_BUFFER) > 0){
        char *rest;
        char *tokens = line;
        char *exec = strsep(&tokens," ");
        char *max_exec = strsep(&tokens," ");
        errorHandler(exec == NULL ? -1 : 0,"strsep error");
        errorHandler(max_exec == NULL ? -1 : 0,"strsep error");
        TRANSFORM t_code = parseTransform(exec);
        long m = strtol(max_exec,&rest,10);
        errorHandler(strlen(rest) == 1 && *rest == '\n' ? 0 : -1,"Error parsing max.\n");
        g_exec_monitor[t_code][EXEC_MAX] = (int)m;
        g_exec_monitor[t_code][EXEC_CURR] = 0;
    }
    if(line != NULL){
        free(line);
    }
    close(fd);
    return 0;
}

void processFile(char **args){
    long t = strtol(strsep(args," "),NULL,10);
    int fd_in = openFileDescriptor(strsep(args," "),O_RDONLY);
    int path_out = open(strsep(args," "),O_WRONLY | O_CREAT,0777);

    pid_t pid = fork();

    if(pid == 0){
        _exit(0);
    }
}

void assignRequest(REQUEST request){

        int fd = openFileDescriptor(CLIENT_SERVER,O_WRONLY);
        kill(getRequestSender(request),SIGUSR1);
        sleep(1);
        close(fd);


}

int main(int argc , char *argv[]) {
    signal(SIGTERM,sigtermHandler);

    if(argc < 3){
        errorHandler(-1,"Invalid argument size.\n");
    }

    errorHandler(mkfifo(CLIENT_SERVER,0777),CLIENT_SERVER);

    initExecMonitor(CONFIG_ARG(argv));
    initBinPath(BIN_PATH_ARG(argv));

    g_running_status = ONLINE;
    while(g_running_status == ONLINE){
        REQUEST request = NULL;
        int fd = openFileDescriptor(CLIENT_SERVER,O_RDONLY);
        readRequest(fd,&request);
        close(fd);
        char fifo[1024];
        sprintf(fifo,"/tmp/%d", getRequestSender(request));
        switch (getRequestType(request)) {
            case PROC_FILE:
                g_task_monitor[g_curr_task][0] = getRequestSender(request);
                g_task_monitor[g_curr_task][1] = fork();
                if(g_task_monitor[g_curr_task][1] == 0){
                    fd = openFileDescriptor(fifo,O_WRONLY);
                    kill(getRequestSender(request),SIGUSR1);
                    _exit(0);
                }
                g_curr_task++;
                break;
            case STATUS:
                break;
            default:
                errorHandler(INVALID_OP,"invalid operation.\n");
                break;
        }
    }
    unlink(CLIENT_SERVER);
    return 0;
}
