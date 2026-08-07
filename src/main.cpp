#include "aerohedge/memory_allocator.hpp"
#include "aerohedge/spsc_ring_buffer.hpp"
#include "aerohedge/thread_utils.hpp"
#include "aerohedge/market_data.hpp"
#include "aerohedge/rdtsc_timer.hpp"
#include "aerohedge/option_engine.hpp"
#include "aerohedge/telemetry_publisher.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

int main() {
    std::cout << "AeroHedge Continuous Engine Starting...\n";

    aerohedge::SPSCRingBuffer<aerohedge::UDPBookTickerPacket, 1024> ring_buffer;
    char raw_network_buffer[sizeof(aerohedge::UDPBookTickerPacket)];

    // 1. Continuous Network Feed Producer Thread
    std::thread network_thread([&]() {
        uint64_t i = 1;
        while (true) {
            aerohedge::UDPBookTickerPacket simulated_packet{
                .timestamp_ns = aerohedge::RDSTCTimer::rdtsc(),
                .symbol_id = 101,
                .best_bid_price = 45000.50 + static_cast<double>(i % 100),
                .best_ask_price = 45001.00 + static_cast<double>(i % 100),
                .bid_volume = 10,
                .ask_volume = 15
            };
            std::memcpy(raw_network_buffer, &simulated_packet, sizeof(aerohedge::UDPBookTickerPacket));
            const auto* incoming_ticker = reinterpret_cast<const aerohedge::UDPBookTickerPacket*>(raw_network_buffer);

            while (!ring_buffer.push(*incoming_ticker)) {
                std::this_thread::yield();
            }
            i++;
            // Small throttle so it doesn't max out the CPU entirely
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    // 2. Continuous Trading Engine Consumer Thread
    std::thread engine_thread([&]() {
        aerohedge::UDPTelemetryPublisher telemetry("127.0.0.1", 8080);

        uint64_t processed_count = 0;
        uint64_t total_latency_cycles = 0;
        aerohedge::UDPBookTickerPacket ticker{};

        while (true) {
            if (ring_buffer.pop(ticker)) {
                uint64_t start_cycles = aerohedge::RDSTCTimer::rdtsc();

                double delta = aerohedge::OptionHedgingEngine::calculate_delta(ticker.best_bid_price, 45000.0, 0.25);
                double hedge_qty = aerohedge::OptionHedgingEngine::compute_hedge_quantity(100.0, delta);
                (void)hedge_qty; 

                uint64_t end_cycles = aerohedge::RDSTCTimer::rdtsc();
                total_latency_cycles += (end_cycles - start_cycles);
                processed_count++;

                // Fire telemetry every 5,000 ticks
                if (processed_count % 5000 == 0) {
                    aerohedge::TelemetryPacket packet{
                        .timestamp_ns = aerohedge::RDSTCTimer::rdtsc(),
                        .ticks_processed = processed_count,
                        .avg_latency_cycles = total_latency_cycles / processed_count
                    };
                    telemetry.publish(packet);
                }
            } else {
                std::this_thread::yield();
            }
        }
    });

    aerohedge::pin_thread_to_core(network_thread, 1);
    aerohedge::pin_thread_to_core(engine_thread, 2);

    aerohedge::MemoryEnforcer::lock_allocations();

    network_thread.join();
    engine_thread.join();

    return 0;
}
