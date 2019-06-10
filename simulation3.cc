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
//       n0 ~~~~~~~~~~~~ n1
//           adhoc wifi
//
// - Flow from n0 to n1 using BulkSendApplication.
// - Tracing of queues and packet receptions to file "bulk-send.tr"
//   and pcap tracing available when tracing is turned on.


// Kommunikation in verteilten Systemen - Simulation Model 2
// This code is basically simulation 1 with point to point replaced with wifi.
// The wifi parts are mostly inspired by the code from the wifi-simple-adhoc example.
// The custom-bulk-send-application files are taken from ns3's bulk send application code
// (bulk-send-{helper,application}.{cc,h})

// This simulation sets up a WiFi connection between two nodes and tries to send
// as many TCP or UDP packets with a configurable size (send_size) as it can, until it has sent
// maxBytes bytes.

// The following changes to the example/simulation 1 code were made:
// WiFi transmission mode & distance between the two nodes are configurable via the command line
// This code uses the IEEE 802.11g standard, just like the specified TP Link router
// Export of Wifi frames to PCAP/ASCII is now possible
// ItuR1411LosPropagationLossModel is used, as it seemed to describe the target scenario the best (see documenation below)
// BulkSendApplication can now use Udp Sockets by manually scheduling the transmit of packets

#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "custom-bulk-send-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BulkSendExample");

int
main(int argc, char *argv[]) {
    // This activates packet logging to ascii and pcap files
    bool tracing = false;
    // This activates verbose wifi logging
    bool logging = false;
    // Default: Stop after sending approx. 1 MiB of data
    uint32_t maxBytes = 1048576;
    // Default: 512 Bytes per packet
    uint32_t send_size = 1024;
    // We simulate a tcp connection (example's default). Can be changed to `ns3::UdpSocketFactory`.
    std::string socket_factory = "ns3::TcpSocketFactory";
    // Wifi Rate - get those from src/wifi/model/wifi-phy.cc for IEEE802.11g
    // Examples: ErpOfdmRate{48 36 48 18 12 9 6}Mbps
    std::string wifi_transmission_mode = "ErpOfdmRate54Mbps";
    // Distance between simulated nodes
    double distance = 5.0;

    //
    // Allow the user to override any of the defaults at
    // run-time, via command-line arguments
    //
    CommandLine cmd;
    cmd.AddValue("tracing", "Flag to enable/disable tracing", tracing);
    cmd.AddValue("logging", "Flag to enable/disable logging", logging);
    cmd.AddValue("maxBytes", "Total number of bytes for application to send", maxBytes);
    cmd.AddValue("send_size", "Bytes sent per packet", send_size);
    cmd.AddValue("socket_factory", "Socket Factory to use. Default is ns3::TcpSocketFactory", socket_factory);
    cmd.AddValue("wifi_transmission_mode", "WiFi transmission mode to use for 802.11g: ErpOfdmRate{48 36 48 18 12 9 6}Mbps", wifi_transmission_mode);
    cmd.AddValue("distance", "Distance between simulated nodes", distance);
    cmd.Parse(argc, argv);

    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(2);

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
    // Cited from ConstantRateWifiManager class documentation:
    // "use constant rates for data and control transmissions
    //
    // This class uses always the same transmission rate for every packet sent."
    //

    wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    // Do place the nodes into 'free air', so that we do not get any reflections (ex. ground)
    positionAlloc->Add(Vector (100.0, 100.0, 100.0));
    positionAlloc->Add(Vector (distance + 100.0, 100.0, 100.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

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
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(0));
    std::cout << "Total Bytes Received: " << sink1->GetTotalRx() << std::endl;
}
