#include "response.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <sys/types.h>

#define WRITE_BUFFER_SIZE 4096

// Reliably send a buffer of data over a socket.
ssize_t send_all(int sockfd, const char *buf, size_t len) {
    size_t total_sent = 0;
    ssize_t n;
    while (total_sent < len) {
        n = send(sockfd, buf + total_sent, len - total_sent, 0);
        if (n == -1) {
            if (errno == EINTR) continue;
            perror("send");
            return -1;
        }
         if (n == 0) {
             fprintf(stderr, "send returned 0, connection may be closed.\n");
             return -1;
         }
        total_sent += n;
    }
    return total_sent;
}

// Format and send the HTTP response status line and headers.
int send_response_header(int sockfd, const HttpResponseInfo *info) {
    char header_buf[WRITE_BUFFER_SIZE];
    char time_buf[128];
    time_t now = time(0);
    struct tm *tm = gmtime(&now);

    strftime(time_buf, sizeof(time_buf), "%a, %d %b %Y %H:%M:%S GMT", tm);

    int len = snprintf(header_buf, sizeof(header_buf),
                       "HTTP/1.1 %d %s\r\n"
                       "Server: basic-c-server/1.0\r\n"
                       "Date: %s\r\n"
                       "Content-Type: %s\r\n"
                       "Content-Length: %ld\r\n"
                       "Connection: close\r\n"
                       "%s"
                       "\r\n",
                       info->status_code, info->status_message,
                       time_buf,
                       info->content_type[0] ? info->content_type : "application/octet-stream",
                       (long)info->content_length,
                       info->additional_headers ? info->additional_headers : ""
                       );

    if (len < 0 || (size_t)len >= sizeof(header_buf)) {
        fprintf(stderr, "Error formatting response header or buffer too small.\n");
        return -1;
    }

    if (send_all(sockfd, header_buf, len) < 0) {
        return -1;
    }

    return 0;
}

// Send the response body content (string).
int send_response_body(int sockfd, const char *body) {
    if (!body) return 0;
    size_t body_len = strlen(body);
    if (body_len > 0) {
        if (send_all(sockfd, body, body_len) < 0) {
            return -1;
        }
    }
    return 0;
}

// Send a complete simple response (header + body string).
int send_simple_response(int sockfd, int status_code, const char *status_message, const char *content_type, const char *body) {
    HttpResponseInfo info = {0};
    info.status_code = status_code;
    info.status_message = status_message;
    strncpy(info.content_type, content_type ? content_type : "text/plain", sizeof(info.content_type) - 1);
    info.content_length = body ? strlen(body) : 0;
    info.additional_headers[0] = '\0';

    if (send_response_header(sockfd, &info) < 0) {
        return -1;
    }
    if (send_response_body(sockfd, body) < 0) {
        return -1;
    }
    return 0;
}

// Format and send an HTML error response.
int send_error_response(int sockfd, int status_code, const char *status_message, const char *details) {
    char body_buf[512];
    snprintf(body_buf, sizeof(body_buf),
             "<html><head><title>%d %s</title></head>"
             "<body><h1>%d %s</h1><p>%s</p></body></html>",
             status_code, status_message,
             status_code, status_message, details ? details : "");

    return send_simple_response(sockfd, status_code, status_message, "text/html", body_buf);
}

// Send file content using read/write loop.
int send_file_response_body(int sockfd, int filefd, off_t file_size) {
    off_t offset = 0;
    ssize_t sent_bytes;

    while (offset < file_size) {
        char buffer[WRITE_BUFFER_SIZE];

        ssize_t bytes_to_read = (file_size - offset < (off_t)sizeof(buffer)) ? (file_size - offset) : (off_t)sizeof(buffer);
        ssize_t bytes_read = read(filefd, buffer, bytes_to_read);

        if (bytes_read == -1) {
            perror("read file");
            return -1;
        }
        if (bytes_read == 0) {
            fprintf(stderr, "Unexpected EOF while reading file.\n");
            return -1;
        }

        sent_bytes = send_all(sockfd, buffer, bytes_read);
        if (sent_bytes == -1) {
             fprintf(stderr, "Failed to send file chunk.\n");
             return -1;
        }
         if (sent_bytes != bytes_read) {
              fprintf(stderr, "Incomplete send during file transfer.\n");
              return -1;
         }

        offset += sent_bytes;
    }

    return 0;
}