from lora_packet_analysis_tools import *

receiver_log_file = "/PATH_TO/2024-04-12_LoRa_test_data/All6SpotterTest.log"
'''
Output of the example_client.
log lines look like: #2,2,17126961014,-695,743,-16
The first field '#N' denotes the Spotter channel.
The third field is the epoch time in 1/10ths of a second - so 17126961014 is actually 1712696101.4 epoch time.
'''

spotter_configs = [
    SpotterConfigSet(channel_number=0, flt_data_file='/PATH_TO/2024-04-12_LoRa_test_data/SPOT-31193C_ch0_0025_FLT.csv'),
    SpotterConfigSet(channel_number=1, flt_data_file='/PATH_TO/2024-04-12_LoRa_test_data/SPOT-31099C_ch1_0038_FLT.csv'),
    SpotterConfigSet(channel_number=2, flt_data_file='/PATH_TO/2024-04-12_LoRa_test_data/SPOT-31106C_ch2_0033_FLT.csv'),
    SpotterConfigSet(channel_number=3, flt_data_file='/PATH_TO/2024-04-12_LoRa_test_data/SPOT-31026C_ch3_0025_FLT.csv'),
    SpotterConfigSet(channel_number=4, flt_data_file='/PATH_TO/2024-04-12_LoRa_test_data/SPOT-31160C_ch4_0043_FLT.csv'),
    SpotterConfigSet(channel_number=5, flt_data_file='/PATH_TO/2024-04-12_LoRa_test_data/SPOT-31188C_ch5_0024_FLT.csv'),
]

receiver_df = parse_receiver_log(receiver_log_file)

spotter_dataframes = load_spotter_data(spotter_configs)

merged_dfs = determine_packet_loss(spotter_dataframes, receiver_df)

for spotter in merged_dfs:
    channel_number = spotter.iloc[0]['channel_number']
    print(f"Saving outputs for channel {channel_number}")
    # Filter for packets that were not delivered
    lost_packets = spotter[(spotter['packet_delivered'] == False)]
    # Filter for packets that were delivered but are marked as corrupted
    corrupted_packets = spotter[((spotter['packet_data_correct'] == False) & (spotter['packet_delivered'] == True))]

    # Save the full spotter DataFrame
    spotter.to_csv(f"channel_{channel_number}_packets_all.csv", index=False)
    # Save the DataFrame of lost packets
    lost_packets.to_csv(f"channel_{channel_number}_packets_lost.csv", index=False)
    # Save the DataFrame of corrupted packets
    corrupted_packets.to_csv(f"channel_{channel_number}_packets_corrupted.csv", index=False)

    print(f"Saved channel {channel_number} all packets to channel_{channel_number}_packets_all.csv")
    print(f"Saved channel {channel_number} lost packets to channel_{channel_number}_packets_lost.csv")
    print(f"Saved channel {channel_number} corrupted packets to channel_{channel_number}_packets_corrupted.csv")

    plot_packet_performance(spotter, channel_number)
