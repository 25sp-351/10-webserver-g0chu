TARGET = http_server

SRCS = main.c request.c response.c handler.c utils.c

OBJS = $(SRCS:.c=.o)

CC = gcc

CFLAGS = -Wall -Wextra -g -pthread -std=c11
LDFLAGS = -pthread -lm

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

main.o: main.c request.h response.h handler.h
request.o: request.c request.h
response.o: response.c response.h
handler.o: handler.c handler.h request.h response.h utils.h
utils.o: utils.c utils.h

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean