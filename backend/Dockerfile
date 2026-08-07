FROM python:3.11-slim

WORKDIR /app

# Install dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy the rest of the backend code
COPY . .

# Expose the REST/WebSocket port and UDP telemetry port
EXPOSE 3000
EXPOSE 8080/udp

CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "3000"]
