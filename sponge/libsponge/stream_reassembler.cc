#include "stream_reassembler.hh"

#include <assert.h>
#include <algorithm>
#include <iostream>
// Dummy implementation of a stream reassembler.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
    _output(capacity), 
    _capacity(capacity),
    _buffer(capacity, '\0'),
    _used(capacity, false),
    _offset(0),
    _eof(false),
    _unassembled_count(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t end_index = index + data.length() - 1;

    // offset also means the next expected index
    std::string written_data;
    size_t written_begin = index;
    if(data.empty()) {
        written_data = "";
        written_begin = index;
    }
    else {
        if(end_index < _offset) { // index < offset
            written_data = "";
            written_begin = -1;
        }    
        else if(index < _offset && end_index < _offset + _capacity) {
            // cut data head.
            written_data = data.substr(_offset - index, data.length() - _offset + index);
            written_begin = _offset;
        }
        else if(index >= _offset && end_index >= _offset + _capacity) {
            // cut data tail
            written_data = data.substr(0, _offset + _capacity - index);
            written_begin = index;
        }
        else if(index < _offset && end_index >= _offset + _capacity) {
            // cut data both head and tail
            written_data = data.substr(_offset - index, _capacity);
            written_begin = _offset;
        }
        else if(index >= _offset && end_index < _offset + _capacity) {
            // write all string of data
            written_data = data;
            written_begin = index;
        }
    }

    assert(written_data.length() <= _capacity);

    size_t physical_index = written_begin - _offset;
    for(size_t i = 0; i < written_data.length(); i += 1) {
        _buffer[physical_index + i] = written_data[i];

        if(_used[physical_index + i] == false) {
            _unassembled_count += 1;
        }
        _used[physical_index + i] = true;
    }

    if(_used[0]) {
        buffer_2_stream();
    }
    if(eof) {
        this->_eof = true;
    }
    if(this->_eof && this->empty()) {
        _output.end_input();
    }
}

void StreamReassembler::buffer_2_stream() {
    assert(_used[0] && "not continuous bytes will be written?!");
    size_t max = this->_output.remaining_capacity();
    if(max == 0) return ;

    size_t end = 0;
    while(_used[end] && end < max) {
        end += 1;
    }
    // write to stream
    this->_output.write(_buffer.substr(0, end));

    // adjust local buffer
    _unassembled_count -= end;
    _offset += end;
    for(size_t i = end; i < _capacity; i += 1) {
        _buffer[i - end] = _buffer[i];
        _used[i - end] = _used[i];
    }
    // padding false
    for(size_t i = _capacity - end; i < _capacity; i += 1) {
        _used[i] = false;
    }

}

size_t StreamReassembler::unassembled_bytes() const {
    return _unassembled_count;
}

bool StreamReassembler::empty() const { 
    return this->unassembled_bytes() == 0;
}