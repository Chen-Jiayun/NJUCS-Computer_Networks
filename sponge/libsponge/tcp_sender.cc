#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <assert.h>
#include <iostream>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {
    return _byte_on_flight;
}

void TCPSender::send_seg(TCPSegment s) {
    s.header().seqno = wrap(_next_seqno, _isn);
    if(!_timer.is_running()) {
        _timer.run(_time, _initial_retransmission_timeout);
    }
    _segments_out.push(s);
    _timer.append_outstanding_segment(s);
    _next_seqno += s.length_in_sequence_space();
    _byte_on_flight += s.length_in_sequence_space();
}


void TCPSender::fill_window() {
    TCPSegment s;
    //set syn, send the first EMPTY segment
    if(!_syn) {
        // state: close
        assert(_next_seqno == 0);
        _syn = true;
        s.header().syn = true;
        send_seg(s);
        return ;
    }

    uint16_t win_sz = (_rcvd_window_size > 0) ? _rcvd_window_size : 1;
    if(_fin) {
        return ;
    }
    // "_next_seqno < _latest_ack + win_sz" means "receiver are expecting new bits" 
    else if(!_fin && stream_in().eof() && _next_seqno < _latest_ack + win_sz) {
        _fin = true;
        s.header().fin = true;
        send_seg(s);
        return ;
    }
    else {
        while(!stream_in().buffer_empty() && _next_seqno < _latest_ack + win_sz) {
            std::cerr << "bug here\n";
            // keep send segment
            uint16_t seg_sz = min(TCPConfig::MAX_PAYLOAD_SIZE, win_sz + _latest_ack - _next_seqno);
            seg_sz = min(seg_sz, static_cast<uint16_t>(this->_stream.buffer_size()));
            s.payload() = Buffer(this->_stream.read(seg_sz));
            // current window can hold the "fin" bit
            if(stream_in().eof() && seg_sz + _next_seqno < _latest_ack + win_sz ) {
                _fin = true;
                s.header().fin = true;
            }
            send_seg(s);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    uint64_t abs_ack = unwrap(ackno, _isn, _next_seqno);
    
    if(abs_ack < _latest_ack || abs_ack > _next_seqno) {
        // do nothing
        return ;
    }
    // latest_ack < abs_ack < _next_seq
    _latest_ack = abs_ack;
    _rcvd_window_size = window_size;
    _byte_on_flight -= _timer.get_ack(ackno, _isn, _next_seqno, _initial_retransmission_timeout, _time);
    this->fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    this->_time += ms_since_last_tick;
    if(_timer.is_time_out(this->_time)) {
        TCPSegment s = _timer.get_oldest();
        _segments_out.push(s);
        // adjust will differ, due to the rcvd window size
        _timer.adjust_on_retransmission(_time, _rcvd_window_size);
    }
}



unsigned int TCPSender::consecutive_retransmissions() const {
    return _timer.get_resend_times();
}

void TCPSender::send_empty_segment() {
    TCPSegment s;
    s.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(s);
}

RetransmissionTimer::RetransmissionTimer() {
}

void RetransmissionTimer::run(size_t curr_time, unsigned int init_RTO) {
    _is_running = true;
    _begin_time = curr_time;
    current_RTO = init_RTO;
}

bool RetransmissionTimer::is_time_out(size_t curr_time) {
    return _is_running && curr_time - _begin_time >= current_RTO;
}

void RetransmissionTimer::append_outstanding_segment(TCPSegment s) {
    _outstanding_q.push(s);
}

size_t RetransmissionTimer::get_ack(const WrappingInt32 ackno, WrappingInt32 isn, uint64_t next_seq, 
                                    unsigned int init_RTO, size_t curr_time) {
    // pop outstanding segment, reset RTO
    uint64_t abs_ack = unwrap(ackno, isn, next_seq);
    bool pop_out_seg = false;
    size_t ret = 0;
    while(!_outstanding_q.empty()) {
        TCPSegment s = _outstanding_q.front();
        uint64_t abs_seq = unwrap(s.header().seqno, isn, next_seq);
        if(abs_ack >= abs_seq + s.length_in_sequence_space()) {
            _outstanding_q.pop();
            pop_out_seg |= true;
            ret += s.length_in_sequence_space(); 
        }
        else {
            // no more packet will be pop out
            break; 
        }
    }   
    // reset timer iff the latest ack "has effected" to old acks
    // the affect is represent by "pop_out_seg: bool"
    if(pop_out_seg) {
        this->run(curr_time, init_RTO);
        this->_count_retransmission_segment = 0;
    }

    if(_outstanding_q.empty()) {
        this->stop();
    }
    return ret;
}
    
    
TCPSegment RetransmissionTimer::get_oldest() {
    return _outstanding_q.front();
}

void RetransmissionTimer::adjust_on_retransmission(size_t curr_time, uint16_t curr_win) {
    if(curr_win) {
        this->current_RTO *= 2;
        _count_retransmission_segment += 1;
    }
    // restart timer func do two things:
    //      1. set begin_time 
    //      2. set curr_RTO
    // curr_RTO has been adjust on the code above
    // so what we need to do is set the begin_time
    // so I don't call the func "Retransmission::run()"
    this->_begin_time = curr_time;
}