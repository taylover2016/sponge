#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
    
    const uint32_t prefix_mask = prefix_length == 0 ? 0 : numeric_limits<int>::min() >> (prefix_length-1);
    RouterEntry new_entry{route_prefix, prefix_length, prefix_mask, next_hop, interface_num};
    _routing_table.push_back(new_entry);

}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // Check the TTL field for expiratiion
    // Decrement if valid
    if (dgram.header().ttl <= 1)
    {
        return;
    }

    // Perform LPM
    uint32_t dest = dgram.header().dst;
    int best_match_idx = -1;
    int best_match_len = -1;

    // Iterate through the table
    for (size_t i = 0; i < _routing_table.size(); i++)
    {
        auto mask = _routing_table[i].prefix_mask;

        if ((dest & mask) == _routing_table[i].route_prefix && best_match_len < _routing_table[i].prefix_length)
        {
            best_match_idx = i;
            best_match_len = _routing_table[i].prefix_length;
        }
    }
    
    // Drop packet if no match
    if (best_match_idx == -1)
    {
        return;
    }

    // Now to forward the packet
    dgram.header().ttl--;
    auto next_hop = _routing_table[best_match_idx].next_hop;
    auto interface_num = _routing_table[best_match_idx].interface_num;
    if (next_hop.has_value())
    {
        _interfaces[interface_num].send_datagram(dgram, next_hop.value());
    }
    else
    {
        _interfaces[interface_num].send_datagram(dgram, Address::from_ipv4_numeric(dest));
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
