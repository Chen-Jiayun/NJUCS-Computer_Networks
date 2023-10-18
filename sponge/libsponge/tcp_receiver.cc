#include "tcp_receiver.hh"
#include <assert.h>
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    this->_is_syn_arrived |= seg.header().syn;
    if(seg.header().syn) {
        _isn = WrappingInt32(seg.header().seqno);
    }

    if(!this->_is_syn_arrived) { 
        // do not need to deal anything
    }
    else {
        this->_is_fin_arrived |= seg.header().fin;
        WrappingInt32 seqno(seg.header().seqno);
        /**
         *  index(payload[0]) = seq,     if syn == false
         *                    = seq + 1, if syn == true 
         */
        size_t index = unwrap(seqno + (seg.header().syn ? 1 : 0), _isn, _checkpoint); 

        if(index) {
            std::string payload = seg.payload().copy();
            this->_reassembler.push_substring(payload, index - 1, this->_is_fin_arrived);
            _checkpoint += seg.length_in_sequence_space();
        } 
        else {
            // invalid index, do nothing
        }
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(!_is_syn_arrived) return nullopt;
    else {
        uint64_t written_bytes = this->_reassembler.stream_out().bytes_written();
        uint64_t fin_size = this->_reassembler.stream_out().input_ended() ? 1 : 0;
        return wrap(written_bytes + 1 + fin_size, _isn);
    }
}

size_t TCPReceiver::window_size() const {
    return _capacity - stream_out().buffer_size();
}
