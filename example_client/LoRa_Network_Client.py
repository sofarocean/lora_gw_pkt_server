#!/usr/bin/env python3

import socket
import sys
import os

if __name__ == "__main__":
    # Check that one command line argument was passed
    if len(sys.argv) != 2:
        # Get the name of this file
        sname = os.path.basename(__file__)
        
        # Print usage message
        print(f"Usage: ./{sname} server_ip_addr")
        sys.exit(1)

    # User provided ip address
    ipaddr = sys.argv[1]

    # Open a socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as skt:
        # Connect to port 2600 of the user specified ip address 
        skt.connect((ipaddr, 2600))
        # Set a timeout for the socket reads
        skt.settimeout(0.1)

        # Buffer for incoming messages
        buff = ""
        while(True):
            # Handle timeouts
            try:
                # Read bytes from socket into buffer
                buff += skt.recv(4).decode()
            except socket.timeout as e:
                # Ignore timed out socket reads
                pass

            # If the buffer contains a newline char then split the newest message from the rest.
            if "\n" in buff:
                pkt,sep,buff = buff.partition("\n")
                print(pkt)
            else:
                # Go to the start of the loop ASAP if we haven't received a full packet yet.
                continue
