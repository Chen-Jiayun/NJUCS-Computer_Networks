#include "tcp_connection.hh"

#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _ms_since_last_seg;
}

void TCPConnection::segment_received(const TCPSegment &seg) { 
    /**
     * 1. check rst flag
     * 2. notify receiver
     * 3. ack to sender
     * 4. at least one valid seg with 'ackno' and win
     */
    
    _ms_since_last_seg = 0;

    if(seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _active = false;
    }

    // may lead to "receiver_OK"
    _receiver.segment_received(seg);
    if(receiver_OK() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    if(seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if(seg.length_in_sequence_space()) {
        // bug case: syn arrival, should send a seg with "SYN"
        // but '_sender.send_empty_segment()' is not enough
        _sender.fill_window();
        bool has_send_seg = send();
        if(!has_send_seg) {
            _sender.send_empty_segment();
            TCPSegment s = _sender.segments_out().front();
            _sender.segments_out().pop();
            _set_ack_and_win(s);
            _segments_out.push(s);
        }
    }

}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t ret = _sender.stream_in().write(data);
    if(ret) {
        _sender.fill_window();
        this->send();
    }
    return ret;
}


void TCPConnection::make_reset() {
    TCPSegment s;
    s.header().rst = true;
    _segments_out.push(s);
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}


//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    _ms_since_last_seg += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        make_reset();
        return ;
    }
    this->send();
    if(receiver_OK() && sender_OK()) {
        if(!_linger_after_streams_finish) {
            // first to close 
            _active = false;
        }
        else if(_ms_since_last_seg >= 10 * _cfg.rt_timeout){
            // linger over
            _active = false;
        }
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    // may need send fin manually
    _sender.fill_window();
    this->send();
}


void TCPConnection::_set_ack_and_win(TCPSegment& s) {
    s.header().win = min(_receiver.window_size(), static_cast<size_t>(std::numeric_limits<uint16_t>::max()));
    if(_receiver.ackno().has_value()) {
        s.header().ack = true;
        s.header().ackno = _receiver.ackno().value();
    }
}

bool TCPConnection::send() {
    bool ret = false;
    auto& q = _sender.segments_out();
    while(!q.empty()) {
        TCPSegment seg = q.front();
        q.pop();
        _set_ack_and_win(seg);
        _segments_out.push(seg);
        ret |= true;
    }
    return ret;
}

void TCPConnection::connect() {
    _sender.fill_window();
    this->send();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            make_reset();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}


bool TCPConnection::receiver_OK() {
    return _receiver.stream_out().eof() && _receiver.unassembled_bytes() == 0;
}

bool TCPConnection::sender_OK() {
    return 
        _sender.stream_in().eof() && \
        _sender.bytes_in_flight() == 0 && \
        _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2;
}