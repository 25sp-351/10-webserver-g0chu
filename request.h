#ifndef REQUEST_H
#define REQUEST_H

#include <stddef.h>

#define MAX_METHOD_LEN 10
#define MAX_URI_LEN 2048
#define MAX_VERSION_LEN 10
#define MAX_HEADERS 50
#define MAX_HEADER_NAME_LEN 256
#define MAX_HEADER_VALUE_LEN 1024

typedef struct {
    char name[MAX_HEADER_NAME_LEN];
    char value[MAX_HEADER_VALUE_LEN];
} HttpHeader;

typedef struct {
    char method[MAX_METHOD_LEN];
    char uri[MAX_URI_LEN];
    char version[MAX_VERSION_LEN];
    HttpHeader headers[MAX_HEADERS];
    int header_count;
} HttpRequest;

int parse_request(int sockfd, HttpRequest *req);

const char* get_request_header(const HttpRequest *req, const char *name);

#endif