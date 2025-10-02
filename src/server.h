#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sqlite3.h>

void send_file(int client_fd, const char *path, const char *content_type);
void init_db();
void *handle_client(void *arg);

#endif // SERVER_H