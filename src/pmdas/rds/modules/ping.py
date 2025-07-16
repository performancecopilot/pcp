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
"""
Python implementation of ping
"""
from collections import defaultdict
import json
import socket
import sys
import os
import time
import select
from datetime import datetime
import argparse
import struct
import ipaddress
import threading
import signal
import re
import concurrent.futures
from rds_info import RdsInfo

ICMP_ECHO = 8
ICMP_MAX_RECV = 2048
RDS_INFO_LEN = 176
SOL_RDS = 276
RDS_INFO_IB_CONNECTIONS = 10008

pings_sent = 0
pending = 0
own_id = os.getpid() & 0xFFFF
pings_json = {}


def signal_handler(sig, frame):
    sys.exit(0)


def create_socket():
    try:
        sock_fd = socket.socket(
            socket.AF_INET, socket.SOCK_RAW, 1)  # 1 for ICMP
        return sock_fd
    except socket.error as err:
        print("socket creation failed with error %s" % (err), file=sys.stderr)
        return None


def bind_socket(sock_fd, src_ip):
    src_port = 0
    try:
        sock_fd.bind((src_ip, src_port))
    except socket.error as err:
        print("socket bind failed with error %s" % (err), file=sys.stderr)


def calculate_checksum(source_string):
    countTo = (int(len(source_string) / 2)) * 2
    total = 0
    count = 0

    # Handle bytes in pairs (decoding as short ints)
    loByte = 0
    hiByte = 0
    while count < countTo:
        if sys.byteorder == "little":
            loByte = source_string[count]
            hiByte = source_string[count + 1]
        else:
            loByte = source_string[count + 1]
            hiByte = source_string[count]
        total = total + ((hiByte) * 256 + (loByte))
        count += 2

    # Handle last byte if applicable (odd-number of bytes)
    # Endianness should be irrelevant in this case
    if countTo < len(source_string):  # Check for odd length
        loByte = source_string[len(source_string) - 1]
        total += loByte

    total &= 0xffffffff  # Truncate total to 32 bits (a variance from ping.c, which
    # uses signed ints, but overflow is unlikely in ping)

    total = (total >> 16) + (total & 0xffff)  # Add high 16 bits to low 16 bits
    total += (total >> 16)                  # Add carry from above (if any)
    answer = ~total & 0xffff              # Invert and truncate to 16 bits
    answer = socket.htons(answer)

    return answer


def create_rds_socket():
    try:
        sock_fd = socket.socket(socket.PF_RDS, socket.SOCK_SEQPACKET, 0)
        return sock_fd
    except socket.error as err:
        print("socket creation failed with error %s" % (err), file=sys.stderr)
        return None


def htosi(data, signed):
    hex_string = data.hex()
    return int.from_bytes(bytes.fromhex(hex_string), byteorder='little', signed=signed)


def send_ping(sock_fd, src_ip, dst_ip_list, send_timestamps):
    seq_number = 0
    global own_id
    global pings_sent

    for dst_ip in dst_ip_list:
        checksum = 0
        header = struct.pack("!BBHHH", ICMP_ECHO, 0,
                             checksum, own_id, seq_number)
        checksum = calculate_checksum(header)
        header = struct.pack("!BBHHH", ICMP_ECHO, 0,
                             checksum, own_id, seq_number)
        packet = header
        try:
            sock_fd.sendto(packet, (dst_ip, 1))
            send_timestamps[dst_ip] = datetime.now()
        except socket.error as err:
            print("socket send failed with error %s" % (err), file=sys.stderr)
            # remove the added entry from the list
            del send_timestamps[dst_ip]
        pings_sent = pings_sent + 1


def recv_pong(sock_fd, src_ip, num_dest, timeout, send_timestamps, print_all):

    inputs = [sock_fd]
    outputs = []
    latencies_info = ""
    pongs_rcvd = 0
    sock_fd.settimeout(timeout)

    select_timeout = timeout

    select_timeout_msec = select_timeout * 1000
    while True:
        select_start_time = int(round(time.time() * 1000))
        readable, writable, exceptional = select.select(
            inputs, outputs, inputs, select_timeout)

        for _ in readable:
            try:
                packet_data, (ipaddr, _) = sock_fd.recvfrom(ICMP_MAX_RECV)
                icmp_header = struct.unpack("!BBHHH", packet_data[20:28])

                if icmp_header[3] == own_id:
                    latency = datetime.now() - send_timestamps[ipaddr]
                    lat_data = ("%s %s - %.3f |" %
                                (src_ip, ipaddr, latency.microseconds))
                    latencies_info = latencies_info + lat_data
                    pongs_rcvd = pongs_rcvd + 1
                    conn = "%s-%s" % (src_ip, ipaddr)
                    pings_json[conn] = latency.microseconds
                    del send_timestamps[ipaddr]
            except KeyError:
                continue

        curr_time = int(round(time.time() * 1000))
        time_elapsed = curr_time - select_start_time
        select_timeout_msec = select_timeout_msec - time_elapsed
        if select_timeout_msec < 0:
            for keys, _ in send_timestamps.items():
                lat_data = ("%s %s - timeout |" % (src_ip, keys))
                latencies_info = latencies_info + lat_data
                conn = "%s-%s" % (src_ip, keys)
                pings_json[conn] = -1
            break

        select_timeout = select_timeout_msec/1000

        if num_dest - pongs_rcvd == 0:
            break

        if not (readable or writable or exceptional):
            for keys, _ in send_timestamps.items():
                lat_data = ("%s %s - timeout |" % (src_ip, keys))
                latencies_info = latencies_info + lat_data
                conn = "%s-%s" % (src_ip, keys)
                pings_json[conn] = -1

            break

    if not print_all:
        print(latencies_info)


def get_each_entry_len(data):
    try:
        rds_info_len = data[-4:].decode()
    except ValueError:
        rds_info_len = data[-4:-2].decode()

    if '\x00' in rds_info_len:
        rds_info_len = ' '.join(rds_info_len.split('\x00'))
    data = data[:-4]

    rds_info_len = re.findall(r'\d+', rds_info_len)
    rds_info_len = int(rds_info_len[-1])

    return data, rds_info_len

def ping_destinations(src_ip, dest_ip_list, timeout):
    sock_fd = create_socket()
    bind_socket(sock_fd, src_ip)
    send_timestamps = {}
    num_dest = len(dest_ip_list)
    send_ping(sock_fd, src_ip, dest_ip_list, send_timestamps)
    recv_pong(sock_fd, src_ip, num_dest, timeout, send_timestamps, True)
    sock_fd.close()


def ping_all_avlbl_dest(timeout):
    rds_info = RdsInfo()
    rds_connections = rds_info.main('-I').split("\n")

    conns_dict = defaultdict(list)

    for conn in rds_connections:
        saddr, daddr = conn.split()[0:2]
        if(validate_ip(saddr) == 0 and validate_ip(daddr) == 0):
            conns_dict[saddr].append(daddr)

    with concurrent.futures.ThreadPoolExecutor() as executor:
        for saddr, dest_ip_list in conns_dict.items():
            executor.submit(ping_destinations, saddr, dest_ip_list, timeout)

    res = json.dumps(pings_json)
    return res


def validate_ip(ip_addr_str):
    try:
        ipaddress.ip_address(ip_addr_str)
        return 0
    except ValueError:
        print("Invalid IP %s" % ip_addr_str, file=sys.stderr)
        return 1


def ping(args):
    dest_ip_list = [str(item) for item in args.list.split(' ')]
    for ip_addr in dest_ip_list:
        if validate_ip(ip_addr):
            print("Invalid IP being passed %s. Please rectify." % (ip_addr), file=sys.stderr)
            sys.exit(1)

    sock_fd = create_socket()
    bind_socket(sock_fd, args.source_ip)

    num_destinations = len(dest_ip_list)
    send_timestamps = {}

    thread2 = threading.Thread(target=recv_pong, args=(
        sock_fd, args.source_ip, num_destinations, args.timeout, send_timestamps, False))
    thread2.start()

    thread1 = threading.Thread(target=send_ping, args=(
        sock_fd, args.source_ip, dest_ip_list, send_timestamps))
    thread1.start()

    thread2.join()
    thread1.join()

    sock_fd.close()


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument(dest='destination_ip',
                        help="destination ip is mandatory")
    parser = argparse.ArgumentParser(description='python version of ping utility.',
                                     epilog="sample usage: ping.py -d \"dst_ip_1 dst_ip_2\" -I src_ip")
    parser.add_argument('-d', '--list', type=str)
    parser.add_argument('-I', '--source_ip', type=str)
    parser.add_argument('-t', '--timeout', type=float, default=3)
    parser.add_argument('-a', '--auto', action='store_true')
    parser.add_argument('-c', '--count', type=int, default=-1)

    args = parser.parse_args()

    if args.auto:
        ping_all_avlbl_dest(args.timeout)
        print(pings_json)
        sys.exit(1)

    else:
        if args.count == -1:
            while True:
                ping(args)
                time.sleep(1)
        else:
            iterations = args.count
            while iterations > 0:
                ping(args)
                iterations = iterations - 1
                if iterations:
                    time.sleep(1)


signal.signal(signal.SIGINT, signal_handler)

if __name__ == "__main__":
    main(sys.argv[1:])
