/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

// Network topology
//
//       n0 ----------- n1
//
// - Flow from n0 to n1 using BulkSendApplication.
// - Tracing of queues and packet receptions to file "bulk-send.tr"
//   and pcap tracing available when tracing is turned on.


// Kommunikation in verteilten Systemen - Simulation Model 1
// This code was adapted from ns3's "tcp-bulk-send.cc" example.
// The custom-bulk-send-application files are taken from ns3's bulk send application code
// (bulk-send-{helper,application}.{cc,h})

// This simulation sets up a Point-To-Point connection between two nodes and tries to send
// as many TCP or UDP packets with a configurable size (send_size) as it can, until it has sent
// maxBytes bytes.

// The following changes to the example code were made:
// Most of the protocol parameters are now configurable using the command line
// Default value of data_rate was set to 100 Mbps to match the TP Link router's ethernet speed
// Usage of TCP _and_ UDP protocols is possible
// BulkSendApplication can now use Udp Sockets by manually scheduling the transmit of packets

#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "custom-bulk-send-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BulkSendExample");

int
main(int argc, char *argv[]) {
    // This activates packet logging to ascii and pcap files
    bool tracing = false;
    // Default: Stop after sending approx. 1 MiB of data
    uint32_t maxBytes = 1048576;
    // Default: 512 Bytes per packet
    uint32_t send_size = 1024;
    // We simulate a tcp connection (example's default). Can be changed to `ns3::UdpSocketFactory`.
    std::string socket_factory = "ns3::TcpSocketFactory";

    // Match the TP Link router's ethernet speed
    std::string data_rate = "100Mbps";

    // This is the default from the example code - as I do not have measurements of real-world
    // P2P-links I'll keep it like that. Can be configured via command line just in case!
    std::string delay = "5ms";

    //
    // Allow the user to override any of the defaults at
    // run-time, via command-line arguments
    //
    CommandLine cmd;
    cmd.AddValue("tracing", "Flag to enable/disable tracing", tracing);
    cmd.AddValue("maxBytes", "Total number of bytes for application to send", maxBytes);
    cmd.AddValue("send_size", "Bytes sent per packet", send_size);
    cmd.AddValue("socket_factory", "Socket Factory to use. Default is ns3::TcpSocketFactory", socket_factory);
    cmd.AddValue("data_rate", "Point-to-point link data rate", data_rate);
    cmd.AddValue("delay", "Point-to-Point connection delay", delay);
    cmd.Parse(argc, argv);

    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point link required by the topology (shown above).
    //
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(data_rate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    //
    // Install the internet stack on the nodes (IP)
    //
    InternetStackHelper internet;
    internet.Install(nodes);

    //
    // We've got the "hardware" in place.  Now we need to add IP addresses.
    //
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);

    NS_LOG_INFO("Create Applications.");

    //
    // Create a BulkSendApplication and install it on node 0
    // A BulkSendApplication sends as many packets as fast as it can
    // until it reaches a certain, configurable, limit.
    //
    uint16_t port = 9;  // well-known echo port number


    CustomBulkSendHelper source(socket_factory,
                          InetSocketAddress(i.GetAddress(1), port));
    // Set the amount of data to send in bytes.  Zero is unlimited.
    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    // Set the amount of data to send per packet
    source.SetAttribute("SendSize", UintegerValue(send_size));
    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(0.0));
    sourceApps.Stop(Seconds(10.0));

    //
    // Create a PacketSinkApplication and install it on node 1
    //
    PacketSinkHelper sink(socket_factory,
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    //
    // Set up tracing if enabled
    //
    if (tracing) {
        AsciiTraceHelper ascii;
        pointToPoint.EnableAsciiAll(ascii.CreateFileStream("bulk-send.tr"));
        pointToPoint.EnablePcapAll("bulk-send", false);
    }

    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");

    // According to tests outputted to a PCAP file, the default file size of 1MiB takes about 0,2s
    // to transmit inside the simulation. Using a 1Mbps link, this time increses to a little bit under 10s.
    // For every realisic scenario, 10s should be okay.
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(0));
    std::cout << "Total Bytes Received: " << sink1->GetTotalRx() << std::endl;
}
