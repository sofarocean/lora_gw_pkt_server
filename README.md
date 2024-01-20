# Setup instructions

0. Set the raspberry pi up with a plain install of "Raspberry Pi OS Lite 32-bit"
  - Follow instructions from raspberrypi.com to complete the OS install.
  - NOTE: You must use the 32 bit version of the OS!
  - NOTE: You must use the lite version of the OS to avoid latency spikes or other unpredictable behavior.

1. Clone this git repo to the target raspberry pi.
  - `git clone https://github.com/mbella-sofar/lora_gw_pkt_server.git ~/lora_gw_pkt_server`

2. Build the util_pkt_server binary
  - `cd ~/lora_gw_pkt_server/util_pkt_server && make`

3. Make a directory for all the files needed for the install.
  - `sudo mkdir -p /opt/lora_basestation/`

4. Copy rak2245_setup.sh to /opt/lora_basestation/
  - `cp ~/lora_gw_pkt_server/rak2245_setup.sh /opt/lora_basestation/`

5. Copy the lora packet server binary to /opt/lora_basestation/
  - `cp ~/lora_gw_pkt_server/util_pkt_server/util_pkt_server /opt/lora_basestation/`

6. Copy rak2245.service to /etc/systemd/system/
  - `cp ~/lora_gw_pkt_server/rak2245.service /etc/systemd/system/`

7. Tell systemd to scan for the newly added service file.
  - `sudo systemctl daemon-reload`

7. Enable the service so it starts on boot fron now on.
  - `sudo systemctl enable rak2245`

8. Start the service now so we can see things working without needing a restart.
  - `sudo systemctl start rak2245`

9. Make sure permissions are set correctly for all files
  - `sudo chmod -R 755 /opt/lora_basestation`
  - `sudo chown -R root:root /opt/lora_basestation`
  - `sudo chmod 644 /etc/systemd/system/rak2245.service`
  - `sudo chown root:root /etc/systemd/system/rak2245.service`


NOTES
 - If you modify the rak2245.service file at any point you need to run `sudo systemctl daemon-reload` command afterwards.
 - 
