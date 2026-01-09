# ==========================================================================
# Forza 4 Server Dockerfile
# ==========================================================================

FROM gcc:latest

# Install netcat for healthcheck
RUN apt-get update && apt-get install -y netcat-openbsd && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy all source code
COPY server.c server.h ./
COPY include/ include/
COPY src/ src/

# Compile the server
RUN gcc -o server server.c src/server_utils.c src/server_game_logic.c src/server_game_management.c src/server_handlers.c -lpthread -Wall -Wextra -O2

# Expose server port
EXPOSE 8080

# Run server
CMD ["./server", "8080"]
