# ==========================================================================
# Forza 4 Client Dockerfile
# ==========================================================================

FROM python:3.11-slim

WORKDIR /app

# Copy client files
COPY client.py .
COPY requirements.txt .

# Install dependencies (if any)
RUN pip install --no-cache-dir -r requirements.txt

# Make the script executable
RUN chmod +x client.py

# Environment variables
ENV SERVER_HOST=server
ENV SERVER_PORT=8080
ENV PYTHONUNBUFFERED=1

# Run the client
CMD ["python", "-u", "client.py"]
