#include "stream_reassembler.hh"
#include "cassert"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
    // Capacity_consumed to count the buffer for the assembler
    // Using _output to get the capacity consumed for the ByteStream
    // The sum of those two cannot exceed capacity
    _capacity = capacity;
    _output = ByteStream(capacity);
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof)
{
    auto pre_cursor = reassemble_buffer.upper_bound(index);
    // There are 3 possibilities
    // begin() == end() [empty] --> pre_cursor == end() == begin()
    // begin() != end() [non-empty] --> pre-cursor == begin() -> index is the smallest
    //                              |_> otherwise pre-cursor-- is the one smaller or equal to index
    size_t starting_index = -1;
    
    if (pre_cursor != reassemble_buffer.begin())
    // There is indeed something smaller in the reassembler
    // Meaning it can't be assembled at once
    {
        pre_cursor--;
        size_t pre_index = pre_cursor->first;
        if (index < pre_index + pre_cursor->second.length())
        {
            starting_index = pre_index + pre_cursor->second.length();
        }
        else
        {
            starting_index = index;
        }
    }
    // Current one is the smallest
    else if (next_index_to_assemble >= index)
    // Check whether there are any redundant contents
    {
        starting_index = next_index_to_assemble;
    }
    else
    {
        starting_index = index;
    }

    
    size_t starting_index_in_data = (starting_index - index);
    ssize_t data_size = data.length() - starting_index_in_data;


    // Find the next one string
    auto post_cursor = reassemble_buffer.lower_bound(starting_index);
    size_t ending_index_in_data = starting_index+data_size;

    while (post_cursor != reassemble_buffer.end())
    {
        // There are some strings that are coming after the current one
        if (post_cursor->first < ending_index_in_data)
        {
            // Partially intersected
            if (post_cursor->first + post_cursor->second.size() > ending_index_in_data)
            {
                data_size = post_cursor->first - starting_index;
                break;
            }
            // The latter is fully contained in current one
            else
            {
                assembler_size -= post_cursor->second.size();
                post_cursor = reassemble_buffer.erase(post_cursor);
                continue;
            }
        }
        else
        {
            break;
        }
    }
    
    // Now the data to be processed starts from starting_point and has a size of data_size
    // Check the capacity
    size_t index_out = next_index_to_assemble + _capacity - _output.buffer_size();
    if (starting_index >= index_out)
    {
        return;
    }

    if (data_size > 0)
    {
        string actual_data = data.substr(starting_index_in_data, data_size);
        if (starting_index == next_index_to_assemble)
        {
            // Send them to ByteStream first
            size_t bytes_written = _output.write(actual_data);
            next_index_to_assemble += bytes_written;
            if (bytes_written < actual_data.size())
            {
                // Need to store the rest in the buffer
                string rest_data = actual_data.substr(bytes_written);
                assembler_size += rest_data.size();
                reassemble_buffer.insert(make_pair(next_index_to_assemble, rest_data));
            }
        }
        else
        {
            assembler_size += actual_data.size();
            reassemble_buffer.insert(make_pair(starting_index, actual_data));
        }
    }
    


    // Check the buffer again
    for (auto iter = reassemble_buffer.begin(); iter != reassemble_buffer.end();)
    {
        assert(next_index_to_assemble <= iter->first);
        if (next_index_to_assemble == iter->first)
        {
            size_t bytes_written = _output.write(iter->second);
            next_index_to_assemble += bytes_written;
            if (bytes_written < iter->second.size())
            {
                // Need to store the rest in the buffer
                string rest_data = (iter->second).substr(bytes_written);
                assembler_size += rest_data.size();
                reassemble_buffer.insert(make_pair(next_index_to_assemble, rest_data));
                assembler_size -= (iter->second).size();
                reassemble_buffer.erase(iter);
                break;
            }
            assembler_size -= (iter->second).size();
            iter = reassemble_buffer.erase(iter);
        }
        else
        {
            break;
        }
        
    }

    if (eof == true) {
        eof_index = index + data.size();
    }

    if (eof_index <= next_index_to_assemble) {
        _output.end_input();
    }

    
}

size_t StreamReassembler::unassembled_bytes() const { return assembler_size; }

bool StreamReassembler::empty() const { return assembler_size == 0; }
