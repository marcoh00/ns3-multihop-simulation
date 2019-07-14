#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from datetime import datetime
import csv
import matplotlib.pyplot as plt
import subprocess
import json
import sys
from multiprocessing.dummy import Pool as ThreadPool

def simulate(run_app):
    print(f'Run simulation: {run_app}')
    proc = subprocess.run(run_app, stdout=subprocess.PIPE)
    result = json.loads(proc.stdout)
    print(f'Result was {json.dumps(result, indent=2)}')
    return result

def tcp_throughput(comparison=False, routing=None):
    heights = (1, 100) if not comparison else (100,)
    data_sizes = (10000, 1000000, 20000000) if not comparison else (20000000,)
    distances = (3, 6, 12, 25, 75, 100, 150, 200, 400, 600)
    start_time = 10260

    test_results = []

    for height in heights:
        for size in data_sizes:
            for distance in distances:
                run_app = ['./simulation3', f'--height={height}', f'--maxBytes={size}', f'--distance={distance}']
                if routing:
                    run_app.append(f'--{routing}')
                result = simulate(run_app)
                if not result['rx_bytes_application'] == 0:
                    time_taken = (result['rx_ms_last'] - start_time) / 1000
                    data_transferred = result['rx_bytes_application'] / 1000
                    throughput = data_transferred / time_taken
                    print(f'Throughput: {throughput} kB/s')

                    test_results.append({
                        'distance': distance,
                        'throughput': throughput,
                        'size': size,
                        'height': height,
                        'command_line': run_app,
                        'raw_data': result
                    })
    with open('tcp_tests.json' if not comparison else 'tcp_comparison.json', 'w') as fp:
        json.dump(test_results, fp)

def udp_packet_loss(tcp_comparison=False, routing=False):
    intervals = (10, 100) if not tcp_comparison else (100,)
    counts = (10, 100, 1000) if not tcp_comparison else (1000,)
    distances = (3, 6, 12, 25, 75, 100, 150, 200, 400, 600)
    heights = (1, 100) if not tcp_comparison else (100,)
    start_time = 10260

    test_results = {
        3: [],
        6: [],
        12: [],
        25: [],
        75: [],
        100: [],
        150: [],
        200: [],
        400: [],
        600: []
    }

    alltasks = len(intervals) * len(counts) * len(distances) * len(heights)
    current = 1

    for height in heights:
        for interval in intervals:
            for count in counts:
                for distance in distances:
                    max_bytes = 1000000 if not tcp_comparison else 20000000
                    run_app = ['./simulation3', f'--height={height}', f'--maxBytes={max_bytes}', f'--distance={distance}', f'--udp_interval={interval}', f'--udp_count={count}', '--socket_factory=ns3::UdpSocketFactory']
                    if routing:
                        run_app.append(f'--{routing}')
                    result = simulate(run_app)
                    print(f"=====> {datetime.now()} {current}/{alltasks} ({(current / alltasks) * 100})")
                    current += 1
                    if not result['rx_bytes_application'] == 0:
                        time_taken = (result['rx_ms_last'] - start_time) / 1000
                        data_transferred = result['rx_bytes_application'] / 1000
                        throughput = data_transferred / time_taken
                        print(f'Throughput: {throughput} kB/s')
                        percent_arrived = (result['rx_count_packets'] / result['tx_count_packets']) * 100
                        print(f"{percent_arrived}% of packets arrived")
                        test_results[distance].append({
                            'throughput': throughput,
                            'arrived': percent_arrived,
                            'count': count,
                            'interval': interval,
                            'height': height,
                            'command_line': run_app,
                            'raw_data': result
                        })
    with open('udp_loss.json' if not tcp_comparison else 'udp_comparison.json', 'w') as fp:
        json.dump(test_results, fp)

def map_on_index(element, iterable):
    for i in range(0, len(iterable)):
        if int(element) == int(iterable[i]):
            return i

def tcp_throughput_graph():
    data = None
    with open('tcp_tests.json', 'r') as fp:
        data = json.load(fp)
    """
        0 height 1m 10kb
        1 height 1m 1mb
        2 height 1m 20mb
        3 height 100m 10kb
        4 height 100m 1mb
        5 height 100m 20mb 3 6 12 25 75 100
    """
    mapping = {
        1: {10000: 0, 1000000: 1, 20000000: 2},
        100: {10000: 3, 1000000: 4, 20000000: 5}
    }
    distances = (3, 6, 12, 25, 75, 100, 150, 200, 400, 600)
    throughputs = [[], [], [], [], [], []]
    for throughput in throughputs:
        for distance in distances:
            throughput.append(0)

    for datapoint in data:
        throughputs[
            mapping[datapoint['height']][datapoint['size']]
        ][map_on_index(datapoint['distance'], distances)] = datapoint['throughput']
    
    print(json.dumps(throughputs, indent=2))

    plt.plot(distances, throughputs[0], '-o', label='Height = 1m, 10kB')
    plt.plot(distances, throughputs[1], '-o', label='Height = 1m, 1MB')
    plt.plot(distances, throughputs[2], '-o', label='Height = 1m, 20MB')
    plt.plot(distances, throughputs[3], '-o', label='Height = 100m, 10kB')
    plt.plot(distances, throughputs[4], '-o', label='Height = 100m, 1MB')
    plt.plot(distances, throughputs[5], '-o', label='Height = 100m, 20MB')
    plt.xlabel("Distance (m)")
    plt.ylabel("Throughput (kB/s)")
    plt.title("TCP Throughput")
    plt.legend()
    plt.show()

def udp_count_interval_graph(metric, target_height, ylabel, title, noval=100):
    """
    0 10ms 10 pck
    1 10ms 1000 pck
    2 10ms 1000 pck
    3 100ms 10 pck
    4 100ms 100 pck
    5 100ms 1000 pck
    """
    mapping = {
        10: {10: 0, 100: 1, 1000: 2},
        100: {10: 3, 100: 4, 1000: 5}}
    distances = (3, 6, 12, 25, 75, 100, 150, 200, 400, 600)
    losses = [[], [], [], [], [], []]
    for loss in losses:
        for distance in distances:
            loss.append(noval)
    data = None
    with open('udp_loss.json', 'r') as fp:
        data = json.load(fp)
    
    for distance, datapoints in data.items():
        for datapoint in datapoints:
            print(distance)
            if datapoint['height'] == target_height:
                losses[
                    mapping[datapoint['interval']][datapoint['count']]
                ][map_on_index(distance, distances)] = datapoint[metric]
    print(json.dumps(losses, indent=2))

    plt.plot(distances, losses[0], '-o', label='10 Packets every 10ms')
    plt.plot(distances, losses[1], '-o', label='100 Packets every 10ms')
    plt.plot(distances, losses[2], '-o', label='1000 Packets every 10ms')
    plt.plot(distances, losses[3], '-o', label='10 Packets every 100ms')
    plt.plot(distances, losses[4], '-o', label='100 Packets every 100ms')
    plt.plot(distances, losses[5], '-o', label='1000 Packets every 100ms')
    plt.xlabel("Distance (m)")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.legend()
    plt.show()

def packet_loss_throughput():
    data = None
    with open('udp_loss.json', 'r') as fp:
        data = json.load(fp)
    
    x_axis = []
    y_axis = []
    graphdict = {}

    for key in data:
        for datapoint in data[key]:
            x_axis.append(datapoint['arrived'])
            y_axis.append(datapoint['throughput'])
            graphdict[datapoint['arrived']] = datapoint['throughput']
    graphkeys = list(graphdict.keys())
    graphkeys.sort()
    graphvalues = []
    for key in graphkeys:
        graphvalues.append(graphdict[key])
    plt.plot(graphkeys, graphvalues, '-o')
    plt.show()

def comparison_graph(tcpfiles=['tcp_comparison.json',], udpfiles=['udp_comparsion.json',], titles=["Comparison of TCP and UDP throughput (h=100m, s=20MB)",]):
    assert len(tcpfiles) == len(udpfiles)
    assert len(tcpfiles) == len(titles)
    data_tcp = list()
    data_udp = list()
    data_olsr = None
    y_tcp = []
    y_udp = []

    for tcpfile in tcpfiles:
        with open(tcpfile, 'r') as fp:
            data_tcp.append(json.load(fp))
        y_tcp.append([0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
        
    for udpfile in udpfiles:
        with open(udpfile, 'r') as fp:
            data_udp.append(json.load(fp))
        y_udp.append([0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
    
    with open('udp_comparison_speed.json', 'r') as fp:
        data_olsr = json.load(fp)
    
    distances = (3, 6, 12, 25, 75, 100, 150, 200, 400, 600)
    y_udpreal = [0 for d in distances]

    for idx, measurement in enumerate(data_tcp):
        for datapoint in measurement:
            y_tcp[idx][map_on_index(datapoint['distance'], distances)] = datapoint['throughput']
    for idx, measurement in enumerate(data_udp):
        for distance, datapoints in measurement.items():
            if len(datapoints) > 0:
                y_udp[idx][map_on_index(distance, distances)] = datapoints[0]['throughput']
    for distance, ms in data_olsr.items():
        y_udpreal[map_on_index(distance, distances)] = ms[0]['throughput']
    
    fig, axs = plt.subplots(len(tcpfiles), 1, sharex=True, sharey=True)
    if len(tcpfiles) == 1:
        axs = (axs,)
    for i in range(0, len(tcpfiles)):
        axs[i].plot(distances, y_tcp[i], '-o', label='TCP')
        axs[i].plot(distances, y_udp[i], '-o', label='UDP')
        axs[i].plot(distances, y_udpreal, '-o', label='UDP (OLSR)')
        axs[i].set_xlabel("Distance (m)")
        axs[i].set_ylabel("Throughput (kB/s)")
        axs[i].set_title(titles[i])
        axs[i].legend()
    fig.tight_layout()
    plt.show()

def olsr_graph(filename):
    measurements = []
    with open(filename, 'r') as fp:
        data = csv.reader(fp)
        for line in data:
            measurements.append({
                "distance": int(line[0]),
                "convergence": int(line[1]),
                "hops": int(line[2]),
                "height": int(line[3])
            })
    print(measurements)

    distances = (3, 6, 12, 25, 75, 100, 150, 200, 400, 600)
    # 0 1m, 1 100m
    y_values_conv = [[], []]
    y_values_hops = [[], []]

    all_value_distances = [d for i in range(0, 2) for d in distances]
    colors = []
    for d in distances:
        colors.append('blue')
    for d in distances:
        colors.append('green')
    
    for distance in distances:
        y_values_conv[0].append(None)
        y_values_conv[1].append(None)
        y_values_hops[0].append(None)
        y_values_hops[1].append(None)
    for measurement in measurements:
        idx = 0 if measurement['height'] == 1 else 1
        if not measurement['hops'] == 0:
            y_values_conv[idx][map_on_index(measurement['distance'], distances)] = measurement['convergence']
            y_values_hops[idx][map_on_index(measurement['distance'], distances)] = measurement['hops']
    
    fig, axs = plt.subplots(2, 1, sharex=True)
    axs[0].plot(distances, y_values_conv[0], '-o', label='h=1m')
    axs[0].plot(distances, y_values_conv[1], '-o', label='h=100m')
    axs[0].set_xlabel("Distance (m)")
    axs[0].set_ylabel("Convergence Time (ms)")
    axs[0].set_title("OLSR Convergence Time")
    axs[0].legend()

    axs[1].plot(distances, y_values_hops[0], '-o', label='h=1m')
    axs[1].plot(distances,  y_values_hops[1], '-o', label='h=100m')
    axs[1].set_xlabel("Distance (m)")
    axs[1].set_ylabel("Maxmium Number of Hops")
    axs[1].set_title("OLSR Routes Hop Count")
    axs[1].legend()

    plt.tight_layout()
    plt.show()

def main():
    if sys.argv[1] == 'tcp':
        if len(sys.argv) > 2:
            tcp_throughput(True, sys.argv[2])
        else:
            tcp_throughput(False, 'olsr')
    elif sys.argv[1] == 'tcpgraph':
        tcp_throughput_graph()
    elif sys.argv[1] == 'udploss':
        if len(sys.argv) > 2:
            udp_packet_loss(True, sys.argv[2])
        else:
            udp_packet_loss(False, 'olsr')
    elif sys.argv[1] == 'udplossgraph':
        udp_count_interval_graph("arrived", int(sys.argv[2]), "Packets arrived at receiver (%)", "UDP Packet Loss")
    elif sys.argv[1] == 'udpthroughgraph':
        udp_count_interval_graph("throughput", int(sys.argv[2]), "Throughput (kB/s)", f"UDP Throughput using different sending patterns (h={sys.argv[2]}m)", noval=0)
    elif sys.argv[1] == 'plthrough':
        packet_loss_throughput()
    elif sys.argv[1] == 'cpgraph':
        comparison_graph(["tcp_tests.json",], ["udp_comparison_speed.json",])
    elif sys.argv[1] == 'routesgraph':
        comparison_graph(
            ["tcp_comparison_1hop.json", "tcp_comparison_3hop.json"],
            ["udp_comparison_1hop.json", "udp_comparison_3hop.json"],
            ["Throughput comparison using static 1-hop route (h=100m, s=20MB)", "Throughput comparison using static 3-hop route (h=100m, s=20MB)"]
        )
    elif sys.argv[1] == 'olsrgraph':
        olsr_graph('olsr.csv')

main()