#pragma once
#include <cstdint>

namespace aerohedge {

// Packed attribute ensures no compiler padding bytes are inserted,
// allowing direct zero-copy casting from raw UDP socket buffers.
#pragma pack(push, 1)
struct UDPBookTickerPacket {
    uint64_t timestamp_ns;
    uint32_t symbol_id;
    double best_bid_price;
    double best_ask_price;
    uint32_t bid_volume;
    uint32_t ask_volume;
};
#pragma pack(pop)

} // namespace aerohedge
