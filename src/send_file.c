#include "server.h"

void send_file(int client_fd, const char *path, const char *content_type)
{
	int fd = open(path, O_RDONLY);

	if (fd < 0)
	{
		const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile not found";
		send(client_fd, not_found, strlen(not_found), 0);
		return;
	}

	struct stat st;
	fstat(fd, &st);
	char header[256];
	snprintf(header, sizeof(header),
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %ld\r\n\r\n",
		content_type, (long)st.st_size
	);
	send(client_fd, header, strlen(header), 0);

	char buf[1024];
	ssize_t n;
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		send(client_fd, buf, (unsigned long)n, 0);

	close(fd);
}