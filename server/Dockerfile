# ==========================================================================
# Forza 4 Server Dockerfile
# Multi-threaded C TCP Server
# ==========================================================================

FROM gcc:latest

# Install netcat for healthcheck
RUN apt-get update && apt-get install -y netcat-openbsd && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source code
COPY server.c .

# Compile the server with pthread support and optimizations
RUN gcc -o server server.c -lpthread -Wall -Wextra -O2

# Expose the server port
EXPOSE 8080

# Run the server
CMD ["./server", "8080"]
