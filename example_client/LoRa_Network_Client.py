#!/usr/bin/env python3

import socket
import sys
import os

if __name__ == "__main__":
    sname = os.path.basename(__file__)
    if len(sys.argv) != 2:
        print(f"Usage: ./{sname} server_ip_addr")
        sys.exit(1)

    ipaddr = sys.argv[1]
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as skt:
        skt.connect((ipaddr, 2600))
        skt.settimeout(0.01)

        buff = ""
        while(True):
            try:
                rx = skt.recv(1).decode()
                buff += rx
            except socket.timeout as e:
                pass

            if "\n" in buff:
                pkt,sep,buff = buff.partition("\n")
                print(pkt)
            else:
                continue
