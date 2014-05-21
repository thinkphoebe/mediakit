#!/usr/bin/env python
# coding=utf8

import socket
import struct
import sys


def udpdump(ip, port, filename):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', int(port)))
    fp = open(filename, 'wb')

    ip_number = struct.unpack('!I', socket.inet_aton(ip))[0]
    print ip_number
    print ip_number & 0xFF000000
    if 0xE0000000 <= (ip_number & 0xFF000000) and ip_number & 0xFF000000 < 0xF0000000:
        print("multicast address\n")
        mreq = struct.pack('4sL', socket.inet_aton(ip), socket.INADDR_ANY)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    while True:
        data, address = sock.recvfrom(65536)
#         print len(data), address
        fp.write(data)


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print 'usage: ./udpdump.py 192.168.1.126 6000 write_file.dump'
        exit(-1)
    udpdump(sys.argv[1], sys.argv[2], sys.argv[3])
