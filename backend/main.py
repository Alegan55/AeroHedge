import struct
import asyncio
import os
import asyncpg
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from contextlib import asynccontextmanager

DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://postgres:postgres@localhost:5432/aerohedge")
db_pool = None

# Global state to hold the latest metrics from the C++ engine
latest_metrics = {
    "status": "waiting_for_data",
    "timestamp_ns": 0,
    "ticks_processed": 0,
    "avg_latency_cycles": 0
}

class TelemetryUDPProtocol(asyncio.DatagramProtocol):
    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, addr):
        if len(data) == 24:
            unpacked = struct.unpack('<QQQ', data)
            latest_metrics["status"] = "live"
            latest_metrics["timestamp_ns"] = unpacked[0]
            latest_metrics["ticks_processed"] = unpacked[1]
            latest_metrics["avg_latency_cycles"] = unpacked[2]
            print(f"⚡ FASTPATH UDP IN -> Ticks: {unpacked[1]} | Latency: {unpacked[2]} cycles")

            # Asynchronously queue database write if pool is available
            if db_pool:
                asyncio.create_task(self.save_metric(unpacked[0], unpacked[1], unpacked[2]))

    async def save_metric(self, ts, ticks, latency):
        try:
            async with db_pool.acquire() as connection:
                await connection.execute(
                    "INSERT INTO telemetry_logs (timestamp_ns, ticks_processed, avg_latency_cycles) VALUES ($1, $2, $3)",
                    ts, ticks, latency
                )
        except Exception as e:
            print(f"DB Error: {e}")

@asynccontextmanager
async def lifespan(app: FastAPI):
    global db_pool
    # Initialize DB connection pool
    try:
        await asyncio.sleep(3)
        db_pool = await asyncpg.create_pool(DATABASE_URL)
        async with db_pool.acquire() as connection:
            await connection.execute("""
                CREATE TABLE IF NOT EXISTS telemetry_logs (
                    id SERIAL PRIMARY KEY,
                    timestamp_ns BIGINT,
                    ticks_processed BIGINT,
                    avg_latency_cycles BIGINT,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                );
            """)
        print("Connected to PostgreSQL & ensured table exists.")
    except Exception as e:
        print(f"Failed to connect to database: {e}")

    loop = asyncio.get_running_loop()
    transport, protocol = await loop.create_datagram_endpoint(
        lambda: TelemetryUDPProtocol(),
        local_addr=('0.0.0.0', 8080)
    )
    print("UDP Telemetry Listener bound to 0.0.0.0:8080")
    
    yield
    
    if db_pool:
        await db_pool.close()
    transport.close()

app = FastAPI(title="AeroHedge Telemetry Gateway", lifespan=lifespan)

@app.get("/api/v1/metrics")
async def get_metrics():
    return latest_metrics

@app.get("/api/v1/analytics/percentiles")
async def get_percentiles():
    if not db_pool:
        return {"error": "Database not connected"}
    
    query = """
    SELECT 
        percentile_cont(0.95) WITHIN GROUP (ORDER BY avg_latency_cycles) as p95,
        percentile_cont(0.99) WITHIN GROUP (ORDER BY avg_latency_cycles) as p99,
        percentile_cont(0.999) WITHIN GROUP (ORDER BY avg_latency_cycles) as p999
    FROM telemetry_logs 
    WHERE created_at > NOW() - INTERVAL '1 hour';
    """
    async with db_pool.acquire() as conn:
        result = await conn.fetchrow(query)
        return dict(result)

@app.websocket("/ws/telemetry")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    print("UI Client connected to WebSocket stream.")
    try:
        while True:
            await websocket.send_json(latest_metrics)
            await asyncio.sleep(0.1)
    except WebSocketDisconnect:
        print("UI Client disconnected from telemetry stream.")
