# Simulation Model 3

WiFi, variable distances. This submission is largely based on simulation1, simulation2 and ns3's simple-wifi-adhoc example.
Please find the changes made inside the comments at the top of `simulation3.cc`.

Additional hint: It makes sense to use the `--tracing` flag when running the simulation as it produces nice and
timestamped PCAP and ASCII files including the total time to transmit.

## How To Build

### CMake

First of all, you need to set the path to ns3's build directory inside the CMakeLists.txt.
At line 5, there's currently this line:

```
set(NS3BUILDDIR /home/marco/Anwendungen/ns3/ns-3.29/build)
```

Please change it accordingly. Afterwards, you can build the simulation using:

```
mkdir build && cd build && cmake .. && make
```

### g++/clang++
This should work:

```
export NS3BUILDDIR=/home/marco/Anwendungen/ns3/ns-3.29/build
g++ simulation3.cc custom-bulk-send-application.cc custom-bulk-send-helper.cc -L${NS3BUILDDIR}/lib -lns3.29-core-debug -lns3.29-stats-debug -lns3.29-network-debug -lns3.29-mobility-debug -lns3.29-mpi-debug -lns3.29-bridge-debug -lns3.29-antenna-debug -lns3.29-propagation-debug -lns3.29-traffic-control-debug -lns3.29-internet-debug -lns3.29-spectrum-debug -lns3.29-config-store-debug -lns3.29-energy-debug -lns3.29-wifi-debug -lns3.29-point-to-point-debug -lns3.29-csma-debug -lns3.29-applications-debug -lns3.29-fd-net-device-debug -lns3.29-buildings-debug -lns3.29-virtual-net-device-debug -lns3.29-lte-debug -lns3.29-lr-wpan-debug -lns3.29-point-to-point-layout-debug -lns3.29-uan-debug -lns3.29-internet-apps-debug -lns3.29-wave-debug -lns3.29-wimax-debug -lns3.29-flow-monitor-debug -lns3.29-sixlowpan-debug -lns3.29-olsr-debug -lns3.29-dsr-debug -lns3.29-csma-layout-debug -lns3.29-mesh-debug -lns3.29-nix-vector-routing-debug -lns3.29-test-debug -lns3.29-aodv-debug -lns3.29-dsdv-debug -lns3.29-tap-bridge-debug -lns3.29-netanim-debug -lns3.29-topology-read-debug -lns3.29-antenna-test-debug -lns3.29-buildings-test-debug -lns3.29-applications-test-debug -lns3.29-aodv-test-debug -lns3.29-flow-monitor-test-debug -lns3.29-dsdv-test-debug -lns3.29-energy-test-debug -lns3.29-dsr-test-debug -lns3.29-core-test-debug -lns3.29-internet-test-debug -lns3.29-internet-apps-test-debug -lns3.29-lr-wpan-test-debug -lns3.29-lte-test-debug -lns3.29-mesh-test-debug -lns3.29-mobility-test-debug -lns3.29-network-test-debug -lns3.29-netanim-test-debug -lns3.29-olsr-test-debug -lns3.29-point-to-point-test-debug -lns3.29-propagation-test-debug -lns3.29-sixlowpan-test-debug -lns3.29-stats-test-debug -lns3.29-spectrum-test-debug -lns3.29-topology-read-test-debug -lns3.29-uan-test-debug -lns3.29-traffic-control-test-debug -lns3.29-wave-test-debug -lns3.29-wifi-test-debug -lns3.29-wimax-test-debug -lns3.29-test-test-debug -std=c++11 -I${NS3BUILDDIR} -Wall -o simulation3
```

### ns3's build system

Unfortunately, I did not manage to get this to compile. It could work by putting the custom-bulk-send-* files from
this submission into the same folders from which their "original ns3 counterparts" originated from and updating the
include directive at the top of `simulation3.cc`.

## How To Run
It is possible you have to change your `LD_LIBRARY_PATH` before running the application to tell your system
where to find ns3's libraries.

```
export NS3BUILDDIR=/home/marco/Anwendungen/ns3/ns-3.29/build
LD_LIBRARY_PATH=${NS3BUILDDIR}/lib ./simulation3 --tracing
```