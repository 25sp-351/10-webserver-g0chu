#ifndef RESPONSE_H
#define RESPONSE_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
    int status_code;
    const char *status_message;
    char content_type[128];
    off_t content_length;

    char additional_headers[1024];
} HttpResponseInfo;

int send_response_header(int sockfd, const HttpResponseInfo *info);

int send_response_body(int sockfd, const char *body);

int send_simple_response(int sockfd, int status_code, const char *status_message, const char *content_type, const char *body);

int send_error_response(int sockfd, int status_code, const char *status_message, const char *details);

int send_file_response_body(int sockfd, int filefd, off_t file_size);


#endif