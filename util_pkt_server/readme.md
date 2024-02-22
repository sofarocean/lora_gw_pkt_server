	 / _____)             _              | |    
	( (____  _____ ____ _| |_ _____  ____| |__  
	 \____ \| ___ |    (_   _) ___ |/ ___)  _ \ 
	 _____) ) ____| | | || |_| ____( (___| | | |
	(______/|_____)_|_|_| \__)_____)\____)_| |_|
	  (C)2013 Semtech-Cycleo

LoRa packet server
===================

1. Introduction
----------------

This program is used to set up a LoRa concentrator based on the settings in
the provided JSON file. After configuring the concentrator this program polls
the SX1301 in the concentrator for new packets. As packets are received by the
concentrator they are read by the program, filtered, decoded, and then
formatted as ASCII before being sent to an attached network client.

2. Dependencies
----------------

This program uses the Parson library (http://kgabis.github.com/parson/) by
Krzysztof Gabis for JSON parsing.
Many thanks to him for that very practical and well written library.

Only high-level functions are used (the ones contained in loragw_hal) so there
is no hardware dependencies assuming the HAL is matched with the proper version
of the hardware.
Data structures of the received packets are accessed by name (ie. not at a
binary level) so new functionalities can be added to the API without affecting
that program at all.

It was tested with v1.3.0 of the libloragw library, and should be compatible
with any later version of the library assuming the API is downward-compatible.

3. Usage
---------

This application comes with the files needed to add a systemd service.
In normal operation this application will be started/stopped/restarted by
systemd.

During normal operation this application will be started once the RPI boots up.
If this application crashes systemd will restart it.

To disable this application (stop it from launching at boot):\
`sudo systemctl disable rak2245.service`

To enable this application (make it launch at boot):\
`sudo systemctl enable rak2245.service`

To stop the application while it is running:\
`sudo systemctl stop rak2245.service`

To start the application if it is not running:\
`sudo systemctl start rak2245.service`

When debugging the application it is easiest to disable the systemd service so
that you can manually launch the application binary after compiling.

To manually launch the application you need to provide the path to a json 
configuration file.\
`./util_pkt_server /path/to/your/json.file`

To stop the application, press Ctrl+C.

On power up the RAK2245 concentrator may require a reset before being ready to
use. The script `rak2245_setup.sh` will run all the required steps to reset the
concentrator. When starting the systemd service the reset script is run before 
trying to (re)start the application.

4. License
-----------

Copyright (c) 2013, SEMTECH S.A.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of the Semtech corporation nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL SEMTECH S.A. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

5. License for Parson library
------------------------------

Parson ( http://kgabis.github.com/parson/ )
Copyright (c) 2012 Krzysztof Gabis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*EOF*
