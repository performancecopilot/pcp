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
import sys
import struct
import psutil
import socket
import ctypes
import binascii
from datetime import datetime

# Constants for socket operations
PF_RDS = 21
SOCK_SEQPACKET = 5
SOL_RDS = 276

RDS_INFO_COUNTERS = 10000
RDS_INFO_CONNECTIONS = 10001
RDS_INFO_SEND_MESSAGES = 10003
RDS_INFO_RETRANS_MESSAGES = 10004
RDS_INFO_RECV_MESSAGES = 10005
RDS_INFO_SOCKETS = 10006
RDS_INFO_TCP_SOCKETS = 10007
RDS_INFO_IB_CONNECTIONS = 10008
RDS_INFO_CONN_PATHS = 10020

# Connection drop reasons
CONN_DROP_REASONS = [
    "--", "user reset", "invalid connection state", "failure to move to DOWN state",
    "connection destroy", "conn_connect failure", "hb timeout", "reconnect timeout",
    "cancel operation on socket", "race between ESTABLISHED event and drop",
    "conn is not in CONNECTING state", "qp event", "incoming REQ in CONN_UP state",
    "incoming REQ in CONNECTING state", "passive setup_qp failure",
    "rdma_accept failure", "active setup_qp failure", "rdma_connect failure",
    "resolve_route failure", "detected rdma_cm_id mismatch", "ROUTE_ERROR event",
    "ADDR_ERROR event", "CONNECT_ERROR or UNREACHABLE or DEVICE_REMOVE event",
    "CONSUMER_DEFINED reject", "REJECTED event", "ADDR_CHANGE event",
    "DISCONNECTED event", "TIMEWAIT_EXIT event", "post_recv failure",
    "send_ack failure", "no header in incoming msg", "corrupted header in incoming msg",
    "fragment header mismatch", "recv completion error", "send completion error",
    "post_send failure", "rds_rdma module unload", "active bonding failover",
    "corresponding loopback conn drop", "active bonding failback", "sk_state to TCP_CLOSE",
    "tcp_send failure"
]

class RdsInfo:
    """Class to implement standard RDS info operations."""

    libc = ctypes.CDLL("libc.so.6", use_errno=True)

    def create_rds_socket(self):
        sock_fd = self.libc.socket(PF_RDS, SOCK_SEQPACKET, 0)
        if sock_fd < 0:
            return None
        return sock_fd

    def get_rds_info_data(self, sock_fd, query_type):
        """Retrieve RDS information data."""
        data_len = ctypes.c_int(0)
        res = self.libc.getsockopt(sock_fd, SOL_RDS, query_type, None, ctypes.byref(data_len))
        if res < 0:
            data_buffer = ctypes.create_string_buffer(int(data_len.value))
            res = self.libc.getsockopt(sock_fd, SOL_RDS, query_type, data_buffer, ctypes.byref(data_len))
            if res < 0:
                return None

            return data_buffer.raw[:data_len.value], res

        return None, None

    @staticmethod
    def little_endian_to_unsigned(hex_string, bit):
        """Convert little-endian byte sequence to an unsigned integer."""
        bit_format = "<I" if bit == 32 else "<Q"
        byte_array = binascii.unhexlify(hex_string)
        return struct.unpack(bit_format, byte_array)[0]

    @staticmethod
    def htosi(data, signed):
        """Convert a hexadecimal string to a signed/unsigned integer."""
        hex_string = data.hex()
        return int.from_bytes(bytes.fromhex(hex_string), byteorder="little", signed=signed)

    @staticmethod
    def to_ipv6(data):
        """Convert IPv4 to IPv6."""
        data = data.strip("0")
        res = data[4:12]
        octets = [res[i:i+2] for i in range(0, len(res), 2)]
        ip_addr = ".".join(str(int(i, 16)) for i in octets)
        return f"::{data[:4]}:{ip_addr}"

    @staticmethod
    def parse_time(hex_time):
        """
        Convert the time to a human-readable format
        """
        time = ""
        time = float(struct.unpack("<Q", hex_time)[0])
        time = "---" if time == float(0) else (datetime.fromtimestamp(time))
        return time

    @staticmethod
    def get_down_time(conn_time, attempt_time):
        """Calculate down time."""
        conn_time = float(struct.unpack("<Q", conn_time)[0])
        attempt_time = float(struct.unpack("<Q", attempt_time)[0])
        return conn_time - attempt_time

    @staticmethod
    def decode_flags(value):
        """Decode RDS Flags."""
        flags = "".join([char if value & (1 << i) else "-" for i, char in enumerate("scCE")])
        return flags

    def get_rds_ib_conns(self, sock_fd):
        """Get RDS_INFO_IB_CONNECTIONS from socket and parse it."""
        data, each = self.get_rds_info_data(sock_fd, RDS_INFO_IB_CONNECTIONS)
        if not data or not each:
            return ""

        res = []
        for i in range(0, len(data), each):
            saddr = socket.inet_ntoa(data[i:i+4])
            daddr = socket.inet_ntoa(data[i+4:i+8])
            tos, sol = str(data[i+60]), str(data[i+61])

            s_dev, d_dev, src_qp, dst_qp = "::", "::", -1, -1
            if self.htosi(data[i+76:i+80], True) != -1:
                s_dev = self.to_ipv6(data[i+11:i+27].hex())
                d_dev = self.to_ipv6(data[i+27:i+43].hex())
                src_qp = self.htosi(data[i+76:i+80], True)
                dst_qp = self.htosi(data[i+88:i+92], True)

            res.append(f"{saddr:14}  {daddr:14}  {tos:3}  {sol:2}  {s_dev:31}  {d_dev:32}  {src_qp:9}  {dst_qp:9}")
        return "\n".join(res)

    def get_rds_tcp_sockets(self, sock_fd):
        """Get RDS_INFO_TCP_SOCKETS from socket and parse it."""
        data, each = self.get_rds_info_data(sock_fd, RDS_INFO_TCP_SOCKETS)

        if not data or not each:
            return ""

        res = []
        for i in range(0, len(data), each):
            laddr = socket.inet_ntoa(data[i:i+4])
            lport = str(int(data[i+4:i+6].hex(), 16))
            raddr = socket.inet_ntoa(data[i+6:i+10])
            rport = str(int(data[i+10:i+12].hex(), 16))
            hdr_remain = str(int(data[i+12:i+13].hex(), 16))
            data_remain = str(int(data[i+13:i+14].hex(), 16))
            sent_nxt = str(self.little_endian_to_unsigned(data[i+28:i+32].hex(), 32))
            exp_una = str(self.little_endian_to_unsigned(data[i+32:i+36].hex(), 32))
            seen_una = str(self.little_endian_to_unsigned(data[i+36:i+40].hex(), 32))

            res.append(
                f"{laddr:15} {lport:5} {raddr:15} {rport:5} "
                f"{hdr_remain:9} {data_remain:10} {sent_nxt:10} "
                f"{exp_una:10} {seen_una:10}"
            )
        return "\n".join(res)

    def get_rds_counters(self, sock_fd):
        """Get RDS_INFO_COUNTERS from socket and parse it."""
        data, each = self.get_rds_info_data(sock_fd, RDS_INFO_COUNTERS)
        if not data or not each:
            return ""

        res = []
        for i in range(0, len(data), each):
            name = data[i:i+31].split(b"\x00", maxsplit=1)[0].decode("utf-8")
            val = str(struct.unpack("<Q", data[i+32:i+40])[0])
            res.append(f"{name} {val}")
        return "\n".join(res)

    def get_rds_conns(self, sock_fd):
        """Get RDS_INFO_CONNECTIONS from socket and parse it."""
        data, each = self.get_rds_info_data(sock_fd, RDS_INFO_CONNECTIONS)
        if not data or not each:
            return ""

        res = []
        for i in range(0, len(data), each):
            laddr = socket.inet_ntoa(data[i+16:i+20])
            raddr = socket.inet_ntoa(data[i+20:i+24])
            tos = str(struct.unpack("<b", data[i + 41 : i + 42])[0])
            next_tx = str(self.little_endian_to_unsigned(data[i + 0 : i + 4].hex(), 32))
            next_rx = str(self.little_endian_to_unsigned(data[i + 8 : i + 12].hex(), 32))
            flags = self.decode_flags(int(struct.unpack("<b", data[i + 40 : i + 41])[0]))

            res.append(f"{laddr} {raddr} {tos}  {next_rx} {next_tx} {flags}")

        return "\n".join(res)

    def get_rds_sockets(self, sock_fd):
        """
        Get RDS_INFO_SOCKETS from the socket and parse it into a human-readable format.
        """
        data, each = self.get_rds_info_data(sock_fd, RDS_INFO_SOCKETS)

        if not data or not each:
            return ""

        res = []

        for i in range(0, len(data), each):
            laddr = socket.inet_ntoa(data[i + 4 : i + 8])
            raddr = socket.inet_ntoa(data[i + 8 : i + 12])
            lport = str(int(data[i + 12 : i + 14].hex(), 16))
            rport = str(int(data[i + 14 : i + 16].hex(), 16))

            snd_buf = str(struct.unpack("<I", data[i : i + 4])[0])
            rcv_buf = self.little_endian_to_unsigned(data[i + 16 : i + 20].hex(), 32)
            inode = str(struct.unpack("<Q", data[i + 20 : i + 28])[0])

            # Default values for pid and comm
            pid, comm = "NA", "NA"

            try:
                cong = self.htosi(data[i+32:i+36], True)
                pid = struct.unpack("<I", data[i + 28 : i + 32])[0]
                comm = psutil.Process(pid).name()
            except (psutil.NoSuchProcess, psutil.AccessDenied, struct.error):
                comm, pid, cong = "NA", "NA", -1

            if cong == 0:
                cong = -1
            elif cong == -1:
                cong = 0

            if not comm:
                comm = "NA"
                pid = "NA"
                cong = -1

            res.append(
                f"{laddr:15} {lport:5} {raddr:15} {rport:5} "
                f"{snd_buf:10} {rcv_buf:10} {inode:8} {cong:8} {pid:10} {comm:16}"
            )

        return "\n".join(res)

    def get_rds_queues(self, sock_fd, arg):
        """
        Get RDS_INFO queue messages from the socket and parse it into a human readable format.
        Args:
            sock_fd : socket
            arg ('-r', '-s', '-t') : Type of the queue.
        Return:
            res : String containing send/recv/retrans queue data in a readable format.
        """
        queue = {
            "-s": [RDS_INFO_SEND_MESSAGES, "Send"],
            "-r": [RDS_INFO_RECV_MESSAGES, "Receive"],
            "-t": [RDS_INFO_RETRANS_MESSAGES, "Retrans"],
        }

        data, each = self.get_rds_info_data(sock_fd, queue[arg][0])
        if not data or not each:
            return ""

        res = []

        for i in range(0, len(data), each):
            laddr = str(socket.inet_ntoa(data[i + 12 : i + 16]))
            raddr = str(socket.inet_ntoa(data[i + 16 : i + 20]))
            lport = str(int(data[i + 20 : i + 22].hex(), 16))
            rport = str(int(data[i + 22 : i + 24].hex(), 16))
            tos = str(struct.unpack("<b", data[i + 24 : i + 25])[0])
            seq = str(struct.unpack("<Q", data[i : i + 8])[0])
            byte = str(struct.unpack("<I", data[i + 8 : i + 12])[0])

            res.append(f"{laddr} {lport} {raddr} {rport} {tos} {seq} {byte}")
        return "\n".join(res)

    def get_rds_paths(self, sock_fd):
        """
        Get RDS_INFO_CONN_PATHS from the socket and parse it into a human-readable format.
        """
        data, each = self.get_rds_info_data(sock_fd, RDS_INFO_CONN_PATHS)
        if not data or not each:
            return ""

        res = []

        # Header for the main connection paths
        res.append(f"{'LocalAddr':<15} {'RemoteAddr':<15} {'Tos':<4} {'Trans':<10}")

        for i in range(0, len(data), each):
            saddr = socket.inet_ntoa(data[i + 12 : i + 16])
            daddr = socket.inet_ntoa(data[i + 28 : i + 32])
            tos = str(struct.unpack("<b", data[i + 48 : i + 49])[0])
            trans = data[i + 32 : i + 48].split(b"\x00", maxsplit=1)[0].decode("utf-8")

            res.append(f"{saddr:<15} {daddr:<15} {tos:<4} {trans:<10}")

            path_num = int(struct.unpack("<b", data[i + 49 : i + 50])[0])

            res += "\n"

            # Header for the path details
            res.append(
                f"{'P':<2} {'Connected@':<22} {'Attempt@':<22} {'Reset@':<22} "
                f"{'Attempts':<10} {'RDS':<5} {'Down(Secs)':<13} {'Reason':<15}"
            )

            path_data = data[i + 50 : i + 346]
            for j in range(0, len(path_data), 37):
                attempt_time = self.parse_time(path_data[j + 0 : j + 8])
                conn_time = self.parse_time(path_data[j + 8 : j + 16])
                reset_time = self.parse_time(path_data[j + 16 : j + 24])

                reason = int(struct.unpack("<I", path_data[j + 24 : j + 28])[0])
                reason = (
                    CONN_DROP_REASONS[reason]
                    if reason < len(CONN_DROP_REASONS)
                    else str(reason)
                )

                attempts = str(struct.unpack("<I", path_data[j + 28 : j + 32])[0])
                p_no = str(struct.unpack("<b", path_data[j + 32 : j + 33])[0])

                rds_flags = self.decode_flags(
                    int(struct.unpack("<b", path_data[j + 36 : j + 37])[0])
                )

                down = "---"
                if rds_flags == "--C-" and reset_time != "---":
                    down = self.get_down_time(
                        path_data[j + 8 : j + 16], path_data[j + 0 : j + 8]
                    )

                res.append(
                    f"{p_no:2} {str(conn_time):<21} {str(attempt_time):<22} {str(reset_time):<22} "
                    f"{attempts:10} {rds_flags:5} {down:13} {reason:15}"
                )

                if path_num == 0:
                    break

        return "\n".join(res)

    def main(self, option):
        """
        Wrapper function for all the above RDS-INFO options
        """
        infos = {
            "-I": self.get_rds_ib_conns,
            "-T": self.get_rds_tcp_sockets,
            "-c": self.get_rds_counters,
            "-k": self.get_rds_sockets,
            "-n": self.get_rds_conns,
            '-p': self.get_rds_paths,
            "-r": lambda sock_fd: self.get_rds_queues(sock_fd, '-r'),
            "-s": lambda sock_fd: self.get_rds_queues(sock_fd, '-s'),
            "-t": lambda sock_fd: self.get_rds_queues(sock_fd, '-t')
        }
        infos = {
            "-I": {"method": self.get_rds_ib_conns, "description": "Get RDS InfiniBand connections"},
            "-T": {"method": self.get_rds_tcp_sockets, "description": "Get RDS TCP socket information"},
            "-c": {"method": self.get_rds_counters, "description": "Get RDS counters"},
            "-k": {"method": self.get_rds_sockets, "description": "Get RDS socket information"},
            "-n": {"method": self.get_rds_conns, "description": "Get RDS connection details"},
            "-p": {"method": self.get_rds_paths, "description": "Get RDS connection paths"},
            "-r": {"method": lambda sock_fd: self.get_rds_queues(sock_fd, '-r'),
                   "description": "Get RDS receive queues"},
            "-s": {"method": lambda sock_fd: self.get_rds_queues(sock_fd, '-s'), "description": "Get RDS send queues"},
            "-t": {"method": lambda sock_fd: self.get_rds_queues(sock_fd, '-t'),
                   "description": "Get RDS retransmit queues"},
        }


        # If the user requests help or an invalid option is passed
        if option == "--help" or option not in infos:
            print("Usage: python script.py <option>")
            print("Available options:")
            for key, value in infos.items():
                print(f"  {key}: {value['description']}")
            return ""

        sock_fd = self.create_rds_socket()
        res = infos[option]["method"](sock_fd)
        self.libc.close(sock_fd)
        return res

if __name__ == "__main__":
    # Ensure an argument is passed
    if len(sys.argv) != 2:
        print("Usage: python rds_info.py <option>")
        sys.exit(1)

    cmd_option = sys.argv[1]
    rds_info = RdsInfo()
    result = rds_info.main(cmd_option)
    if result:
        print(result)
