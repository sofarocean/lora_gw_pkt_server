/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
    Configure LoRa concentrator and record received packets in a log file

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
    #define _XOPEN_SOURCE 600
#else
    #define _XOPEN_SOURCE 500
#endif

#include <stdint.h>     /* C99 types */
#include <stdbool.h>    /* bool type */
#include <stdio.h>      /* printf fprintf sprintf fopen fputs */

#include <string.h>     /* memset */
#include <signal.h>     /* sigaction */
#include <time.h>       /* time clock_gettime strftime gmtime clock_nanosleep*/
#include <unistd.h>     /* getopt access */
#include <stdlib.h>     /* atoi */

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "parson.h"
#include "loragw_hal.h"
#include "errno.h"      /* network socket error handling */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))
#define MSG(args...)    fprintf(stderr,"loragw_pkt_logger: " args) /* message that is destined to the user */

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

/* signal handling variables */
struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
static int exit_sig = 0; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
static int quit_sig = 0; /* 1 -> application terminates without shutting down the hardware */

/* configuration variables needed by the application  */
uint64_t lgwm = 0; /* LoRa gateway MAC address */
char lgwm_str[17];

/* clock and log file management */
time_t now_time;
time_t log_start_time;
char log_file_name[64];

int32_t radio_freqs[2];
int32_t chan_if_hz[2][4];

/* -------------------------------------------------------------------------- */
/* --- Custom Constants ----------------------------------------------------- */
#define INT32MAX 0x7FFFFFFF
#define RXBUFLEN 1024

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

static void sig_handler(int sigio);

int parse_SX1301_configuration(const char * conf_file);

int cmpfunc (const void * a, const void * b);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

// Function to pass to qsort to get a list of ints sorted.
int cmpfunc (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

static void sig_handler(int sigio) {
    if (sigio == SIGQUIT) {
        quit_sig = 1;;
    } else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
        exit_sig = 1;
    }
}

int parse_SX1301_configuration(const char * conf_file) {
    int i;
    const char conf_obj[] = "SX1301_conf";
    char param_name[32]; /* used to generate variable parameter names */
    const char *str; /* used to store string value from JSON object */
    struct lgw_conf_board_s boardconf;
    struct lgw_conf_rxrf_s rfconf;
    struct lgw_conf_rxif_s ifconf;
    JSON_Value *root_val;
    JSON_Object *root = NULL;
    JSON_Object *conf = NULL;
    JSON_Value *val;
    uint32_t sf, bw;

    /* Fill the channel if array with a placeholder value to later determine if the json section was absent */
    for (int i=0; i < 2; i++) {
        for (int j=0; j < 4; j++) {
            chan_if_hz[i][j] = INT32MAX;
        }
    }
    /* try to parse JSON */
    root_val = json_parse_file_with_comments(conf_file);
    root = json_value_get_object(root_val);
    if (root == NULL) {
        MSG("ERROR: %s id not a valid JSON file\n", conf_file);
        exit(EXIT_FAILURE);
    }
    conf = json_object_get_object(root, conf_obj);
    if (conf == NULL) {
        MSG("INFO: %s does not contain a JSON object named %s\n", conf_file, conf_obj);
        return -1;
    } else {
        MSG("INFO: %s does contain a JSON object named %s, parsing SX1301 parameters\n", conf_file, conf_obj);
    }

    /* set board configuration */
    memset(&boardconf, 0, sizeof boardconf); /* initialize configuration structure */
    val = json_object_get_value(conf, "lorawan_public"); /* fetch value (if possible) */
    if (json_value_get_type(val) == JSONBoolean) {
        boardconf.lorawan_public = (bool)json_value_get_boolean(val);
    } else {
        MSG("WARNING: Data type for lorawan_public seems wrong, please check\n");
        boardconf.lorawan_public = false;
    }
    val = json_object_get_value(conf, "clksrc"); /* fetch value (if possible) */
    if (json_value_get_type(val) == JSONNumber) {
        boardconf.clksrc = (uint8_t)json_value_get_number(val);
    } else {
        MSG("WARNING: Data type for clksrc seems wrong, please check\n");
        boardconf.clksrc = 0;
    }
    MSG("INFO: lorawan_public %d, clksrc %d\n", boardconf.lorawan_public, boardconf.clksrc);
    /* all parameters parsed, submitting configuration to the HAL */
    if (lgw_board_setconf(boardconf) != LGW_HAL_SUCCESS) {
        MSG("ERROR: Failed to configure board\n");
        return -1;
    }

    /* set configuration for RF chains */
    for (i = 0; i < LGW_RF_CHAIN_NB; ++i) {
        memset(&rfconf, 0, sizeof(rfconf)); /* initialize configuration structure */
        sprintf(param_name, "radio_%i", i); /* compose parameter path inside JSON structure */
        val = json_object_get_value(conf, param_name); /* fetch value (if possible) */
        if (json_value_get_type(val) != JSONObject) {
            MSG("INFO: no configuration for radio %i\n", i);
            continue;
        }
        /* there is an object to configure that radio, let's parse it */
        sprintf(param_name, "radio_%i.enable", i);
        val = json_object_dotget_value(conf, param_name);
        if (json_value_get_type(val) == JSONBoolean) {
            rfconf.enable = (bool)json_value_get_boolean(val);
        } else {
            rfconf.enable = false;
        }
        if (rfconf.enable == false) { /* radio disabled, nothing else to parse */
            MSG("INFO: radio %i disabled\n", i);
        } else  { /* radio enabled, will parse the other parameters */
            snprintf(param_name, sizeof param_name, "radio_%i.freq", i);
            rfconf.freq_hz = (uint32_t)json_object_dotget_number(conf, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.rssi_offset", i);
            rfconf.rssi_offset = (float)json_object_dotget_number(conf, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.type", i);
            str = json_object_dotget_string(conf, param_name);
            if (!strncmp(str, "SX1255", 6)) {
                rfconf.type = LGW_RADIO_TYPE_SX1255;
            } else if (!strncmp(str, "SX1257", 6)) {
                rfconf.type = LGW_RADIO_TYPE_SX1257;
            } else {
                MSG("WARNING: invalid radio type: %s (should be SX1255 or SX1257)\n", str);
            }
            snprintf(param_name, sizeof param_name, "radio_%i.tx_enable", i);
            val = json_object_dotget_value(conf, param_name);
            if (json_value_get_type(val) == JSONBoolean) {
                rfconf.tx_enable = (bool)json_value_get_boolean(val);
                if (rfconf.tx_enable == true) {
                    /* tx notch filter frequency to be set */
                    snprintf(param_name, sizeof param_name, "radio_%i.tx_notch_freq", i);
                    rfconf.tx_notch_freq = (uint32_t)json_object_dotget_number(conf, param_name);
                }
            } else {
                rfconf.tx_enable = false;
            }
            radio_freqs[i] = rfconf.freq_hz; // Save the radio center frequencies to global for later use converting packet freq back to spotter number.
            MSG("INFO: radio %i enabled (type %s), center frequency %u, RSSI offset %f, tx enabled %d, tx_notch_freq %u\n", i, str, rfconf.freq_hz, rfconf.rssi_offset, rfconf.tx_enable, rfconf.tx_notch_freq);
        }
        /* all parameters parsed, submitting configuration to the HAL */
        if (lgw_rxrf_setconf(i, rfconf) != LGW_HAL_SUCCESS) {
            MSG("ERROR: invalid configuration for radio %i\n", i);
            return -1;
        }
    }
    
    uint8_t r0ch = 0;
    uint8_t r1ch = 0;
    /* set configuration for LoRa multi-SF channels (bandwidth cannot be set) */
    for (i = 0; i < LGW_MULTI_NB; ++i) {
        memset(&ifconf, 0, sizeof(ifconf)); /* initialize configuration structure */
        sprintf(param_name, "chan_multiSF_%i", i); /* compose parameter path inside JSON structure */
        val = json_object_get_value(conf, param_name); /* fetch value (if possible) */
        if (json_value_get_type(val) != JSONObject) {
            MSG("INFO: no configuration for LoRa multi-SF channel %i\n", i);
            continue;
        }
        /* there is an object to configure that LoRa multi-SF channel, let's parse it */
        sprintf(param_name, "chan_multiSF_%i.enable", i);
        val = json_object_dotget_value(conf, param_name);
        if (json_value_get_type(val) == JSONBoolean) {
            ifconf.enable = (bool)json_value_get_boolean(val);
        } else {
            ifconf.enable = false;
        }
        if (ifconf.enable == false) { /* LoRa multi-SF channel disabled, nothing else to parse */
            MSG("INFO: LoRa multi-SF channel %i disabled\n", i);
        } else  { /* LoRa multi-SF channel enabled, will parse the other parameters */
            sprintf(param_name, "chan_multiSF_%i.radio", i);
            ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf, param_name);
            sprintf(param_name, "chan_multiSF_%i.if", i);
            ifconf.freq_hz = (int32_t)json_object_dotget_number(conf, param_name);
            // TODO: handle individual SF enabling and disabling (spread_factor)
            // Save the intermediate frequencies to global for later use converting packet freq back to spotter number.
            if (ifconf.rf_chain == 0) {
                chan_if_hz[0][r0ch++] = ifconf.freq_hz;
            } else if (ifconf.rf_chain == 1) {
                chan_if_hz[1][r1ch++] = ifconf.freq_hz;
            }
            MSG("INFO: LoRa multi-SF channel %i enabled, radio %i selected, IF %i Hz, 125 kHz bandwidth, SF 7 to 12\n", i, ifconf.rf_chain, ifconf.freq_hz);
        }
        /* all parameters parsed, submitting configuration to the HAL */
        if (lgw_rxif_setconf(i, ifconf) != LGW_HAL_SUCCESS) {
            MSG("ERROR: invalid configuration for Lora multi-SF channel %i\n", i);
            return -1;
        }
    }

    /* set configuration for LoRa standard channel */
    memset(&ifconf, 0, sizeof(ifconf)); /* initialize configuration structure */
    val = json_object_get_value(conf, "chan_Lora_std"); /* fetch value (if possible) */
    if (json_value_get_type(val) != JSONObject) {
        MSG("INFO: no configuration for LoRa standard channel\n");
    } else {
        val = json_object_dotget_value(conf, "chan_Lora_std.enable");
        if (json_value_get_type(val) == JSONBoolean) {
            ifconf.enable = (bool)json_value_get_boolean(val);
        } else {
            ifconf.enable = false;
        }
        if (ifconf.enable == false) {
            MSG("INFO: LoRa standard channel %i disabled\n", i);
        } else  {
            ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf, "chan_Lora_std.radio");
            ifconf.freq_hz = (int32_t)json_object_dotget_number(conf, "chan_Lora_std.if");
            bw = (uint32_t)json_object_dotget_number(conf, "chan_Lora_std.bandwidth");
            switch(bw) {
                case 500000: ifconf.bandwidth = BW_500KHZ; break;
                case 250000: ifconf.bandwidth = BW_250KHZ; break;
                case 125000: ifconf.bandwidth = BW_125KHZ; break;
                default: ifconf.bandwidth = BW_UNDEFINED;
            }
            sf = (uint32_t)json_object_dotget_number(conf, "chan_Lora_std.spread_factor");
            switch(sf) {
                case  7: ifconf.datarate = DR_LORA_SF7;  break;
                case  8: ifconf.datarate = DR_LORA_SF8;  break;
                case  9: ifconf.datarate = DR_LORA_SF9;  break;
                case 10: ifconf.datarate = DR_LORA_SF10; break;
                case 11: ifconf.datarate = DR_LORA_SF11; break;
                case 12: ifconf.datarate = DR_LORA_SF12; break;
                default: ifconf.datarate = DR_UNDEFINED;
            }
            MSG("INFO: LoRa standard channel enabled, radio %i selected, IF %i Hz, %u Hz bandwidth, SF %u\n", ifconf.rf_chain, ifconf.freq_hz, bw, sf);
        }
        if (lgw_rxif_setconf(8, ifconf) != LGW_HAL_SUCCESS) {
            MSG("ERROR: invalid configuration for Lora standard channel\n");
            return -1;
        }
    }

    /* set configuration for FSK channel */
    memset(&ifconf, 0, sizeof(ifconf)); /* initialize configuration structure */
    val = json_object_get_value(conf, "chan_FSK"); /* fetch value (if possible) */
    if (json_value_get_type(val) != JSONObject) {
        MSG("INFO: no configuration for FSK channel\n");
    } else {
        val = json_object_dotget_value(conf, "chan_FSK.enable");
        if (json_value_get_type(val) == JSONBoolean) {
            ifconf.enable = (bool)json_value_get_boolean(val);
        } else {
            ifconf.enable = false;
        }
        if (ifconf.enable == false) {
            MSG("INFO: FSK channel %i disabled\n", i);
        } else  {
            ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf, "chan_FSK.radio");
            ifconf.freq_hz = (int32_t)json_object_dotget_number(conf, "chan_FSK.if");
            bw = (uint32_t)json_object_dotget_number(conf, "chan_FSK.bandwidth");
            if      (bw <= 7800)   ifconf.bandwidth = BW_7K8HZ;
            else if (bw <= 15600)  ifconf.bandwidth = BW_15K6HZ;
            else if (bw <= 31200)  ifconf.bandwidth = BW_31K2HZ;
            else if (bw <= 62500)  ifconf.bandwidth = BW_62K5HZ;
            else if (bw <= 125000) ifconf.bandwidth = BW_125KHZ;
            else if (bw <= 250000) ifconf.bandwidth = BW_250KHZ;
            else if (bw <= 500000) ifconf.bandwidth = BW_500KHZ;
            else ifconf.bandwidth = BW_UNDEFINED;
            ifconf.datarate = (uint32_t)json_object_dotget_number(conf, "chan_FSK.datarate");
            MSG("INFO: FSK channel enabled, radio %i selected, IF %i Hz, %u Hz bandwidth, %u bps datarate\n", ifconf.rf_chain, ifconf.freq_hz, bw, ifconf.datarate);
        }
        if (lgw_rxif_setconf(9, ifconf) != LGW_HAL_SUCCESS) {
            MSG("ERROR: invalid configuration for FSK channel\n");
            return -1;
        }
    }
    json_value_free(root_val);
    return 0;
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

int main()
{
    int i; /* loop and temporary variables */

    /* configuration file related */
    const char global_conf_fname[] = "global_conf.json"; /* contain global (typ. network-wide) configuration */
    const char local_conf_fname[] = "local_conf.json"; /* contain node specific configuration, overwrite global parameters for parameters that are defined in both */

    /* allocate memory for packet fetching and processing */
    struct lgw_pkt_rx_s rxpkt[16]; /* array containing up to 16 inbound packets metadata */
    struct lgw_pkt_rx_s *p; /* pointer on a RX packet */
    int nb_pkt;

    /* buffer for each message to be sent to the client */
    char tx_msg[100];
    /* Struct to hold data from the spotters */
    union spotterdata_u {
        uint8_t bytes[17];
        struct  {
            unsigned long int timestamp __attribute__((__packed__));
            unsigned char timestamp_f __attribute__((__packed__));
            long int X __attribute__((__packed__));
            long int Y __attribute__((__packed__));
            long int Z __attribute__((__packed__));
        } d;
    } spotterdata;

    /* Network receive buffer */
    char rx_msg[RXBUFLEN];

    /* Keep track of our connection status */
    int connected = 0;

    /* configure signal handling */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);

    /* configuration files management */
    if (access(global_conf_fname, R_OK) == 0) {
    /* if there is a global conf, parse it */
        MSG("INFO: found global configuration file %s, trying to parse it\n", global_conf_fname);
        parse_SX1301_configuration(global_conf_fname);
    } else if (access(local_conf_fname, R_OK) == 0) {
    /* if there is only a local conf, parse it and that's all */
        MSG("INFO: found local configuration file %s, trying to parse it\n", local_conf_fname);
        parse_SX1301_configuration(local_conf_fname);
    } else {
        MSG("ERROR: failed to find any configuration file named %s, or %s\n", global_conf_fname, local_conf_fname);
        return EXIT_FAILURE;
    }

    /* starting the concentrator */
    i = lgw_start();
    if (i == LGW_HAL_SUCCESS) {
        MSG("INFO: concentrator started, packet can now be received\n");
    } else {
        MSG("ERROR: failed to start the concentrator\n");
        return EXIT_FAILURE; //TODO: Uncomment this line before using this outside GDB!!!
        //#warning Uncomment the above line before actually building this!!
    }

    /* transform the MAC address into a string */
    sprintf(lgwm_str, "%08X%08X", (uint32_t)(lgwm >> 32), (uint32_t)(lgwm & 0xFFFFFFFF));

    /* Set things up to serve data over the network */
    int serversock, clientsock;
    struct sockaddr_in loraserver, loraclient;

    if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        MSG("ERROR: failed to create network socket, exiting\n");
        return EXIT_FAILURE;
    }
    memset(&loraserver, 0, sizeof(loraserver));
    loraserver.sin_family = AF_INET;
    loraserver.sin_addr.s_addr = htonl(INADDR_ANY);
    loraserver.sin_port = htons(2600);

    if (bind(serversock, (struct sockaddr *) &loraserver, sizeof(loraserver)) < 0) {
        MSG("ERROR: failed to bind network socket, exiting\n");
        return EXIT_FAILURE;
    }
    
    // Only allow one connection at a time (second arg = 0)
    if (listen(serversock, 0) < 0) {
        MSG("ERROR: failed to listen on network socket, exiting\n");
        return EXIT_FAILURE;
    }

    /* Calculate the packet rx frequencies we expect */
    int32_t chanlist[8];
    const uint8_t radio_channels = 4;
    memset(chanlist, 0, sizeof(chanlist));
    for (unsigned int k=0; k < ARRAY_SIZE(chanlist); k++) {
        uint8_t radion = (k / radio_channels);
        uint8_t rchannel = (k % radio_channels);
        int32_t cfreq = radio_freqs[radion];
        int32_t ifreq = chan_if_hz[radion][rchannel];
        if (cfreq == 0) {
            MSG("ERROR: Radio %d frequency not set!", radion);
            return EXIT_FAILURE;
        }
        if (ifreq == INT32MAX) {
            MSG("INFO: Radio %d has a missing chan_multiSF_ section in the json file.", radion);
            chanlist[k] = INT32MAX;
        } else {
            chanlist[k] = cfreq + ifreq;
            MSG("INFO: Spotter %d: LoRa receiver set to %dHz.\n", k, chanlist[k]);
        }
    }

    // Sort our channel list
    qsort(chanlist, ARRAY_SIZE(chanlist), sizeof(int32_t), cmpfunc);

    /* main loop */
    /* While a client is connected keep forwarding packets from the RAK module to the client. */
    while ((quit_sig != 1) && (exit_sig != 1)) {
        if (connected == 0) {
            // Wait for a new client to connect.
            MSG("INFO: Waiting for a client to connect.\n");
            unsigned int clientlen = sizeof(loraclient);
            /* Wait for client connection */
            if ((clientsock = accept(serversock, (struct sockaddr *) &loraclient, &clientlen)) < 0) {
                MSG("ERROR: failed to accept client connection, exiting\n");
                return EXIT_FAILURE;
            }
            /* A client is now connected */
            MSG("INFO: Client connected: %s\n", inet_ntoa(loraclient.sin_addr));

            char hellomsg[] = "Connected...\n";
            send(clientsock, hellomsg, strlen(hellomsg), 0);
            connected = 1;

            /* Clear the buffer out right after we connect so we don't send old packets to the client */
            nb_pkt = lgw_receive(ARRAY_SIZE(rxpkt), rxpkt);
            if (nb_pkt == LGW_HAL_ERROR) {
                MSG("ERROR: failed packet fetch, exiting\n");
                return EXIT_FAILURE;
            }
        }
        /* fetch packets */
        nb_pkt = lgw_receive(ARRAY_SIZE(rxpkt), rxpkt);
        if (nb_pkt == LGW_HAL_ERROR) {
            MSG("ERROR: failed packet fetch, exiting\n");
            return EXIT_FAILURE;
        }

        // Check if the client is still connected.
        int received = -1;
        if ((received = recv(clientsock, rx_msg, RXBUFLEN, MSG_DONTWAIT)) < 0) {
            if (errno == EBADF) {
                connected = 0;
                continue; // Break out of the packet processing loop to reconnect to a client.
            } else if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                // No data was in the buffer so keep working on other stuff.
            } else {
                MSG("ERROR: recv reported error code: %s\n", strerror(errno));
                return EXIT_FAILURE;
            }
        } else if (received == 0) {
            MSG("INFO: Client disconnected.\n");
            // The client disconnected!
            close(clientsock); // Don't forget to close the socket when the remote end disconnects!
        }

        /* process packets */
        for (i=0; i < nb_pkt; ++i) {
            p = &rxpkt[i];

            // Only process and forward packets that meet our criteria.
            if ((p->status == STAT_CRC_OK) && (p->modulation == MOD_LORA) &&
               (p->bandwidth == BW_125KHZ) && (p->datarate == DR_LORA_SF7) &&
               (p->coderate == CR_LORA_4_5)) {
                // Packet meets all our criteria. Time to forward it to the network.
                int spotn = -1;
                for (unsigned int l=0; l < ARRAY_SIZE(chanlist); l++) {
                    if ((int64_t)chanlist[l] == (int64_t)p->freq_hz) spotn = l;
                }
                if (spotn == -1) {
                    MSG("INFO: Somehow received packet on unknown frequency (%d Hz)!?\n", p->freq_hz);
                    continue; // Skip the rest of the steps to process this packet.
                }
                
                // Load the received data into a union/struct to parse.
                memcpy(spotterdata.bytes, p->payload, p->size);
                
                // Combine the timestamp in seconds with the fractional part (tenths of a second)
                unsigned long long int extended_timestamp = (unsigned long long int)spotterdata.d.timestamp * 10;
                //unsigned long long int extended_timestamp = (unsigned long long int)spotterdata.d.timestamp;
                extended_timestamp += (unsigned long long int)spotterdata.d.timestamp_f;
                
                // Zero out our message buffer
                memset(tx_msg, 0, ARRAY_SIZE(tx_msg));
                //sprintf(tx_msg, "#,%d,%lu,%ld,%ld,%ld\n", spotn, spotterdata.d.timestamp, spotterdata.d.X, spotterdata.d.Y, spotterdata.d.Z);
                sprintf(tx_msg, "#%d,%llu,%ld,%ld,%ld\n", spotn, extended_timestamp, spotterdata.d.X, spotterdata.d.Y, spotterdata.d.Z);
                send(clientsock, tx_msg, strlen(tx_msg), 0);
            }
        }
    }

    if (exit_sig == 1) {
        /* clean up before leaving */
        i = lgw_stop();
        if (i == LGW_HAL_SUCCESS) {
            MSG("INFO: concentrator stopped successfully\n");
        } else {
            MSG("WARNING: failed to stop concentrator successfully\n");
        }

        /* Make sure the socket is closed */
        close(clientsock); // This is probably the wrong way to do this!!!
    }

    MSG("INFO: Exiting packet server program\n");
    return EXIT_SUCCESS;
}

/* --- EOF ------------------------------------------------------------------ */
