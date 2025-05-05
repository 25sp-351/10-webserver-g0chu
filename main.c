#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>

#include "request.h"
#include "handler.h"
#include "response.h"

#define DEFAULT_PORT 80
#define MAX_CONNECTIONS 10

volatile sig_atomic_t server_fd = -1;

// Signal handler to close listening socket and exit cleanly.
void handle_shutdown(int sig) {
    (void)sig;
    printf("\nShutting down server...\n");
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }
    exit(0);
}

// Thread function to handle one client connection: parse request, handle, respond, close.
void *client_handler_thread(void *arg) {
    pthread_detach(pthread_self());

    int client_sockfd = (int)(intptr_t)arg;

    printf("Thread %lu: Handling connection on socket %d\n", pthread_self(), client_sockfd);

    HttpRequest req;
    int parse_status = parse_request(client_sockfd, &req);

    if (parse_status == 0) {
        handle_request(client_sockfd, &req);
    } else if (parse_status == -1) {
        fprintf(stderr, "Thread %lu: Failed to parse request from socket %d\n", pthread_self(), client_sockfd);
        send_error_response(client_sockfd, 400, "Bad Request", "Could not parse the request.");
    } else if (parse_status == -2) {
         fprintf(stderr, "Thread %lu: Client disconnected on socket %d during request read.\n", pthread_self(), client_sockfd);
    }

    printf("Thread %lu: Closing connection on socket %d\n", pthread_self(), client_sockfd);
    close(client_sockfd);
    return NULL;
}

// Setup socket, bind, listen, accept connections, and dispatch threads.
int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    int opt;

    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [-p port]\n", argv[0]);
                return 1;
        }
    }

    struct sockaddr_in server_addr;
    int sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return 1;
    }
    server_fd = sockfd;

    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return 1;
    }

    if (listen(sockfd, MAX_CONNECTIONS) < 0) {
        perror("listen failed");
        close(sockfd);
        return 1;
    }

    printf("Server listening on port %d...\n", port);

    signal(SIGINT, handle_shutdown);
    signal(SIGTERM, handle_shutdown);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sockfd;

    while (1) {
        printf("Waiting for new connection...\n");
        client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);

        if (client_sockfd < 0) {
            if (errno == EINTR && server_fd == -1) {
                 printf("Accept interrupted by shutdown signal.\n");
                 break;
            }
            perror("accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Accepted connection from %s:%d on socket %d\n", client_ip, ntohs(client_addr.sin_port), client_sockfd);

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler_thread, (void *)(intptr_t)client_sockfd) != 0) {
            perror("pthread_create failed");
            close(client_sockfd);
        }
    }

    if (server_fd != -1) {
        close(server_fd);
    }
    printf("Server shutdown complete.\n");
    return 0;
}