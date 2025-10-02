/*
Main server file
*/

// Libraries to handle signal and atomic operations for Docker compatibility and graceful shutdown
#include <signal.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

// Standard libraries, functions for networking, threading, and SQLite prototypes
#include "server.h"

volatile sig_atomic_t keep_running = 1;

void handle_sigterm(int sig)
{
	keep_running = 0;
}

int main(void)
{
	signal(SIGTERM, handle_sigterm);
	signal(SIGINT, handle_sigterm);

	init_db();

	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0)
	{
		perror("socket");
		exit(1);
	}

	int opt = 1;
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	addr.sin_addr.s_addr = INADDR_ANY;
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		exit(1);
	}

	if (listen(listen_fd, 10) < 0)
	{
		perror("listen");
		exit(1);
	}

	// Set socket to non-blocking
	fcntl(listen_fd, F_SETFL, O_NONBLOCK);

	printf("Waiting for connections on :8080...\n");

	while (keep_running)
	{
		struct sockaddr_in clientAddr;
		socklen_t clientAddrLen = sizeof(clientAddr);
		int client_fd = accept(listen_fd, (struct sockaddr *)&clientAddr, &clientAddrLen);
		if (client_fd < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				usleep(100000); // Sleep 100ms to avoid busy loop
				continue;
			}
			perror("accept");
			continue;
		}

		struct client_info
		{
			int fd;
			char ip[INET_ADDRSTRLEN];
		} *info = malloc(sizeof(struct client_info));

		info->fd = client_fd;
		inet_ntop(AF_INET, &clientAddr.sin_addr, info->ip, sizeof(info->ip));

		pthread_t tid;
		if (pthread_create(&tid, NULL, handle_client, info) != 0)
		{
			perror("pthread_create");
			close(client_fd);
			free(info);
			continue;
		}

		pthread_detach(tid);
	}

	close(listen_fd);
	return 0;
}
