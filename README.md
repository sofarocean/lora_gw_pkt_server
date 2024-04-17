# Overview

This repository contains everything needed to set a Raspberry Pi 4 up to work with a RAK2245 as a network connected LoRa basestation. There are two C programs along with several supporting utilities and configuration files.

After completing all setup steps the Raspberry Pi will start the LoRa packet server program at boot and will re-launch it if it crashes for any reason.

For more information on how to connect to the LoRa packet server once it is running, see the Littoral LoRa Spotter User Guide in Notion.

# Setup Instructions

0. Set the raspberry pi up with a plain install of "Raspberry Pi OS Lite 32-bit"
    - Follow instructions from [raspberrypi.com](https://www.raspberrypi.com/) to install the OS.
    - NOTE: You must use the 32 bit version of the OS!
    - NOTE: You must use the lite version of the OS to avoid latency spikes or other unpredictable behavior.

1. Enable SPI on the Raspberry Pi using the `sudo raspi-config` command.
   - Select "Interface Options" from the main raspi-config menu.
   - Select "SPI" from the sub menu.
   - Respond "Yes" to the prompt. 

2. Install git and clone this git repo to the target raspberry pi.
    - `sudo apt install git`
    - `git clone https://github.com/mbella-sofar/lora_gw_pkt_server.git ~/lora_gw_pkt_server`

3. Build the util_pkt_server binary
    - `cd ~/lora_gw_pkt_server/util_pkt_server`
    - `make`

4. Make a directory for all the files needed for the install.
    - `sudo mkdir -p /opt/lora_basestation/`

5. Copy rak2245_setup.sh to /opt/lora_basestation/
    - `sudo cp ~/lora_gw_pkt_server/rak2245_setup.sh /opt/lora_basestation/`

6. Copy the lora packet server binary to /opt/lora_basestation/
    - `sudo cp ~/lora_gw_pkt_server/util_pkt_server/util_pkt_server /opt/lora_basestation/`

7. Copy rak2245.service to /etc/systemd/system/
    - `sudo cp ~/lora_gw_pkt_server/rak2245.service /etc/systemd/system/`

8. Make sure permissions are set correctly for all files
    - `sudo chmod -R 755 /opt/lora_basestation`
    - `sudo chown -R root:root /opt/lora_basestation`
    - `sudo chmod 644 /etc/systemd/system/rak2245.service`
    - `sudo chown root:root /etc/systemd/system/rak2245.service`

9. Tell systemd to scan for the newly added service file.
    - `sudo systemctl daemon-reload`

10. Enable the service so it starts on boot fron now on.
    - `sudo systemctl enable rak2245`

11. Start the service now so we can see things working without needing a restart.
    - `sudo systemctl start rak2245`

## NOTES
   - If you modify the rak2245.service file at any point you need to run `sudo systemctl daemon-reload` command afterwards.
