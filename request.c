//
// Created by Sandro Duarte on 11/05/2022.
//
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <printf.h>
#include "request.h"

struct request {
    unsigned int output_file_path_len;
    unsigned long input_file_path_len;
    pid_t sender;
    int request_type;
    int item_number;
    int *items;
    char *input_file_path;
    char *output_file_path;
};

typedef enum {
    PROC_FILE,
    STATUS,
    INVALID_OP = -1
}OPTION;

/**
 * Parses a string into an OPTION. Also handles errors.
 * @param str string to parse
 * @return Valid option.
 */
OPTION parseOption(const char *str){
    OPTION op_code = INVALID_OP;
    if(!strcmp(str,"proc-file")){
        op_code = PROC_FILE;
    }else if(!strcmp(str,"status")){
        op_code = STATUS;
    }
    return op_code;
}

REQUEST newRequest(int sender , int request_type,
                   const char *input_file_path , const char *output_file_path,int item_number , const int *items){
    REQUEST r = calloc(1, sizeof(struct request));
    r->sender = sender;
    r->request_type = request_type;
    r->item_number = item_number;
    if(items && item_number > 0){
        r->items = calloc(item_number,sizeof(int));
        for (int i = 0; i < item_number; ++i) {
            r->items[i] = items[i];
        }
    }
    if (input_file_path){
        r->input_file_path_len = strlen(input_file_path);
        r->input_file_path = strdup(input_file_path);
    }
    if (output_file_path){
        r->output_file_path_len = strlen(output_file_path);
        r->output_file_path = strdup(output_file_path);
    }

    return r;
}

void freeRequest(REQUEST request){
    if(request){
        if(request->items){
            free(request->items);
        }
        if(request->input_file_path){
            free(request->input_file_path);
        }
        if(request->output_file_path){
            free(request->output_file_path);
        }
    }
}

pid_t getRequestSender(REQUEST request){
    return request->sender;
}

int getRequestType(REQUEST request){
    return request->request_type;
}

char *getRequestInputFilePath(REQUEST request){
    char *r = NULL;
    if(request && request->input_file_path_len){
        r = strdup(request->input_file_path);
    }
    return r;
}

char *getRequestOutputFilePath(REQUEST request){
    char *r = NULL;
    if(request && request->output_file_path_len){
        r = strdup(request->output_file_path);
    }
    return r;
}

int getRequestItemNumber(REQUEST request){
    return request->item_number;
}

int *getRequestItems(REQUEST request){
    int *r = calloc(request->item_number,sizeof(int));
    for (int i = 0; i < request->item_number; ++i) {
        r[i] = request->items[i];
    }
    return r;
}

ssize_t writeRequest(int fd , REQUEST request){
    ssize_t bytes_wrote = 0;
    bytes_wrote += write(fd,&request->sender,sizeof(int));
    bytes_wrote += write(fd,&request->request_type,sizeof(int));
    bytes_wrote += write(fd,&request->item_number,sizeof(int));
    bytes_wrote += write(fd,request->items,request->item_number*sizeof(int));
    bytes_wrote += write(fd,&request->input_file_path_len,sizeof(unsigned long));
    bytes_wrote += write(fd,&request->output_file_path_len,sizeof(unsigned long));
    bytes_wrote += write(fd,request->input_file_path,request->input_file_path_len*sizeof(char)+1);
    bytes_wrote += write(fd,request->output_file_path,request->output_file_path_len*sizeof(char)+1);
    return bytes_wrote;
}

ssize_t readStr(int fd , char * str , unsigned long size){
    ssize_t bytes_read = 0;
    for (int i = 0; i < size-1; ++i) {
        bytes_read += read(fd,&str[i], sizeof(char));
    }
    return bytes_read;
}

ssize_t readItems(int fd , int *items , int size){
    ssize_t bytes_read = 0;
    for (int i = 0; i < size; ++i) {
        bytes_read += read(fd,&items[i], sizeof(int));
    }
    return bytes_read;
}

ssize_t readRequest(int fd , REQUEST* request){
    *request = calloc(1, sizeof(struct request));
    ssize_t bytes_read = 0;
    bytes_read += read(fd, &(*request)->sender, sizeof(int));
    bytes_read += read(fd, &(*request)->request_type, sizeof(int));
    bytes_read += read(fd, &(*request)->item_number, sizeof(int));
    (*request)->items = calloc((*request)->item_number, sizeof(int));
    bytes_read += readItems(fd,(*request)->items,(*request)->item_number);
    bytes_read += read(fd, &(*request)->input_file_path_len, sizeof(unsigned long));
    bytes_read += read(fd, &(*request)->output_file_path_len, sizeof(unsigned long));
    (*request)->input_file_path = calloc((*request)->input_file_path_len+1, sizeof(char));
    (*request)->output_file_path = calloc((*request)->output_file_path_len+1, sizeof(char));
    bytes_read += readStr(fd,(*request)->input_file_path , (*request)->input_file_path_len+1);

    bytes_read += readStr(fd,(*request)->output_file_path , (*request)->output_file_path_len+1);
    return bytes_read;
}

REQUEST parseRequest(const char **argv , int argc){
    int *items = NULL;
    if(argc > 2){
        items = calloc(argc - 3, sizeof(int));
        for (int i = 4; i < argc; ++i) {
            items[i] = (int)strtol(argv[i] , NULL , 10);
        }
    }
    REQUEST r = newRequest(getpid(),
                           (int)parseOption(argv[1]),
                           argv[2],
                           argv[3],
                           argc,
                           items);
    return r;
}

