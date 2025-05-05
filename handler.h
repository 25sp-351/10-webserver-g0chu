#ifndef HANDLER_H
#define HANDLER_H

#include "request.h"

void handle_request(int sockfd, const HttpRequest *req);

#endif