FROM alpine:3.22

# Install build tools
RUN apk add --no-cache gcc musl-dev sqlite-dev

# Set working directory
WORKDIR /app

# Copy source and static files
COPY src/ ./src
COPY www/ ./www

# Build the server
RUN gcc src/*.c -o server -lpthread -lsqlite3

# Expose the chat server port
EXPOSE 8080

# Run the server
CMD ["./server"]