# Chat-App Server

## Launching the Server

To build and run the server using Docker, execute the following command in the project directory:

```bash
docker build -t chat-app . && docker run -itd -p HOST_PORT:8080 --rm --name chat-app chat-app
```

- `docker build -t chat-app .` builds the Docker image and tags it as `chat-app`.
- `docker run -itd -p HOST_PORT:8080 --rm --name chat-app chat-app` runs the container in detached mode, mapping port `HOST_PORT` on the host to port 8080 in the container.

The server will be accessible at `http://localhost:8080`.

## Accessing the server from a web browser

Open your web browser and navigate to `http://IP_ADDRESS:HOST_PORT`, replacing `IP_ADDRESS` with the actual IP address of the machine running the Docker container and `HOST_PORT` with the port you specified when running the container. If you're running it locally, you can use `localhost` or `127.0.0.1` (loopback IP address, common for testing).

On the first visit, you will be prompted to enter a username. After entering your username, you can start sending and receiving messages in the chat application.