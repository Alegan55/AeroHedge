#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

namespace aerohedge {

// Packed struct so we can zero-copy send it over the wire
#pragma pack(push, 1)
struct TelemetryPacket {
    uint64_t timestamp_ns;
    uint64_t ticks_processed;
    uint64_t avg_latency_cycles;
};
#pragma pack(pop)

class UDPTelemetryPublisher {
public:
    UDPTelemetryPublisher(const char* ip, int port) {
        sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd_ < 0) {
            std::cerr << "Warning: Failed to create telemetry socket.\n";
            return;
        }

        // CRITICAL: Set socket to non-blocking so we NEVER yield the CPU
        int flags = fcntl(sock_fd_, F_GETFL, 0);
        fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

        std::memset(&addr_, 0, sizeof(addr_));
        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(port);
        inet_pton(AF_INET, ip, &addr_.sin_addr);
    }

    ~UDPTelemetryPublisher() {
        if (sock_fd_ >= 0) close(sock_fd_);
    }

    // Fire and forget!
    inline void publish(const TelemetryPacket& packet) noexcept {
        if (sock_fd_ >= 0) {
            // sendto on a non-blocking socket drops the packet into the kernel buffer and returns instantly
            sendto(sock_fd_, &packet, sizeof(packet), 0, 
                   reinterpret_cast<struct sockaddr*>(&addr_), sizeof(addr_));
        }
    }

private:
    int sock_fd_{-1};
    struct sockaddr_in addr_;
};

} // namespace aerohedge
