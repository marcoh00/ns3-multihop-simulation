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
//                                              Wifi
//                          - distance -    - distance -   - distance -
//                       ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//       l1 ------------ r1              r2             r3             r4 ------------ l2
//           p2p                                                            p2p
//           10.1.1.0/24                    10.1.2.0/24                     10.1.10.0/24
//
// - Flow from n0 to n1 using BulkSendApplication.
// - Tracing of queues and packet receptions to file "bulk-send.tr"
//   and pcap tracing available when tracing is turned on.


// Kommunikation in verteilten Systemen - Simulation Model 3
// This code is based on the simulation models 1 and 2.
// The wifi parts are mostly inspired by the code from the wifi-simple-adhoc example.
// The custom-bulk-send-application files are taken from ns3's bulk send application code
// (bulk-send-{helper,application}.{cc,h})
// The static routing parts are inspired by the simple-global-routing example.
// OLSR setup was taken from manet-routing-compare example

// This simulation sets up a WiFi and point to point connections as shown above.
// After that, it sets up static routing tables (and olsr if --olsr is specified).
// When using OLSR the static tables only tell l1 how to get to r1 and r1 is supposed to get its route to r4
// via OLSR. When using static routing it sets up routing tables such that r1 -> r2 -> r3 -> r4.
// Unfortunately OLSR routing doesn't work yet, as the program segfaults somewhere inside ns3's code. I was not able
// to fix this yet.
// The program proceeds by sending as many TCP or UDP packets with a configurable size (send_size) as it can,
// until it has sent maxBytes bytes.

// The following changes to the example/simulation 1+2 code were made:
// Set all variables specified on the practice sheet and use MinstrelWifiManager
// Set up more nodes, subnets and routing between them

#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/olsr-helper.h"
#include "custom-bulk-send-helper.h"
#include "custom-bulk-send-application.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BulkSendExample");

void PhyRxDrop(Ptr<const Packet> p) {
    std::cout << "PHY RX DROP" << std::endl;
}

void PhyTxDrop(Ptr<const Packet> p) {
    std::cout << "PHY TX DROP" << std::endl;
}

void MacTxDrop(Ptr<const Packet> p) {
    std::cout << "MAC TX DROP" << std::endl;
}
void MacRxDrop(Ptr<const Packet> p) {
    std::cout << "MAC RX DROP" << std::endl;
}

ns3::Time last_time_tx;
uint64_t packet_count_tx = 0;
void TxPacket(Ptr<const Packet> packet) {
    last_time_tx = Simulator::Now();
    packet_count_tx++;
}

ns3::Time last_time_rx;
uint64_t packet_count_rx = 0;
void RecvPacket(Ptr<const Packet> packet, const Address & address) {
    last_time_rx = Simulator::Now();
    packet_count_rx++;
}

int
main(int argc, char *argv[]) {
    // This activates packet logging to ascii and pcap files
    bool tracing = false;
    // This activates verbose wifi logging
    bool logging = false;
    // Use OLSR for wifi routing
    bool olsr = false;
    // Default: Stop after sending approx. 1 MiB of data
    uint32_t maxBytes = 1048576;
    // Default: 512 Bytes per packet
    uint32_t send_size = 1024;
    // We simulate a tcp connection (example's default). Can be changed to `ns3::UdpSocketFactory`.
    std::string socket_factory = "ns3::TcpSocketFactory";
    // Wifi Rate - get those from src/wifi/model/wifi-phy.cc for IEEE802.11g
    // Examples: ErpOfdmRate{48 36 48 18 12 9 6}Mbps
    // Distance between simulated nodes
    double distance = 50.0;
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
    cmd.AddValue("logging", "Flag to enable/disable logging", logging);
    cmd.AddValue("olsr", "Use OLSR for wifi routing", olsr);
    cmd.AddValue("maxBytes", "Total number of bytes for application to send", maxBytes);
    cmd.AddValue("send_size", "Bytes sent per packet", send_size);
    cmd.AddValue("socket_factory", "Socket Factory to use. Default is ns3::TcpSocketFactory", socket_factory);
    cmd.AddValue("distance", "Distance between simulated nodes", distance);
    cmd.AddValue("data_rate", "Point-to-point link data rate", data_rate);
    cmd.AddValue("delay", "Point-to-Point connection delay", delay);
    cmd.Parse(argc, argv);

    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");

    NodeContainer routers;
    routers.Create(4);

    //
    // Setup Wifi
    // This is done like shown in the wifi-simple-adhoc example
    //

    WifiHelper wifi;

    if(logging) {
        wifi.EnableLogComponents();
    }

    // Use 802.11g, like the specified TP Link Router
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();

    // Set Wifi parameters like specified
    wifiPhy.Set("ChannelWidth", UintegerValue(20));
    wifiPhy.Set("TxGain", DoubleValue(1.0));
    wifiPhy.Set("RxGain", DoubleValue(1.0));
    wifiPhy.Set("TxPowerStart", DoubleValue(1.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(1.0));
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

    //
    // "This class implements the ITU-R 1411 LOS propagation model for Line-of-Sight (LoS) short range outdoor
    // communication in the frequency range 300 MHz to 100 GHz."
    // => This means we assume the scenario stated above!
    //

    wifiChannel.AddPropagationLoss("ns3::ItuR1411LosPropagationLossModel", "Frequency", DoubleValue(2400.0 * 1e6));
    // wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (-80));
    wifiPhy.SetChannel(wifiChannel.Create());

    ///
    // Use MinstrelWifiManager like the exercise suggests
    //

    wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    // Do place the nodes into 'free air', so that we do not get any reflections (ex. ground)
    positionAlloc->Add(Vector (100.0, 100.0, 100.0));
    for(auto i = 1; i < 4; i++) {
        positionAlloc->Add(Vector(((double)i * distance) + 100.0, 100.0, 100.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(routers);

    //
    // Ethernet Connections
    //

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(data_rate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer firstHopDevices = pointToPoint.Install(firstHop);
    NetDeviceContainer routerDevices = wifi.Install(wifiPhy, wifiMac, routers);
    NetDeviceContainer lastHopDevices = pointToPoint.Install(lastHop);

    //
    // Install the internet stack with OLSR on the nodes (IP)
    //
    OlsrHelper olsrhelper;
    InternetStackHelper internet;
    internet.Install(laptops);
    if(olsr) internet.SetRoutingHelper(olsrhelper);
    internet.Install(routers);

    //
    // We've got the "hardware" in place.  Now we need to add IP addresses.
    //
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer routerNet = ipv4.Assign(routerDevices);


    if(!olsr) {
        //
        // Set up static routing to the packets get routed along the 4 different routers
        //

        Ptr<Ipv4> router1addr = routers.Get(0)->GetObject<Ipv4>();
        Ptr<Ipv4> router2addr = routers.Get(1)->GetObject<Ipv4>();
        Ptr<Ipv4> router3addr = routers.Get(2)->GetObject<Ipv4>();
        Ptr<Ipv4> router4addr = routers.Get(3)->GetObject<Ipv4>();
        Ipv4StaticRoutingHelper staticRoutingHelper;

        // Router 1 to Router 4 via router 2
        Ptr<Ipv4StaticRouting> r1_l1tol2 = staticRoutingHelper.GetStaticRouting(router1addr);
        r1_l1tol2->AddHostRouteTo(Ipv4Address("10.1.2.4"), Ipv4Address("10.1.2.2"), 1);

        // Router 2 to Router 4 via router 3
        Ptr<Ipv4StaticRouting> r2_l1tol2 = staticRoutingHelper.GetStaticRouting(router2addr);
        r2_l1tol2->AddHostRouteTo(Ipv4Address("10.1.2.4"), Ipv4Address("10.1.2.3"), 1);

        // Router 3 to Router 1 via via router 2
        Ptr<Ipv4StaticRouting> r3_l1tol2 = staticRoutingHelper.GetStaticRouting(router3addr);
        r3_l1tol2->AddHostRouteTo(Ipv4Address("10.1.2.1"), Ipv4Address("10.1.2.2"), 1);

        // Router 4 to Router 1 via via router 3
        Ptr<Ipv4StaticRouting> r4_l1tol2 = staticRoutingHelper.GetStaticRouting(router4addr);
        r4_l1tol2->AddHostRouteTo(Ipv4Address("10.1.2.1"), Ipv4Address("10.1.2.3"), 1);
    }

    NS_LOG_INFO("Create Applications.");

    //
    // Create a BulkSendApplication and install it on laptop 1
    // A BulkSendApplication sends as many packets as fast as it can
    // until it reaches a certain, configurable, limit.
    //
    uint16_t port = 9;  // well-known echo port number


    CustomBulkSendHelper source(socket_factory,
                                InetSocketAddress(Ipv4Address("10.1.2.4"), port));
    // Set the amount of data to send in bytes.  Zero is unlimited.
    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    // Set the amount of data to send per packet
    source.SetAttribute("SendSize", UintegerValue(send_size));
    ApplicationContainer sourceApps = source.Install(routers.Get(0));
    sourceApps.Start(Seconds(30.0));
    sourceApps.Stop(Seconds(90.0));
    Ptr<CustomBulkSendApplication> source1 = DynamicCast<CustomBulkSendApplication>(sourceApps.Get(0));
    source1->TraceConnectWithoutContext("Tx", MakeCallback(&TxPacket));

    //
    // Create a PacketSinkApplication and install it on laptop 2
    //
    PacketSinkHelper sink(socket_factory,
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(routers.Get(3));

    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(90.0));

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(0));
    sink1->TraceConnectWithoutContext("Rx", MakeCallback(&RecvPacket));

    //
    // Set up tracing if enabled
    //
    if (tracing) {
        AsciiTraceHelper ascii;
        wifiPhy.EnableAsciiAll(ascii.CreateFileStream("bulk-send.tr"));
        wifiPhy.EnablePcapAll("bulk-send", false);
    }

    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");

    // According to tests outputted to a PCAP file, the default file size of 1MiB takes about 0,2s
    // to transmit inside the simulation. Using a 1Mbps link, this time increses to a little bit under 10s.
    // For every realisic scenario, 10s should be okay.

    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop", MakeCallback(&MacTxDrop));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxDrop", MakeCallback(&PhyTxDrop));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop", MakeCallback(&PhyRxDrop));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MacRxDrop", MakeCallback(&MacRxDrop));
    Config::ConnectWithoutContext("/NodeList/*/$ns3::UdpL4Protocol/SocketList/*/Drop", MakeCallback(&MacRxDrop));

    Simulator::Stop(Seconds(180.0));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    
    std::cout << "Total Bytes Received: " << sink1->GetTotalRx() << " (" << ((double)sink1->GetTotalRx() / maxBytes) * 100.0 << "%)" << std::endl;
    std::cout << "Total packets received: " << packet_count_rx << std::endl;
    std::cout << "Last packet received at: " << last_time_rx.GetMilliSeconds() << "ms" << std::endl;
    std::cout << std::endl;
    std::cout << "Total packets sent: " << packet_count_tx << std::endl;
    std::cout << "Last packet sent at: " << last_time_tx.GetMilliSeconds() << "ms" << std::endl;
}
