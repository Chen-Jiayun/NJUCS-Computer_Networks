#include "byte_stream.hh"
#include <assert.h>

// Dummy implementation of a flow-controlled in-memory byte stream.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

// 'capacity + 5' to avoid out-of-bound, defensive code
ByteStream::ByteStream(const size_t capacity):
    buffer(capacity + 5, '\0'), 
    _capacity(capacity),
    _input_end(false),
    _curr_size(0),
    _written(0),
    _read(0) {
}

#define MIN(x, y) ((x) < (y) ? (x) : (y))
size_t ByteStream::write(const string &data) {
    size_t ret = MIN(data.size(), _capacity - _curr_size); 
    for(size_t i = _curr_size; i < _curr_size + ret; i += 1) {
        buffer[i] = data[i - _curr_size];
    }
    _curr_size += ret;
    _written += ret;
    return ret;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    assert(len <= _curr_size && "read too much bytes");
    return this->buffer.substr(0, len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    assert(len <= _curr_size && "pop too much bytes");
    for(size_t i = len; i < _curr_size; i += 1) {
        buffer[i - len] = buffer[i];
    }
    this->_curr_size -= len;
    this->_read += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string ret = this->peek_output(len);
    this->pop_output(len);
    return ret;
}

void ByteStream::end_input() {
    _input_end = true;
}

bool ByteStream::input_ended() const { 
    return this->_input_end;
}

size_t ByteStream::buffer_size() const {
    return _curr_size;
}

bool ByteStream::buffer_empty() const {
    return this->_curr_size == 0;
}

bool ByteStream::eof() const { 
    return this->input_ended() && this->buffer_empty();
}

size_t ByteStream::bytes_written() const { return _written; }

size_t ByteStream::bytes_read() const { return _read; }

size_t ByteStream::remaining_capacity() const { 
    return _capacity - _curr_size;
 }