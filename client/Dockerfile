# ==========================================================================
# Forza 4 Client Dockerfile
# ==========================================================================

FROM python:3.11-slim

WORKDIR /app

# Copy client files
COPY client.py .
COPY requirements.txt .

# Install dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Chmod to execute
RUN chmod +x client.py

# Environment
ENV SERVER_HOST=server
ENV SERVER_PORT=8080
ENV PYTHONUNBUFFERED=1

# Run client
CMD ["python", "-u", "client.py"]
