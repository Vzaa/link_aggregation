#include <iostream>
#include <unistd.h> // sleep()
#include <string.h> // memcpy()
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "link_aggregator.hh"

LinkAggregator::LinkAggregator( const std::string config_filename )
        : m_config(config_filename)
        , m_link_manager(m_config.PeerAddresses(), m_config.IfNames())
        , m_nfds(2) {

    m_pfds[0].fd = m_link_manager.PipeRxFd();
    m_pfds[1].fd = m_client.RxFd();
    m_pfds[0].events = LAGG_POLL_EVENTS;
    m_pfds[1].events = LAGG_POLL_EVENTS;

    // Print config
    PrintConfig();
}

void LinkAggregator::Aggregate() {

    for(;;) {
        errno = 0;
        int nfds_rdy = poll(m_pfds, m_nfds, -1);
        assert_perror(errno);

        if(m_pfds[1].revents & LAGG_POLL_EVENTS) {
            DeliveryChain();
        }

        if(m_pfds[0].revents & LAGG_POLL_EVENTS) {
            ReceptionChain();
        }
    }
}

void LinkAggregator::DeliveryChain() {

    Buffer const * buf;

    // Receive from client
    buf = RecvPktFromClient();
    if(buf) {
        // Got packet, forward to links
        SendOnLinks(buf);
//        std::cout << "Freeing Buffer (tx chain)" << std::endl;
        delete buf;
//        std::cout << "Free ok" << std::endl;
    }
}

void LinkAggregator::ReceptionChain() {

    Buffer const * buf;

    // Receive from links
    buf = RecvOnLinks();
    if(buf) {
        // Got packet, forward to client
        SendPktToClient(buf);
//        std::cout << "Deleting buffer retrieved from pool\n";
        delete buf;
//        std::cout << "Free ok" << std::endl;
    }
}

Buffer const * LinkAggregator::RecvPktFromClient() {

    return m_client.RecvPkt();
}

int LinkAggregator::SendPktToClient( Buffer const * buf ) {

    return m_client.SendPkt(buf);
}

void LinkAggregator::PrintConfig() const {
    std::cout << "Proxying traffic destined for:\n";
    std::cout << "    " << m_config.ClientIp().Str() << std::endl;
    std::cout << "Link setup:\n";
    auto links = m_link_manager.Links();
    for( int i = 0; i < links.size(); i++ ) {
        std::cout << "    ";
        std::cout << links[i]->OwnAddr().Str();
        std::cout << " <--" << links[i]->IfName() << "--> ";
        std::cout << links[i]->PeerAddr().Str();
        std::cout << std::endl;
    }
}

/*
 * Send the provided msg on all links
 */

int LinkAggregator::SendOnLinks( Buffer const * buf ) {

    return m_link_manager.Send(buf);
}

Buffer const * LinkAggregator::RecvOnLinks() {

    return m_link_manager.Recv();
}
