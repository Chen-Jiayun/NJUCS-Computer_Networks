#include "wrapping_integers.hh"

#include <assert.h>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

using u64 = uint64_t;
using u32 = uint32_t;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return WrappingInt32(n + isn.raw_value());
}

static u64 min3(u64 x, u64 y, u64 z) {
    return min(min(x, y), z);
}

static uint64_t closest(uint64_t x, uint64_t c) {
    constexpr uint64_t MOD = (0x1ul << 32);
    u64 d1 = x - c, d2 = x + MOD - c, d3 = x - MOD - c;
    if(x < c) {
        d1 = c - x;
    }
    if(x - MOD < c) {
        d3 = c - (x - MOD);
    }
    u64 min = min3(d1, d2, d3);
    if(min == d1) return x;
    else if(min == d2) return x + MOD;
    else if(min == d3) return x - MOD;
    else {
        assert(0 && "should never touch here");
    }
    return 0;
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    WrappingInt32 temp(n - isn);
    uint32_t temp_raw = temp.raw_value();

    uint64_t mask = (static_cast<u64> (UINT32_MAX) << 32);
    uint64_t high_checkpoint = checkpoint & mask;
    uint64_t x = high_checkpoint | temp_raw;

    return closest(x, checkpoint);
}
