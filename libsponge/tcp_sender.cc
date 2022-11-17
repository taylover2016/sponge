#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <string>

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

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // Take care of the situation where window size == 0
    size_t window_size = _window_size ? _window_size : 1;

    // Try to fill the window
    while (_bytes_in_flight < window_size)
    {
        TCPSegment seg;

        // First consider the SYN flag
        if (!_syn_flag_set)
        {
            _syn_flag_set = true;
            seg.header().syn = true;
        }

        // Then the seqno
        seg.header().seqno = next_seqno();

        // Determine the size of the payload
        size_t payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, window_size - _bytes_in_flight - seg.header().syn);
        string payload = _stream.read(payload_size);

        // Note that the payload need not to have a length of payload_size!!
        // Because the payload_size is the maximum length available
        // While there may not be so much data!

        size_t actual_size_read = payload.length();

        // Take care of the FIN flag
        if (_stream.eof() && !_fin_flag_set && actual_size_read + _bytes_in_flight < window_size)
        {
            seg.header().fin = true;
            _fin_flag_set = true;
        }
        
        seg.payload() = Buffer(move(payload));

        // If there is no data to send, abort immediately
        if (seg.length_in_sequence_space() == 0)
        {
            break;
        }

        // Make sure the timer is up to date
        if (_in_flight_segments.empty())
        {
            _retransmission_timeout = _initial_retransmission_timeout;
            _time_elapsed = 0;
        }

        // Send the segment
        _segments_out.push(seg);

        // Keep track of the segment
        _bytes_in_flight += seg.length_in_sequence_space();
        _in_flight_segments.push(make_pair(_next_seqno, seg));

        // Update the absolute seqno
        _next_seqno += seg.length_in_sequence_space();

        // If the FIN flag is set, exit immediately
        if (_fin_flag_set)
        {
            break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t abs_ackno = unwrap(ackno, _isn, _next_seqno);

    // Note that abs_ackno has to be smaller than _next_seqno!
    if (abs_ackno > _next_seqno)
    {
        return;
    }

    // Discard all the segments that are fully acknowledged
    // (Packet length fully smaller than abs_ackno)
    while (_in_flight_segments.size() != 0)
    {
        const TCPSegment &seg = _in_flight_segments.front().second;
        // Check whether the segment is fully acknowledged
        if (_in_flight_segments.front().first + seg.length_in_sequence_space() <= abs_ackno)
        {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _in_flight_segments.pop();

            // A new ack received, update the timer
            _retransmission_timeout = _initial_retransmission_timeout;
            _time_elapsed = 0;
            _consecutive_retransmission_count = 0;
        }
        else
        {
            break;
        }
    }

    // Try to fill the window
    _window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _time_elapsed += ms_since_last_tick;

    // If there is any segment in flight, and the timer expires
    if (_bytes_in_flight != 0 && _time_elapsed >= _retransmission_timeout)
    {
        // Retransimit the oldest segment
        _segments_out.push(_in_flight_segments.front().second);

        if (_window_size > 0)
        {
            _retransmission_timeout *= 2;
            _consecutive_retransmission_count++;
        }

        _time_elapsed = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmission_count; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
