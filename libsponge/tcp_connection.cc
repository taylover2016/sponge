#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_interval; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // Reset the time interval
    _time_interval = 0;

    // Send the segment to the receiver
    _receiver.segment_received(seg);

    // Check the RST flag first
    if (seg.header().rst)
    {
        // Reset the connection
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _linger_after_streams_finish = false;
        _is_active = false;
        
        return;
    }

    // Update the sender if necessary
    if (seg.header().ack)
    {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    
    ////////////////
    // Edge Cases //
    ////////////////

    // TCP connection setup
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED)
        {
            // SYN received
            // Send SYN/ACK
            connect();
            return;
        }

    // TCP connection teardown
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)
        {
            // No need to CLOSE_WAIT
            _linger_after_streams_finish = false;
        }
    
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && ! _linger_after_streams_finish)
        {
            // Close directly
            _is_active = false;
            return;
        }

    // If the segment occupies any seqno, reply to update the ackno and window
    if (seg.length_in_sequence_space() && _sender.segments_out().empty())
    {
        _sender.send_empty_segment();
    }

    // Take care of "keep-alive" segment
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0)
        and seg.header().seqno == _receiver.ackno().value() - 1)
        {
            _sender.send_empty_segment();
        }
    
    // Send packets in storage
    _send_packets();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    size_t size_written = _sender.stream_in().write(data);
    _sender.fill_window();
    _send_packets();
    return size_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // Update the time interval (counter used to record the elapsed time since the last segment received)
    _time_interval += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    // Send an RST segment if retransimission timeout
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS)
    {
        // Send an RST segment
        TCPSegment seg;
        seg.header().rst = true;
        _segments_out.push(seg);

        // Reset the connection
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _linger_after_streams_finish = false;
        _is_active = false;

        return;
    }

    // Send any packets in storage
    _send_packets();

    // Close if necessary
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish
        && _time_interval >= 10*_cfg.rt_timeout)
        {
            // Close directly
            _is_active = false;
            _linger_after_streams_finish = false;
            return;
        }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _send_packets();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _is_active = true;
    _send_packets();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            // Send an RST segment
            TCPSegment seg;
            seg.header().rst = true;
            _segments_out.push(seg);

            // Reset the connection
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            _linger_after_streams_finish = false;
            _is_active = false;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_send_packets()
{
    while (! _sender.segments_out().empty())
    {
        // Get the next segment to send
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        if (_receiver.ackno().has_value())
        {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
}
