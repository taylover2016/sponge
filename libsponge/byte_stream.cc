#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
{
    max_capacity = capacity;
    return;
}

size_t ByteStream::write(const string &data) {
    size_t bytes_written = min(data.length(), remaining_capacity());
    for (size_t i = 0; i < bytes_written; i++)
    {
        buffer.push_back(data[i]);
    }
    total_bytes_written += bytes_written;
    return bytes_written;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_len = min(len, buffer.size());
    return string(buffer.begin(), buffer.begin()+peek_len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_len = min(len, buffer.size());
    total_bytes_read += pop_len;
    buffer.erase(buffer.begin(), buffer.begin()+pop_len);
    return;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string output = peek_output(len);
    pop_output(len);
    return output;
}

void ByteStream::end_input() {
    ended = true;
}

bool ByteStream::input_ended() const {
    return ended;
}

size_t ByteStream::buffer_size() const {
    return buffer.size();
}

bool ByteStream::buffer_empty() const {
    return buffer.empty();
}

bool ByteStream::eof() const {
    return buffer.empty() && ended;
}

size_t ByteStream::bytes_written() const {
    return total_bytes_written;
}

size_t ByteStream::bytes_read() const {
    return total_bytes_read;
}

size_t ByteStream::remaining_capacity() const {
    return max_capacity - buffer.size();
}
