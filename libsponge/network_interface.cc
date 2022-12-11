#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <optional>
#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    if (ARPTable.find(next_hop_ip) != ARPTable.end())
    {
        // The link layer address of the next hop is known
        // Send it directly
        EthernetFrame new_frame;
        
        // Frame construction
        new_frame.header().src = _ethernet_address;
        new_frame.header().dst = (ARPTable.find(next_hop_ip)->second).first;
        new_frame.header().type = EthernetHeader::TYPE_IPv4;
        new_frame.payload() = dgram.serialize();

        // Send the frame
        _frames_out.push(new_frame);

    }
    else
    {
        // Send an ARP request to get the link layer address
        // Note that two requests must be separated for at least 5s
        if (ARP_under_going.find(next_hop_ip) == ARP_under_going.end())
        {
            // The first request in the last 5s
            // Send it
            SendARPMsg(next_hop_ip, _ethernet_address, true);

            // Enqueue the datagram for later transmission
            DatagramBuffer[next_hop_ip].push(dgram);
        }
    }
    return;
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst == _ethernet_address || frame.header().dst == ETHERNET_BROADCAST)
    {
        // Only process frames destined to current ethernet address
        if (frame.header().type == EthernetHeader::TYPE_IPv4)
        {
            // IPv4 datagrams
            InternetDatagram dgram;
            if (dgram.parse(frame.payload()) == ParseResult::NoError)
            {
                return dgram;
            }
        }
        else
        {
            // ARP datagrams
            ARPMessage arp;
            
            if (arp.parse(frame.payload()) == ParseResult::NoError)
            {
                // Update the mapping table
                ARPTable[arp.sender_ip_address] = {arp.sender_ethernet_address, 0};

                // Delete the term in under going table
                std::map<uint32_t, size_t>::iterator iter;
                iter = ARP_under_going.find(arp.sender_ip_address);
                if (iter != ARP_under_going.end())
                {
                    ARP_under_going.erase(iter);
                }

                // Clear the queue 
                std::map<uint32_t, std::queue<InternetDatagram> >::iterator iter_buffer;
                iter_buffer = DatagramBuffer.find(arp.sender_ip_address);
                if (iter_buffer != DatagramBuffer.end())
                {
                    while (!(iter_buffer->second).empty())
                    {
                        InternetDatagram dgram = (iter_buffer->second).front();

                        // Frame construction
                        EthernetFrame new_frame;
                        new_frame.header().src = _ethernet_address;
                        new_frame.header().dst = (ARPTable.find(arp.sender_ip_address)->second).first;
                        new_frame.header().type = EthernetHeader::TYPE_IPv4;
                        new_frame.payload() = dgram.serialize();

                        (iter_buffer->second).pop();

                        // Send the frame
                        _frames_out.push(new_frame);
                    }
                    
                    DatagramBuffer.erase(iter_buffer);
                }

                // Send a reply if a request is received
                if (arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == _ip_address.ipv4_numeric())
                {
                    SendARPMsg(arp.sender_ip_address, arp.sender_ethernet_address, false);
                }
            }
            
        }
    }

    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick)
{
    // Update the time record of two hash tables
    std::map<uint32_t, pair <EthernetAddress, size_t> >::iterator iter_mapping;
    std::map<uint32_t, size_t>::iterator iter_outstanding;

    std::queue<uint32_t> deleted_mappings;

    for (iter_mapping = ARPTable.begin(); iter_mapping != ARPTable.end(); iter_mapping++)
    {
        iter_mapping->second.second += ms_since_last_tick;
        if (iter_mapping->second.second >= ARP_EXPIRED)
        {
            deleted_mappings.push(iter_mapping->first);
        }
    }

    for (iter_outstanding = ARP_under_going.begin(); iter_outstanding != ARP_under_going.end(); iter_outstanding++)
    {
        iter_outstanding->second += ms_since_last_tick;
        if (iter_outstanding->second >= ARP_TIMEOUT)
        {
            iter_outstanding->second = 0;

            SendARPMsg(iter_outstanding->first, _ethernet_address, true);
        }
    }

    // Delete if necessary
    while (!deleted_mappings.empty())
    {
        ARPTable.erase(deleted_mappings.front());
        deleted_mappings.pop();
    }
}


void NetworkInterface::SendARPMsg(const uint32_t & ip_addr, const EthernetAddress & Ethernet_addr, const bool is_request)
{
    if (is_request)
    {
        // Send an ARP request
        ARPMessage req;
        req.opcode = ARPMessage::OPCODE_REQUEST;
        req.sender_ethernet_address = _ethernet_address;
        req.sender_ip_address = _ip_address.ipv4_numeric();
        req.target_ip_address = ip_addr;

        // Use an Ethernet frame to send the request
        EthernetFrame ARP_Frame;
        ARP_Frame.header().src = _ethernet_address;
        ARP_Frame.header().dst = ETHERNET_BROADCAST;
        ARP_Frame.header().type = EthernetHeader::TYPE_ARP;
        ARP_Frame.payload() = req.serialize();

        _frames_out.push(ARP_Frame);
        ARP_under_going[ip_addr] = 0;
        
    }
    else
    {
        // Send an ARP reply
        ARPMessage rep;
        rep.opcode = ARPMessage::OPCODE_REPLY;
        rep.sender_ethernet_address = _ethernet_address;
        rep.sender_ip_address = _ip_address.ipv4_numeric();
        rep.target_ip_address = ip_addr;
        rep.target_ethernet_address = Ethernet_addr;

        // Use an Ethernet frame to send the request
        EthernetFrame ARP_Frame;
        ARP_Frame.header().src = _ethernet_address;
        ARP_Frame.header().dst = Ethernet_addr;
        ARP_Frame.header().type = EthernetHeader::TYPE_ARP;
        ARP_Frame.payload() = rep.serialize();

        _frames_out.push(ARP_Frame);
    }
}
