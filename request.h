//
// Created by Sandro Duarte on 11/05/2022.
//

#ifndef TP_SO_2122_SDSTORE_REQUEST_H
#define TP_SO_2122_SDSTORE_REQUEST_H

typedef struct request *REQUEST;

REQUEST newRequest(int sender , int request_type,
                   const char *input_file_path , const char *output_file_path ,int item_number , const int *items);
pid_t getRequestSender(REQUEST request);
int getRequestType(REQUEST request);
char *getRequestInputFilePath(REQUEST request);
char *getRequestOutputFilePath(REQUEST request);
int getRequestItemNumber(REQUEST request);
int *getRequestItems(REQUEST request);
ssize_t writeRequest(int fd , REQUEST request);
ssize_t readRequest(int fd , REQUEST* request);
void freeRequest(REQUEST request);
REQUEST parseRequest(const char **argv , int argc);

#endif //TP_SO_2122_SDSTORE_REQUEST_H
