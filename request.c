#define _GNU_SOURCE

#include "request.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/socket.h>
#include <strings.h>

#define READ_BUFFER_SIZE 4096

// Read a single line (ending in \n or \r\n) from a socket.
ssize_t read_line(int sockfd, char *buf, size_t size) {
    size_t i = 0;
    char c = '\0';
    ssize_t n;

    while (i < size - 1 && c != '\n') {
        n = recv(sockfd, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                n = recv(sockfd, &c, 1, MSG_PEEK);
                if (n > 0 && c == '\n') {
                    recv(sockfd, &c, 1, 0);
                }
                c = '\n';
            }
            buf[i++] = c;
        } else if (n == 0) {
            if (i == 0) return 0;
            break;
        } else {
            perror("recv");
            return -1;
        }
    }
    buf[i] = '\0';
    return i;
}

// Parse an HTTP request (request line and headers) from a socket.
int parse_request(int sockfd, HttpRequest *req) {
    char line_buffer[READ_BUFFER_SIZE];
    ssize_t nread;

    memset(req, 0, sizeof(HttpRequest));

    nread = read_line(sockfd, line_buffer, sizeof(line_buffer));
    if (nread <= 0) {
        fprintf(stderr, "Failed to read request line or client disconnected.\n");
        return (nread == 0) ? -2 : -1;
    }

    if (sscanf(line_buffer, "%9s %2047s %9s", req->method, req->uri, req->version) != 3) {
        fprintf(stderr, "Malformed request line: %s\n", line_buffer);
        return -1;
    }

    if (strncmp(req->version, "HTTP/1.1", 8) != 0 && strncmp(req->version, "HTTP/1.0", 8) != 0) {
         fprintf(stderr, "Warning: Unsupported HTTP version: %s\n", req->version);
    }

    req->header_count = 0;
    while (req->header_count < MAX_HEADERS) {
        nread = read_line(sockfd, line_buffer, sizeof(line_buffer));
        if (nread < 0) {
            fprintf(stderr, "Failed to read headers.\n");
            return -1;
        }
        if (nread == 0) {
             fprintf(stderr, "Client disconnected unexpectedly during header reading.\n");
             return -2;
        }

        if (strcmp(line_buffer, "\n") == 0) {
            break;
        }

        char *colon = strchr(line_buffer, ':');
        if (colon == NULL) {
            fprintf(stderr, "Malformed header line: %s\n", line_buffer);
            continue;
        }

        *colon = '\0';
        char *value_start = colon + 1;

        while (*value_start && isspace((unsigned char)*value_start)) {
            value_start++;
        }

        char *value_end = value_start + strlen(value_start) - 1;
        while (value_end >= value_start && isspace((unsigned char)*value_end)) {
            *value_end-- = '\0';
        }

        strncpy(req->headers[req->header_count].name, line_buffer, MAX_HEADER_NAME_LEN - 1);
        req->headers[req->header_count].name[MAX_HEADER_NAME_LEN - 1] = '\0';

        strncpy(req->headers[req->header_count].value, value_start, MAX_HEADER_VALUE_LEN - 1);
        req->headers[req->header_count].value[MAX_HEADER_VALUE_LEN - 1] = '\0';

        req->header_count++;
    }

    if (req->header_count >= MAX_HEADERS) {
        fprintf(stderr, "Warning: Too many headers received (max %d).\n", MAX_HEADERS);
    }

    return 0;
}

// Retrieve the value of a specific header (case-insensitive).
const char* get_request_header(const HttpRequest *req, const char *name) {
    for (int i = 0; i < req->header_count; ++i) {
        if (strcasecmp(req->headers[i].name, name) == 0) {
            return req->headers[i].value;
        }
    }
    return NULL;
}