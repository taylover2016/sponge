#include "tcp_receiver.hh"
#include <optional>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // Get the header of current TCP segment
    const TCPHeader &header = seg.header();

    // Listen for the SYN message if it has not arrived
    if (!syn_received)
    {
        if (!header.syn)
        {
            // Not a SYN message, abandon it
            return;
        }

        // Otherwise, record the SeqNo. and start receiving
        isn = header.seqno;
        syn_received = true;
    }

    // Use the absolute AckNo. as the checkpoint
    uint64_t checkpoint = 1 + _reassembler.stream_out().bytes_written();
    uint64_t seqno_64 = unwrap(header.seqno, isn, checkpoint);
    
    // Note for the offset caused by the  SYN flag
    uint64_t stream_index = seqno_64 - 1 + header.syn;

    _reassembler.push_substring(seg.payload().copy(), stream_index, header.fin);

}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    // If SYN has not been received
    if(!syn_received)
    {
        return nullopt;
    }

    // Otherwise return the AckNo.
    uint64_t next_index_64 = 1 + _reassembler.stream_out().bytes_written();

    if (_reassembler.stream_out().input_ended())
    {
        next_index_64++;
    }

    return wrap(next_index_64, isn);

}

size_t TCPReceiver::window_size() const {
    return _capacity - _reassembler.stream_out().buffer_size();
}
