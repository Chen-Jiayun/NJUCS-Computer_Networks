#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
// 'capacity + 5' to avoid out-of-bound
ByteStream::ByteStream(const size_t capacity):
    buffer(capacity + 5, '\0'), 
    _capacity(capacity) {
    this->_empty = true;
    this->_curr_size = 0;
}

#define MIN(x, y) ((x) < (y) ? (x) : (y))
size_t ByteStream::write(const string &data) {
    size_t ret = MIN(data.size(), _capacity - _curr_size); 
    for(size_t i = _curr_size; i < _curr_size + ret; i += 1) {
        buffer[i] = data[i - _curr_size];
    }
    _curr_size += ret;
    if(_curr_size) {
        _empty = false;
    }
    return ret;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    DUMMY_CODE(len);
    return {};
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { DUMMY_CODE(len); }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    DUMMY_CODE(len);
    return {};
}

void ByteStream::end_input() {}

bool ByteStream::input_ended() const { return {}; }

size_t ByteStream::buffer_size() const { return {}; }

bool ByteStream::buffer_empty() const { return {}; }

bool ByteStream::eof() const { return false; }

size_t ByteStream::bytes_written() const { return {}; }

size_t ByteStream::bytes_read() const { return {}; }

size_t ByteStream::remaining_capacity() const { 
    return _capacity - _curr_size;
 }