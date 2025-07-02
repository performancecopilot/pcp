# Copyright (c) 2025 Oracle and/or its affiliates.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
import argparse
from collections import defaultdict
import fcntl
import ipaddress
import select
import signal
import struct
import sys
import threading
import socket
import time
import json
from time import perf_counter
sys.path.append("/var/lib/pcp/pmdas/rds/modules")
from rds_info import RdsInfo


rds_pings_json = {}


def signal_handler(sig, frame):
    sys.exit(0)

def create_rds_socket():
    try:
        sock_fd = socket.socket(socket.PF_RDS, socket.SOCK_SEQPACKET, 0)
        return sock_fd
    except socket.error as err:
        print("socket creation failed with error %s" % (err))
        return None

def bind_rds_socket(sock_fd, saddr):
    src_port = 0
    try:
        sock_fd.bind((saddr, src_port))
    except socket.error as err:
        print("socket bind failed with error %s" % (err))


def set_tos(sock_fd, tos):
    try:
        fcntl.ioctl(sock_fd, 0x89E0, struct.pack('L', int(tos)))
    except socket.error as err:
        print("Setting QOS failed with error %s" % (err))

def validate_ip(ip_addr_str):
    try:
        ipaddress.ip_address(ip_addr_str)
        return 0
    except ValueError:
        print("Invalid IP %s" % ip_addr_str)
        return 1

def send_rds_ping(sock_fd, saddr, tos, dst_ip_list, send_timestamps):
    """
    Sends RDS pings to the destination IPs and stores timestamps in a thread-specific dictionary.
    """
    dst_port = 0
    try:
        for dst_ip in dst_ip_list:
            data = ""
            sock_fd.sendto(data.encode('utf-8'), (dst_ip, dst_port))
            timestamp = perf_counter()
            conn = f"{saddr}-{dst_ip}-{tos}"
            send_timestamps[conn] = timestamp
    except socket.error:
        pass

def recv_rds_pong(sock_fd, saddr, tos, num_dest, timeout, send_timestamps, thread_results):
    """
    Receives RDS pongs and calculates latencies based on timestamps from the thread-specific dictionary.
    """
    pongs_rcvd = 0
    inputs = [sock_fd]
    outputs = []
    sock_fd.settimeout(timeout + 1)  # Add a small buffer to the timeout

    while True:
        readable, writable, exceptional = select.select(inputs, outputs, inputs, timeout)


        for _ in readable:
            try:
                _, (ipaddr, _) = sock_fd.recvfrom(0)
                conn = f"{saddr}-{ipaddr}-{tos}"
                if conn in send_timestamps:
                    latency = perf_counter() - send_timestamps[conn]
                    thread_results[conn] = int(latency * 1_000_000)  # Convert seconds to microseconds
                    del send_timestamps[conn]
                    pongs_rcvd += 1
            except Exception:
                for conn in list(send_timestamps.keys()):
                    thread_results[conn] = -1
                break

        if num_dest - pongs_rcvd == 0:
            break

        if not (readable or writable or exceptional):
            # Timeout occurred, mark remaining connections as timed out
            for conn in list(send_timestamps.keys()):
                thread_results[conn] = -1
            break

def rds_ping_all_avlbl_dest(timeout):
    """
    Main function to send and receive RDS pings for all available destinations.
    """
    sockets_list = []
    threads_list = []
    thread_results_list = []  # List to store thread-specific results

    rds_info = RdsInfo()
    rds_connections = rds_info.main("-I").split("\n")

    conns_dict = defaultdict(list)
    for conn in rds_connections:
        try:
            saddr, daddr, tos = conn.split()[0:3]
            conns_dict[f"{saddr} {tos}"].append(daddr)
        except ValueError:
            continue

    for key, dest_ip_list in conns_dict.items():
        saddr, tos = key.split()[0:2]

        num_dest = len(dest_ip_list)
        sock_fd = create_rds_socket()
        bind_rds_socket(sock_fd, saddr)
        set_tos(sock_fd, tos)
        sockets_list.append(sock_fd)

        # Thread-specific dictionaries
        send_timestamps = {}
        thread_results = {}
        thread_results_list.append(thread_results)

        # Start recv_rds_pong thread
        thread2 = threading.Thread(target=recv_rds_pong, args=(
            sock_fd, saddr, tos, num_dest, timeout, send_timestamps, thread_results))
        thread2.start()
        threads_list.append(thread2)

        # Start send_rds_ping thread
        thread1 = threading.Thread(target=send_rds_ping, args=(
            sock_fd, saddr, tos, dest_ip_list, send_timestamps))
        thread1.start()
        threads_list.append(thread1)

    for thread in threads_list:
        thread.join()

    for sock in sockets_list:
        sock.close()

    # Merge thread-specific results into the global dictionary
    for thread_results in thread_results_list:
        rds_pings_json.update(thread_results)

    # Return the JSON-formatted result
    return json.dumps(rds_pings_json, indent=4)


def rds_ping_multiple_destinations(source_ip, destination_ips, tos, timeout):
    """
    Pings multiple RDS connections from a single source IP and returns the latencies as a string.
    """
    sock_fd = create_rds_socket()
    bind_rds_socket(sock_fd, source_ip)
    set_tos(sock_fd, tos)

    # Thread-specific dictionaries
    send_timestamps = {}
    thread_results = {}

    # Start recv_rds_pong thread
    thread2 = threading.Thread(target=recv_rds_pong, args=(
        sock_fd, source_ip, tos, len(destination_ips), timeout, send_timestamps, thread_results))
    thread2.start()

    # Start send_rds_ping thread
    thread1 = threading.Thread(target=send_rds_ping, args=(
        sock_fd, source_ip, tos, destination_ips, send_timestamps))
    thread1.start()

    # Wait for threads to complete
    thread1.join()
    thread2.join()

    sock_fd.close()

    # Format and return the latency result
    return print_latencies(thread_results)

def print_latencies(latencies_dict):
    print(f"{'Source':<20} {'Destination':<20} {'QoS':<5} {'Latency':<10}")

    for conn, latency in latencies_dict.items():
        try:
            saddr, daddr, qos = conn.split('-')
            qos = int(qos)
        except ValueError:
            print(f"Invalid connection key format: {conn}")
            continue

        latency_display = f"{latency} usec" if latency != -1 else "Timeout"
        print(f"{saddr:<20} {daddr:<20} {qos:<5} {latency_display:<10}")
    print()

def main(argv):
    parser = argparse.ArgumentParser(description='python version of rds-ping utility.',
                                     epilog="sample usage: rds-ping.py -d \"dst_ip1,dst_ip2\" -I saddr -Q tos")
    parser.add_argument('-d', '--destination_ip', type=str, help="Comma-separated list of destination IP addresses")
    parser.add_argument('-Q', '--tos', type=str, default=0, help="Type of Service (QoS)")
    parser.add_argument('-I', '--source_ip', type=str, help="Source IP address")
    parser.add_argument('-t', '--timeout', type=float, default=3, help="Timeout in seconds")
    parser.add_argument('-c', '--count', type=int, default=-1, help="Number of iterations")
    parser.add_argument('-a', '--all_connections', action='store_true', help="Ping all connections")
    args = parser.parse_args()

    if args.all_connections:
        rds_ping_all_avlbl_dest(args.timeout)
        print_latencies(rds_pings_json)
        sys.exit(1)
    elif args.source_ip and args.destination_ip:
        destination_ips = args.destination_ip.split(",")  # Split the destination IPs into a list
        try:
            if args.count == -1:
                while True:
                    _ = rds_ping_multiple_destinations(args.source_ip, destination_ips, args.tos, args.timeout)
                    time.sleep(1)
            else:
                iterations = args.count
                while iterations > 0:
                    _ = rds_ping_multiple_destinations(args.source_ip, destination_ips, args.tos, args.timeout)
                    iterations -= 1
                    if iterations:
                        time.sleep(1)
        except KeyboardInterrupt:
            sys.exit(1)
    else:
        print("Error: Both source_ip (-I) and dest_ip (-d) must be provided for single or multiple connection ping.")
        sys.exit(1)


signal.signal(signal.SIGINT, signal_handler)

if __name__ == "__main__":
    main(sys.argv[1:])
