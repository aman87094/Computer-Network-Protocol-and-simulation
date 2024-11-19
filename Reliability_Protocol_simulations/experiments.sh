#!/bin/bash

# Define the loss and mean values
loss_values=(0.1 0.5 1.0 1.5 2.0 5.0)
mean_values=(50 100 150 200 250 500)

# Create CSV file with headers
echo "Loss,Mean,Download Time (s)" > experiment_results.csv

sudo tc qdisc del dev lo root netem &

# Loop through each combination of loss and mean
for loss in "${loss_values[@]}"; do
  for mean in "${mean_values[@]}"; do
    echo "Running experiment with loss $loss% and mean $mean ms..."
    
    # Set up network parameters
    sudo tc qdisc add dev lo root netem loss $loss% delay ${mean}ms 10ms distribution normal rate 1600Kbit&
    
    # Run sender.py in the background
    python3 sender.py SW &
    sender_pid=$!  # Capture PID of sender process
    
    # Run receiver.py in the background and capture download time
    download_time=$(python3 receiver.py "SWcopyimg${loss}_${mean}.jpg" | tail -n 1)
    
    # Wait for the sender and receiver processes to complete
    wait $sender_pid
    
    echo "Download time: $download_time seconds"
    
    # Save results to CSV file
    echo "$loss,$mean,$download_time" >> experiment_results.csv

    sudo tc qdisc del dev lo root netem &
    
    echo "Experiment completed."
  done
done
