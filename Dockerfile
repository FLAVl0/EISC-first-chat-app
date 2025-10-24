# Stage 1: Build JS assets
FROM node:20-alpine AS js-build

# Set working directory and copy source files
WORKDIR /js
COPY www/ts ./ts

# Install dependencies and build the TypeScript files to executable JavaScript
RUN npm install --prefix ./ts
RUN ./ts/node_modules/.bin/tsc --project ./ts/tsconfig.json

# Stage 2: Build C server
FROM alpine:3.22

RUN apk add --no-cache gcc musl-dev sqlite-dev

# Copy built JS assets from the first stage as well as C source files
WORKDIR /app
COPY src/ ./src
COPY www/ ./www
COPY --from=js-build /js/ts/dist ./www/dist

# Compile the C server
RUN gcc src/*.c -o server -lpthread -lsqlite3

# Expose the server port and set the command to run the server
EXPOSE 8080
CMD ["./server"]