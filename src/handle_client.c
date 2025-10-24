#include "server.h"

void *handle_client(void *arg)
{
	const char *bad_request =
		"HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 11\r\n\r\nBad Request";

	struct client_info
	{
		int fd;
		char ip[INET_ADDRSTRLEN];
	} *info = arg;

	int client_fd = info->fd;
	char client_ip[INET_ADDRSTRLEN];
	strncpy(client_ip, info->ip, INET_ADDRSTRLEN);
	free(info);

	char buffer[2048];
	bool running = true;

	printf("Accepted connection from %s\n", client_ip);

	while (running)
	{
		ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

		if (n <= 0)
		{
			printf("Client disconnected\n");
			break;
		}

		buffer[n] = '\0';

		// Parse HTTP method and path
		char method[8], path[128];
		sscanf(buffer, "%7s %127s", method, path);

		// Extract forwarded IP if present
		char forwarded_ip[INET_ADDRSTRLEN] = "";
		char *xff = strstr(buffer, "X-Forwarded-For: ");

		if (xff)
		{
			xff += strlen("X-Forwarded-For: ");
			sscanf(xff, "%15s", forwarded_ip);
			strncpy(client_ip, forwarded_ip, INET_ADDRSTRLEN);
		}

		// Extract device_id from Cookie header if present
		char device_id[64] = "";
		char *cookie_ptr = strstr(buffer, "Cookie: ");

		if (cookie_ptr)
		{
			char *id_ptr = strstr(cookie_ptr, "device_id=");

			if (id_ptr)
			{
				id_ptr += strlen("device_id=");
				char *end = strpbrk(id_ptr, "; \r\n");
				size_t len = end ? (size_t)(end - id_ptr) : strlen(id_ptr);
				strncpy(device_id, id_ptr, len);
				device_id[len] = '\0';
			}
		}

		// Extract username from Cookie header if present
		char username[64] = "";
		cookie_ptr = strstr(buffer, "Cookie: ");

		if (cookie_ptr)
		{
			char *user_ptr = strstr(cookie_ptr, "username=");

			if (user_ptr)
			{
				user_ptr += strlen("username=");
				char *end = strpbrk(user_ptr, "; \r\n");
				size_t len = end ? (size_t)(end - user_ptr) : strlen(user_ptr);
				strncpy(username, user_ptr, len);
				username[len] = '\0';
			}
		}

		// If device_id not present, generate one and set cookie in response
		bool set_cookie = false;

		if (strlen(device_id) == 0)
		{
			snprintf(device_id, sizeof(device_id), "dev%ld", random() % 1000000000);
			set_cookie = true;
		}

		if (strcmp(method, "GET") == 0)
		{
			// Extract path without query string
			char clean_path[128];
			strncpy(clean_path, path, sizeof(clean_path) - 1);
			clean_path[sizeof(clean_path) - 1] = '\0';
			char *qmark = strchr(clean_path, '?');

			if (qmark)
				*qmark = '\0';

			if (strcmp(clean_path, "/") == 0)
			{
				// Check for username cookie
				bool has_username = false;
				char *cookie_ptr = strstr(buffer, "Cookie: ");
				if (cookie_ptr)
				{
					char *user_ptr = strstr(cookie_ptr, "username=");

					if (user_ptr)
						has_username = true;
				}

				if (has_username)
					send_file(client_fd, "www/pages/index.html", "text/html");

				else
					send_file(client_fd, "www/pages/username.html", "text/html");
			}
			// NEW: serve the username page explicitly
			else if (strcmp(clean_path, "/username") == 0)
			{
				send_file(client_fd, "www/pages/username.html", "text/html");
			}
			// Serve any CSS file in www/styles/
			else if (strncmp(clean_path, "/styles/", 8) == 0 && strstr(clean_path, ".css"))
			{
				char file_path[256];
				snprintf(file_path, sizeof(file_path), "www%s", clean_path);
				send_file(client_fd, file_path, "text/css");
			}

			// Serve any JS file in www/scripts/
			else if (strncmp(clean_path, "/dist/", 9) == 0 && strstr(clean_path, ".js"))
			{
				char file_path[256];
				snprintf(file_path, sizeof(file_path), "www%s", clean_path);
				send_file(client_fd, file_path, "application/javascript");
			}

			else if (strcmp(clean_path, "/messages") == 0)
			{
				// Parse ?since=... from path
				char *since = NULL;
				char *qmark = strchr(buffer, '?');

				if (qmark)
				{
					char *since_ptr = strstr(qmark, "since=");

					if (since_ptr)
					{
						since_ptr += 6;
						char *end = strpbrk(since_ptr, "& \r\n");
						size_t len = end ? (size_t)(end - since_ptr) : strlen(since_ptr);
						since = malloc(len + 1);
						strncpy(since, since_ptr, len);
						since[len] = '\0';
					}
				}

				sqlite3 *db;
				if (sqlite3_open("chat.db", &db) == SQLITE_OK)
				{
					char sql[256];

					if (since && strlen(since) > 0)
					{
						snprintf(sql, sizeof(sql),
								 "SELECT user, text, strftime('%%s', ts) * 1000 FROM messages WHERE ts > ? ORDER BY id ASC;");
					}
					else
					{
						snprintf(sql, sizeof(sql),
								 "SELECT user, text, strftime('%%s', ts) * 1000 FROM messages ORDER BY id ASC;");
					}

					sqlite3_stmt *stmt;
					char json[8192] = "[";
					bool first = true;

					if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
					{
						if (since && strlen(since) > 0)
							sqlite3_bind_text(stmt, 1, since, -1, SQLITE_TRANSIENT);

						while (sqlite3_step(stmt) == SQLITE_ROW)
						{
							if (!first)
								strcat(json, ",");

							first = false;
							char entry[512];
							snprintf(entry, sizeof(entry),
									 "{\"user\":\"%s\",\"text\":\"%s\",\"ts\":%lld}",
									 sqlite3_column_text(stmt, 0),
									 sqlite3_column_text(stmt, 1),
									 sqlite3_column_int64(stmt, 2));
							strcat(json, entry);
						}
						sqlite3_finalize(stmt);
					}

					strcat(json, "]");
					sqlite3_close(db);

					if (since)
						free(since);

					char header[256];
					snprintf(header, sizeof(header),
							 "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n",
							 strlen(json));
					send(client_fd, header, strlen(header), 0);
					send(client_fd, json, strlen(json), 0);
				}
				else
				{
					const char *not_found =
						"HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nDB error";
					send(client_fd, not_found, strlen(not_found), 0);
				}
			}
			else
			{
				const char *not_found =
					"HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nNot found";
				send(client_fd, not_found, strlen(not_found), 0);
			}
		}
		else if (strcmp(method, "POST") == 0 && strcmp(path, "/messages") == 0)
		{
			// Locate body
			char *body = strstr(buffer, "\r\n\r\n");

			if (body)
				body += 4;

			else
				body = "";

			// Find the "text" field in the incoming JSON
			char *text_ptr = strstr(body, "\"text\"");

			if (!text_ptr)
			{
				send(client_fd, bad_request, strlen(bad_request), 0);
				continue;
			}

			// Extract the text value from the JSON
			char text_val[512];

			if (sscanf(text_ptr, "\"text\":\"%511[^\"]\"", text_val) != 1)
			{
				send(client_fd, bad_request, strlen(bad_request), 0);
				continue;
			}

			// Extract the ip value from the JSON if present
			char ip_val[INET_ADDRSTRLEN] = "";
			char *ip_ptr = strstr(body, "\"ip\"");

			if (ip_ptr)
			{
				sscanf(ip_ptr, "\"ip\":\"%15[^\"]\"", ip_val);
				if (strlen(ip_val) > 0)
				{
					strncpy(client_ip, ip_val, INET_ADDRSTRLEN);
				}
			}

			// Extract device_id from JSON if present
			char dev_val[64] = "";
			char *dev_ptr = strstr(body, "\"device_id\"");

			if (dev_ptr)
			{
				sscanf(dev_ptr, "\"device_id\":\"%63[^\"]\"", dev_val);

				if (strlen(dev_val) > 0)
					strncpy(device_id, dev_val, sizeof(device_id));
			}

			// Save to database
			sqlite3 *db;
			if (sqlite3_open("chat.db", &db) == SQLITE_OK)
			{
				// Resolve effective username from users table by device_id; fallback to cookie or IP
				char effective_user[64] = "";
				sqlite3_stmt *sel = NULL;
				if (sqlite3_prepare_v2(db, "SELECT username FROM users WHERE device_id=? LIMIT 1;", -1, &sel, 0) == SQLITE_OK)
				{
					sqlite3_bind_text(sel, 1, device_id, -1, SQLITE_TRANSIENT);
					if (sqlite3_step(sel) == SQLITE_ROW)
					{
						const unsigned char *u = sqlite3_column_text(sel, 0);
						if (u) {
							strncpy(effective_user, (const char *)u, sizeof(effective_user) - 1);
							effective_user[sizeof(effective_user) - 1] = '\0';
						}
					}
					sqlite3_finalize(sel);
				}
				if (effective_user[0] == '\0')
				{
					if (strlen(username) > 0) {
						strncpy(effective_user, username, sizeof(effective_user) - 1);
						effective_user[sizeof(effective_user) - 1] = '\0';
					} else {
						strncpy(effective_user, client_ip, sizeof(effective_user) - 1);
						effective_user[sizeof(effective_user) - 1] = '\0';
					}
				}

				sqlite3_stmt *stmt;
				const char *sql = "INSERT INTO messages (user, text, device_id) VALUES (?, ?, ?);";
				if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK)
				{
					sqlite3_bind_text(stmt, 1, effective_user, -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(stmt, 2, text_val, -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(stmt, 3, device_id, -1, SQLITE_TRANSIENT);
					sqlite3_step(stmt);
					sqlite3_finalize(stmt);
				}

				sqlite3_close(db);
			}

			// Proper response with Content-Length
			const char *resp_body = "{\"status\":\"ok\"}";
			char ok[512];
			size_t header_len = 0;

			if (set_cookie)
			{
				header_len = snprintf(ok, sizeof(ok),
									  "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nSet-Cookie: device_id=%s; Path=/; HttpOnly\r\n\r\n",
									  strlen(resp_body), device_id);
			}
			else
			{
				header_len = snprintf(ok, sizeof(ok),
									  "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n",
									  strlen(resp_body));
			}

			memcpy(ok + header_len, resp_body, strlen(resp_body));
			send(client_fd, ok, header_len + strlen(resp_body), 0);
		}
		
		// NEW: handle username updates
		else if (strcmp(method, "POST") == 0 && strcmp(path, "/username") == 0)
		{
			// Locate body
			char *body = strstr(buffer, "\r\n\r\n");
			if (body) body += 4; else body = "";

			// Extract "username" from JSON
			char uname_val[64] = "";
			char *u_ptr = strstr(body, "\"username\"");
			if (!u_ptr || sscanf(u_ptr, "\"username\"%*[^\"]\"%63[^\"]\"", uname_val) != 1 || strlen(uname_val) == 0)
			{
				send(client_fd, bad_request, strlen(bad_request), 0);
				continue;
			}

			// Persist username for this device_id and update past messages
			sqlite3 *db;
			if (sqlite3_open("chat.db", &db) == SQLITE_OK)
			{
				// Upsert into users
				const char *upsert_sql =
					"INSERT INTO users (device_id, username) VALUES (?, ?)"
					"ON CONFLICT(device_id) DO UPDATE SET username=excluded.username;";

				sqlite3_stmt *stmt1 = NULL;
				if (sqlite3_prepare_v2(db, upsert_sql, -1, &stmt1, 0) == SQLITE_OK)
				{
					sqlite3_bind_text(stmt1, 1, device_id, -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(stmt1, 2, uname_val, -1, SQLITE_TRANSIENT);
					sqlite3_step(stmt1);
					sqlite3_finalize(stmt1);
				}

				// Update all past messages for this device_id to reflect new username
				const char *update_msgs = "UPDATE messages SET user=? WHERE device_id=?;";
				sqlite3_stmt *stmt2 = NULL;
				if (sqlite3_prepare_v2(db, update_msgs, -1, &stmt2, 0) == SQLITE_OK)
				{
					sqlite3_bind_text(stmt2, 1, uname_val, -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(stmt2, 2, device_id, -1, SQLITE_TRANSIENT);
					sqlite3_step(stmt2);
					sqlite3_finalize(stmt2);
				}

				sqlite3_close(db);
			}
			else
			{
				const char *err =
					"HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nDB error";
				send(client_fd, err, strlen(err), 0);
				continue;
			}

			// Respond with JSON and set cookies
			const char *resp_body = "{\"status\":\"ok\"}";
			char ok[768];
			size_t header_len = 0;

			if (set_cookie)
			{
				header_len = snprintf(ok, sizeof(ok),
					"HTTP/1.1 200 OK\r\n"
					"Content-Type: application/json\r\n"
					"Content-Length: %zu\r\n"
					"Set-Cookie: device_id=%s; Path=/; HttpOnly\r\n"
					"Set-Cookie: username=%s; Path=/; HttpOnly\r\n"
					"\r\n",
					strlen(resp_body), device_id, uname_val);
			}
			else
			{
				header_len = snprintf(ok, sizeof(ok),
					"HTTP/1.1 200 OK\r\n"
					"Content-Type: application/json\r\n"
					"Content-Length: %zu\r\n"
					"Set-Cookie: username=%s; Path=/; HttpOnly\r\n"
					"\r\n",
					strlen(resp_body), uname_val);
			}

			memcpy(ok + header_len, resp_body, strlen(resp_body));
			send(client_fd, ok, header_len + strlen(resp_body), 0);
		}
		else
			send(client_fd, bad_request, strlen(bad_request), 0);
	}

	close(client_fd);
	return NULL;
}