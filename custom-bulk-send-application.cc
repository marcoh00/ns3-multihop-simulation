/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Georgia Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: George F. Riley <riley@ece.gatech.edu>
 */

// Kommunikation in verteilten Systemen - Simulation Model 1
// This code was adapted from ns3's bulk-send-application code
// The original code is not able to use udp sockets because udp sockets cannot
// emit the "connection established" event.
// We try to schedule the first sent packet at simulator time 0 instead, if we shall use an udp socket.
// This is mostly untested!

#include <math.h>
#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/udp-socket.h"
#include "custom-bulk-send-application.h"

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE ("CustomBulkSendApplication");

    NS_OBJECT_ENSURE_REGISTERED (CustomBulkSendApplication);

    TypeId
    CustomBulkSendApplication::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::CustomBulkSendApplication")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<CustomBulkSendApplication>()
                .AddAttribute("SendSize", "The amount of data to send each time.",
                              UintegerValue(512),
                              MakeUintegerAccessor(&CustomBulkSendApplication::m_sendSize),
                              MakeUintegerChecker<uint32_t>(1))
                .AddAttribute("Remote", "The address of the destination",
                              AddressValue(),
                              MakeAddressAccessor(&CustomBulkSendApplication::m_peer),
                              MakeAddressChecker())
                .AddAttribute("MaxBytes",
                              "The total number of bytes to send. "
                              "Once these bytes are sent, "
                              "no data  is sent again. The value zero means "
                              "that there is no limit.",
                              UintegerValue(0),
                              MakeUintegerAccessor(&CustomBulkSendApplication::m_maxBytes),
                              MakeUintegerChecker<uint64_t>())
                .AddAttribute("UdpInterval",
                              "UDP connections: Resend packets every x ms",
                              UintegerValue(100),
                              MakeUintegerAccessor(&CustomBulkSendApplication::m_udpinterval),
                              MakeUintegerChecker<uint32_t>())
                .AddAttribute("UdpCount",
                              "UDP connections: Send x packets every timeframe",
                              UintegerValue(100),
                              MakeUintegerAccessor(&CustomBulkSendApplication::m_udpcount),
                              MakeUintegerChecker<uint32_t>())
                .AddAttribute("Protocol", "The type of protocol to use.",
                              TypeIdValue(TcpSocketFactory::GetTypeId()),
                              MakeTypeIdAccessor(&CustomBulkSendApplication::m_tid),
                              MakeTypeIdChecker())
                .AddTraceSource("Tx", "A new packet is created and is sent",
                                MakeTraceSourceAccessor(&CustomBulkSendApplication::m_txTrace),
                                "ns3::Packet::TracedCallback");
        return tid;
    }


    CustomBulkSendApplication::CustomBulkSendApplication()
            : m_socket(0),
              m_connected(false),
              m_isudp(false),
              m_totBytes(0),
              m_rxBytes(0) {
        NS_LOG_FUNCTION (this);
    }

    CustomBulkSendApplication::~CustomBulkSendApplication() {
        NS_LOG_FUNCTION (this);
    }

    void
    CustomBulkSendApplication::SetMaxBytes(uint64_t maxBytes) {
        NS_LOG_FUNCTION (this << maxBytes);
        m_maxBytes = maxBytes;
    }

    Ptr<Socket>
    CustomBulkSendApplication::GetSocket(void) const {
        NS_LOG_FUNCTION (this);
        return m_socket;
    }

    void
    CustomBulkSendApplication::DoDispose(void) {
        NS_LOG_FUNCTION (this);

        m_socket = 0;
        // chain up
        Application::DoDispose();
    }

// Application Methods
    void CustomBulkSendApplication::StartApplication(void) // Called at time specified by Start
    {
        NS_LOG_FUNCTION (this);

        // Create the socket if not already
        if (!m_socket) {
            m_socket = Socket::CreateSocket(GetNode(), m_tid);

            if (m_socket->GetSocketType() != Socket::NS3_SOCK_STREAM &&
                m_socket->GetSocketType() != Socket::NS3_SOCK_SEQPACKET) {
                // This seems to be an UDP socket, which has no idea of "connections".
                // We need to schedule this by ourselves!
                NS_LOG_INFO("This seems to be an UDP socket, which is not supported in ns3's regular implementation. "
                            "CustomBulkSendApplication was patched to allow this and seems to work, but please take the "
                            "measurements with a grain of salt!");
                m_connected = true;
                m_isudp = true;
                Simulator::Schedule(Seconds(0), &CustomBulkSendApplication::SendData, this);
            }

            if (Inet6SocketAddress::IsMatchingType(m_peer)) {
                if (m_socket->Bind6() == -1) {
                    NS_FATAL_ERROR ("Failed to bind socket");
                }
            } else if (InetSocketAddress::IsMatchingType(m_peer)) {
                if (m_socket->Bind() == -1) {
                    NS_FATAL_ERROR ("Failed to bind socket");
                }
            }

            m_socket->Connect(m_peer);
            m_socket->ShutdownRecv();
            m_socket->SetConnectCallback(
                    MakeCallback(&CustomBulkSendApplication::ConnectionSucceeded, this),
                    MakeCallback(&CustomBulkSendApplication::ConnectionFailed, this));
            m_socket->SetSendCallback(
                    MakeCallback(&CustomBulkSendApplication::DataSend, this));
        }
        if (m_connected) {
            SendData();
        }
    }

    void CustomBulkSendApplication::StopApplication(void) // Called at time specified by Stop
    {
        NS_LOG_FUNCTION (this);

        if (m_socket != 0) {
            m_socket->Close();
            m_connected = false;
        } else {
            NS_LOG_WARN ("CustomBulkSendApplication found null socket to close in StopApplication");
        }
    }


// Private helpers

    void CustomBulkSendApplication::SendData(void) {
        NS_LOG_FUNCTION (this);
        uint32_t usendcount = 0;
        while (m_maxBytes == 0 || (!m_isudp && (m_totBytes < m_maxBytes)) ||
               (m_isudp && (usendcount < m_udpcount))) { // Time to send more

            // uint64_t to allow the comparison later.
            // the result is in a uint32_t range anyway, because
            // m_sendSize is uint32_t.
            uint64_t toSend = m_sendSize;
            // Make sure we don't send too many
            if (m_maxBytes > 0 && !m_isudp) {
                toSend = std::min(toSend, m_maxBytes - m_totBytes);
            }

            NS_LOG_LOGIC ("sending packet at " << Simulator::Now());
            Ptr<Packet> packet = Create<Packet>(toSend);
            int actual = m_socket->Send(packet);
            if (actual > 0) {
                m_totBytes += actual;
                usendcount++;
                m_txTrace(packet);
            }
            // We exit this loop when actual < toSend as the send side
            // buffer is full. The "DataSent"this->m_sendSize callback will pop when
            // some buffer space has freed ip.
            if ((unsigned) actual != toSend) {
                break;
            }
        }
        // Check if time to close (all sent)
        if (!m_isudp && (m_totBytes >= m_maxBytes)) {
            m_socket->Close();
            m_connected = false;
            NS_LOG_INFO("All packets sent at " << Simulator::Now());
        }
        if (m_isudp && (m_rxBytes >= m_maxBytes)) {
            m_socket->Close();
            m_connected = false;
            NS_LOG_INFO("All packets sent at " << Simulator::Now());
        } else if (m_isudp && m_rxBytes < m_maxBytes) {
            Simulator::Schedule(MilliSeconds(m_udpinterval), &CustomBulkSendApplication::SendData, this);
        }
    }

    void CustomBulkSendApplication::ConnectionSucceeded(Ptr<Socket> socket) {
        NS_LOG_FUNCTION (this << socket);
        NS_LOG_LOGIC ("CustomBulkSendApplication Connection succeeded");
        m_connected = true;
        SendData();
    }

    void CustomBulkSendApplication::ConnectionFailed(Ptr<Socket> socket) {
        NS_LOG_FUNCTION (this << socket);
        NS_LOG_LOGIC ("CustomBulkSendApplication, Connection Failed");
    }

    void CustomBulkSendApplication::DataSend(Ptr<Socket>, uint32_t) {
        NS_LOG_FUNCTION (this);

        if (m_connected) { // Only send new data if the connection has completed
            SendData();
        }
    }

    void CustomBulkSendApplication::AnnouncePacketsReceived(uint64_t rxcnt) {
        m_rxBytes = rxcnt;
    }


} // Namespace ns3
