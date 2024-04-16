# TODO - implement support for FDB files and other signal_stages
# TODO - add support for multiple log session files?

import matplotlib.pyplot as plt
import pandas as pd
from tqdm import tqdm
from typing import NamedTuple, List


class SpotterConfigSet(NamedTuple):
    channel_number: int
    flt_data_file: str
    '''
    FLT.csv file containing the data
    flt_data_file is a csv with headers: "millis, GPS_Epoch_Time(s), outx(mm), outy(mm), outz(mm)", and an extra unlabeled "flag" column
    '''


def parse_receiver_log(receiver_log_path: str) -> pd.DataFrame:
    # Parse the receiver log file
    print("Parsing receiver data...")
    log_data = []
    with open(receiver_log_path, 'r') as file:
        total_lines = sum(1 for line in file if line.startswith('#'))  # Count the lines that will be processed
        file.seek(0)  # Go back to the start of the file

        for line in tqdm(file, total=total_lines, desc="Parsing receiver log"):
            if line.startswith('#'):
                parts = line.strip().split(',')
                log_data.append({
                    'channel_number': int(parts[0][1:]),
                    'signal_stage': int(parts[1]),
                    'epoch_time': float(parts[2]) / 10,
                    'outx': int(parts[3]),
                    'outy': int(parts[4]),
                    'outz': int(parts[5])
                })
    print(f"\tParsed {len(log_data)} packets")
    return pd.DataFrame(log_data)


def load_spotter_data(spotter_configs: List[SpotterConfigSet]) -> List[pd.DataFrame]:
    print(f"Parsing Spotter data from {len(spotter_configs)} Spotters...")
    spotter_data = []
    for config in spotter_configs:
        # When reading the Spotter data, specify the column names explicitly if they are not being parsed correctly
        # flag: "I" - filter is chilling, "V" - interpolated sample
        spotter_df = pd.read_csv(config.flt_data_file, sep=',', names=['millis', 'GPS_Epoch_Time(s)', 'outx(mm)', 'outy(mm)', 'outz(mm)', 'flag'], header=0)
        spotter_df['channel_number'] = config.channel_number
        spotter_data.append(spotter_df)
        print(f"{config.channel_number}: {len(spotter_df)} packets")
    return spotter_data


def determine_packet_loss(spotter_dataframes: List[pd.DataFrame], receiver_dataframe: pd.DataFrame):
    merged_dfs = []
    for spotter_df in spotter_dataframes:
        channel = spotter_df['channel_number'].iloc[0]
        print(f"Evaluating performance for channel {channel}...")
        receiver_df = receiver_dataframe[receiver_dataframe['channel_number'] == channel]

        # Before sorting, make sure to reset the index to avoid alignment issues after merge_asof
        spotter_df_sorted = (spotter_df
                             .dropna(subset=['GPS_Epoch_Time(s)'])
                             .dropna(subset=['outz(mm)']) # in case there's a truncated line at the end
                             .sort_values('GPS_Epoch_Time(s)')
                             .reset_index(drop=True))
        receiver_df_sorted = receiver_df.sort_values('epoch_time').reset_index(drop=True)

        # Efficiently match packets using merge_asof with a tolerance since times may not match exactly
        merged_df = pd.merge_asof(spotter_df_sorted, receiver_df_sorted,
                                  by='channel_number',
                                  left_on='GPS_Epoch_Time(s)', right_on='epoch_time',
                                  direction='nearest', tolerance=0.1)

        # Rename the receiver columns to indicate they are the received data
        merged_df.rename(columns={
            'epoch_time': 'rec_epoch_time',
            'outx': 'rec_outx',
            'outy': 'rec_outy',
            'outz': 'rec_outz'}, inplace=True)

        # Mark packets as delivered or not based on if rec_epoch_time is not NaN
        merged_df['packet_delivered'] = ~merged_df['rec_epoch_time'].isna()

        # Check if the packet data is correct (convert to integers first)
        # - Allow off by +/- 1mm to account for rounding errors.
        # Use different values for fillna to ensure any NaNs are flagged
        corruption_check_conditions = (
            merged_df['packet_delivered'] &
            (abs(merged_df['rec_outx'].fillna(-10).astype(int) - merged_df['outx(mm)'].fillna(10).astype(int)) <= 1) &
            (abs(merged_df['rec_outy'].fillna(-10).astype(int) - merged_df['outy(mm)'].fillna(10).astype(int)) <= 1) &
            (abs(merged_df['rec_outz'].fillna(-10).astype(int) - merged_df['outz(mm)'].fillna(10).astype(int)) <= 1)
        )
        merged_df['packet_data_correct'] = corruption_check_conditions
        merged_df.loc[~corruption_check_conditions, 'packet_data_correct'] = False # ensure any NaNs are set False

        num_packets_attempted = len(merged_df)
        num_packets_delivered = len(merged_df[(merged_df['packet_delivered'] == True)])
        num_packets_lost = len(merged_df[(merged_df['packet_delivered'] == False)])
        num_packets_lost_or_corrupted = len(merged_df[(merged_df['packet_data_correct'] == False)])

        # Print out packet loss rate
        packet_loss_rate = 1 - merged_df['packet_delivered'].mean()
        print(f"Channel {channel} packet loss rate: {packet_loss_rate:.2%} ({num_packets_lost} of {num_packets_attempted})")

        # Print out packet corruption rate
        packet_corruption_rate = 1 - merged_df[(merged_df['packet_delivered'] == True)]['packet_data_correct'].mean()
        print(f"Channel {channel} packet corruption rate: {packet_corruption_rate:.2%} ({num_packets_lost_or_corrupted - num_packets_lost} of {num_packets_attempted})")
        merged_dfs.append(merged_df)

    return merged_dfs


def plot_packet_performance(merged_df, channel_number, grouping_window_sec = 40, show = False):
    # merged_df is assumed to be a merged DataFrame
    # merged_df['epoch_time'] should be the time in seconds or a proper DateTime format
    # Converting epoch time to a readable format, assuming epoch_time is in seconds
    merged_df['time'] = pd.to_datetime(merged_df['GPS_Epoch_Time(s)'], unit='s')

    group_freq = f"{grouping_window_sec}S"

    # Group by the defined frequency and calculate necessary aggregations
    print(f"Grouping performance data for channel {channel_number}")
    grouped = merged_df.groupby(pd.Grouper(key='time', freq=group_freq)).agg({
        'packet_delivered': lambda x: (x == False).sum(),  # Count of undelivered packets
        'packet_data_correct': lambda x: ((x == False) & (merged_df.loc[x.index, 'packet_delivered'] == True)).sum()  # Count of corrupted but delivered packets
    })
    # Directly calculate total_packets as the size of each group
    grouped['total_packets'] = merged_df.groupby(pd.Grouper(key='time', freq=group_freq)).size()

    # Calculate the percentage of packet loss and corruption
    grouped['packet_loss_rate'] = (grouped['packet_delivered'] / grouped['total_packets']) * 100
    grouped['packet_corruption_rate'] = (grouped['packet_data_correct'] / grouped['total_packets']) * 100

    print(f"Generating plot for channel {channel_number}")
    # Plotting
    plt.figure(figsize=(15, 7))
    plt.plot(grouped.index, grouped['packet_loss_rate'], label='Packet Loss Rate (%)', color='red')
    plt.plot(grouped.index, grouped['packet_corruption_rate'], label='Packet Corruption Rate (%)', color='blue')
    plt.title(f'Channel {channel_number} Packet Loss and Corruption Rate % Over Time, {grouping_window_sec} sec window')
    plt.xlabel('Time')
    plt.ylabel('Rate (%)')
    plt.legend()
    plt.grid(True)
    plt.xticks(rotation=45)  # Rotate the x-axis labels for better readability
    plt.tight_layout()  # Adjust layout to make room for label rotation
    plt.legend()
    plt.grid(True)
    print(f"Saving plot for channel {channel_number}")
    plt.savefig(f"channel_{channel_number}_performance.jpg")  # Save the plot as a JPEG file
    if show:
        plt.show()
