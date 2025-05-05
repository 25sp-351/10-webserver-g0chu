# define _DEFAULT_SOURCE

#include "handler.h"
#include "response.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define STATIC_ROOT "./static"
#define MAX_PATH_LEN 4096

// Handle requests for static files under the /static/ path.
void handle_static_request(int sockfd, const HttpRequest *req) {
    char full_path[MAX_PATH_LEN];
    char safe_uri[MAX_URI_LEN];

    if (strstr(req->uri, "..")) {
        send_error_response(sockfd, 400, "Bad Request", "Invalid characters in URI.");
        return;
    }

    strncpy(safe_uri, req->uri, sizeof(safe_uri) - 1);
    safe_uri[sizeof(safe_uri) - 1] = '\0';

    const char *relative_path = safe_uri + strlen("/static");
    if (*relative_path == '/') {
        relative_path++;
    }

    int path_len = snprintf(full_path, sizeof(full_path), "%s/%s", STATIC_ROOT, relative_path);
    if (path_len < 0 || (size_t)path_len >= sizeof(full_path)) {
         send_error_response(sockfd, 500, "Internal Server Error", "File path too long.");
         return;
    }

    char resolved_path[PATH_MAX];
    if (realpath(full_path, resolved_path) == NULL) {
        if (errno == ENOENT) {
            send_error_response(sockfd, 404, "Not Found", "File not found.");
        } else if (errno == EACCES) {
             send_error_response(sockfd, 403, "Forbidden", "Permission denied.");
        } else {
            perror("realpath");
            send_error_response(sockfd, 500, "Internal Server Error", "Error resolving file path.");
        }
        return;
    }

    char resolved_static_root[PATH_MAX];
     if (realpath(STATIC_ROOT, resolved_static_root) == NULL) {
         perror("realpath static root");
         send_error_response(sockfd, 500, "Internal Server Error", "Cannot resolve static root directory.");
         return;
     }

    if (strncmp(resolved_path, resolved_static_root, strlen(resolved_static_root)) != 0) {
        send_error_response(sockfd, 403, "Forbidden", "Access denied (outside static root).");
        return;
    }

    struct stat st;
    if (stat(resolved_path, &st) == -1) {
        perror("stat");
        send_error_response(sockfd, 500, "Internal Server Error", "Could not stat file.");
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        send_error_response(sockfd, 403, "Forbidden", "Directory listing is not allowed.");
        return;
    }

    if (!S_ISREG(st.st_mode)) {
         send_error_response(sockfd, 403, "Forbidden", "Not a regular file.");
         return;
    }

    int filefd = open(resolved_path, O_RDONLY);
    if (filefd < 0) {
        perror("open");
        if (errno == EACCES) {
             send_error_response(sockfd, 403, "Forbidden", "Permission denied reading file.");
        } else {
             send_error_response(sockfd, 500, "Internal Server Error", "Could not open file.");
        }
        return;
    }

    HttpResponseInfo info = {0};
    info.status_code = 200;
    info.status_message = "OK";
    info.content_length = st.st_size;
    const char *mime_type = get_mime_type(resolved_path);
    strncpy(info.content_type, mime_type, sizeof(info.content_type) - 1);
    info.additional_headers[0] = '\0';

    if (send_response_header(sockfd, &info) < 0) {
        fprintf(stderr, "Failed to send static file header.\n");
        close(filefd);
        return;
    }

    if (send_file_response_body(sockfd, filefd, st.st_size) < 0) {
        fprintf(stderr, "Failed to send static file body.\n");
    }

    close(filefd);
}

// Handle calculation requests under the /calc/ path.
void handle_calc_request(int sockfd, const HttpRequest *req) {
    char op[10];
    double num1, num2;
    char remaining_path[MAX_URI_LEN];

    int parsed = sscanf(req->uri, "/calc/%9[^/]/%lf/%lf%s", op, &num1, &num2, remaining_path);

    if (parsed != 3) {
         if (parsed == 4 && strlen(remaining_path) > 0 && remaining_path[0] != '?') {
              send_error_response(sockfd, 400, "Bad Request", "Too many path components for /calc.");
              return;
         }
         send_error_response(sockfd, 400, "Bad Request", "Invalid format. Use /calc/[add|mul|div]/<num1>/<num2>");
         return;
    }

    double result;
    if (strcmp(op, "add") == 0) {
        result = num1 + num2;
    } else if (strcmp(op, "mul") == 0) {
        result = num1 * num2;
    } else if (strcmp(op, "div") == 0) {
        if (num2 == 0.0) {
            send_error_response(sockfd, 400, "Bad Request", "Division by zero is not allowed.");
            return;
        }
        result = num1 / num2;
    } else {
        send_error_response(sockfd, 404, "Not Found", "Invalid operation. Use 'add', 'mul', or 'div'.");
        return;
    }

    if (result == HUGE_VAL || result == -HUGE_VAL) {
        send_error_response(sockfd, 400, "Bad Request", "Calculation resulted in overflow/underflow.");
        return;
    }

    char body_buf[512];
    snprintf(body_buf, sizeof(body_buf),
             "<html><head><title>Calculation Result</title></head>"
             "<body><h1>Calculation Result</h1>"
             "<p>%.2f %s %.2f = <strong>%.4f</strong></p>"
             "</body></html>",
             num1,
             (strcmp(op,"add")==0) ? "+" : (strcmp(op,"mul")==0) ? "*" : "/",
             num2,
             result);

    send_simple_response(sockfd, 200, "OK", "text/html", body_buf);
}

// Route incoming requests to appropriate handlers based on method and URI.
void handle_request(int sockfd, const HttpRequest *req) {
    printf("Received Request: %s %s %s\n", req->method, req->uri, req->version);

    if (strcmp(req->method, "GET") != 0) {
        send_error_response(sockfd, 405, "Method Not Allowed", "Only GET method is supported for this resource.");
        return;
    }

    if (strncmp(req->uri, "/static/", 8) == 0) {
        handle_static_request(sockfd, req);
    } else if (strncmp(req->uri, "/calc/", 6) == 0) {
        handle_calc_request(sockfd, req);
    } else if (strcmp(req->uri, "/") == 0 || strcmp(req->uri, "/index.html") == 0) {
        HttpRequest fake_req = *req;
        strncpy(fake_req.uri, "/static/index.html", sizeof(fake_req.uri)-1);
        fake_req.uri[sizeof(fake_req.uri)-1] = '\0';
        handle_static_request(sockfd, &fake_req);
    }
    else {
        send_error_response(sockfd, 404, "Not Found", "The requested resource was not found on this server.");
    }
}