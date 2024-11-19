import socket
import time
import os
import struct
import random
import numpy as np
import matplotlib.pyplot as plt
import sys

DROP_PROB = 8


# Creates a packet from a sequence number and byte data
def make(seq_num, data = b''):
    seq_bytes = seq_num.to_bytes(4, byteorder = 'little', signed = True)
    return seq_bytes + data

# Creates an empty packet
def make_empty():
    return b''

# Extracts sequence number and data from a non-empty packet
def extract(packet):
    seq_num = int.from_bytes(packet[0:4], byteorder = 'little', signed = True)
    return seq_num, packet[4:]



class PacketGenerator:
    def __init__(self, filename, packet_size):
        self.filename = filename
        self.packet_size = packet_size

    def generate_packets(self):
        packets = []
        seq_num=0
        with open(self.filename, 'rb') as file:
            while True:
                data = file.read(self.packet_size)
                if not data:
                    break
                packets.append(make(seq_num,data))
                seq_num+=1
        return packets

import _thread
import time

class Timer(object):
    TIMER_STOP = -1

    def __init__(self, duration):
        self._start_time = self.TIMER_STOP
        self._duration = duration

    # Starts the timer
    def start(self):
        if self._start_time == self.TIMER_STOP:
            self._start_time = time.time()

    # Stops the timer
    def stop(self):
        if self._start_time != self.TIMER_STOP:
            self._start_time = self.TIMER_STOP

    # Determines whether the timer is runnning
    def running(self):
        return self._start_time != self.TIMER_STOP

    # Determines whether the timer timed out
    def timeout(self):
        if not self.running():
            return False
        else:
            return time.time() - self._start_time >= self._duration
    

class Sender:
    def __init__(self, protocol, window_size, rto, packets, server_address, server_port,packet_size):
        self.protocol = protocol
        self.window_size = window_size
        self.rto = rto
        self.packets = packets
        self.server_address = server_address
        self.server_port = server_port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.seq_num=0
        self.ack_expected=0
        self.mutex = _thread.allocate_lock()
        self.send_timer=Timer(rto)
        self.base=0
        self.sleep_interval=5
        self.SLEEP_INTERVAL=5
        self.DROP_PROB = 8
        self.sr_buffer = set()
        self.max_packet_size = 2*packet_size #for safety


    def send(packet, sock, addr):
        # if random.randint(0, DROP_PROB) > 0:
        sock.sendto(packet, addr)
        # return

    # Receive a packet from the unreliable channel
    def recv(self,sock):
        packet, addr = sock.recvfrom(self.max_packet_size)
        return packet, addr
    

    def send_packets(self):
        if self.protocol == 'SW':
            self.stop_and_wait()
        elif self.protocol == 'GBN':
            self.go_back_n()
        elif self.protocol == 'SR':
            self.selective_repeat()

    def stop_and_wait(self):
        # Implement Stop-and-Wait protocol
        # global seq_num,ack_expected
        while self.ack_expected < len(self.packets):
            packet = self.packets[self.seq_num]
            self.sock.sendto(packet, (self.server_address, self.server_port))
            start_time = time.time()
            self.sock.settimeout(self.rto)
            try:
                pkt, _ = self.sock.recvfrom(self.max_packet_size)
                ack, _ = extract(pkt)
                end_time = time.time()
                if ack == self.ack_expected:
                    print(f"Ack {self.ack_expected} received in {end_time - start_time} seconds")
                    self.ack_expected += 1
                    self.seq_num += 1
            except socket.timeout:
                print(f"Timeout for packet {self.seq_num}, resending...")
                continue
        self.sock.sendto(make_empty(), (self.server_address, self.server_port))
        print("File transfer complete.")


    def go_back_n(self):
        # Implement Go-Back-N protocol
        num_packets = len(self.packets)
        # print('I gots', num_packets)
        self.window_size = min(self.window_size,num_packets-self.base)
        next_to_send = 0
        self.base = 0
        
        # Start the receiver thread
        _thread.start_new_thread(self.receive, (self.sock,))
        while self.base < num_packets:
            self.mutex.acquire()
            # Send all the packets in the window
            while next_to_send < self.base + self.window_size:
                # print('Sending packet', next_to_send)
                # self.send(self.packets[next_to_send], self.sock, self.RECEIVER_ADDR)
                if(next_to_send>=num_packets):
                    break
                self.sock.sendto(self.packets[next_to_send], (self.server_address, self.server_port))
                next_to_send += 1
            self.mutex.release()
            # Start the timer
            if not self.send_timer.running():
                # print('Starting timer')
                self.send_timer.start()

            # Wait until a timer goes off or we get an ACK
            while self.send_timer.running() and not self.send_timer.timeout():
                # self.mutex.release()
                # print('Sleeping')
                # time.sleep(self.SLEEP_INTERVAL)
                # self.mutex.acquire()
                pass

            if self.send_timer.timeout():
                # Looks like we timed out
                # print('Timeout ',)
                print(f"Timeout for packet {self.seq_num}, resending...")
                self.send_timer.stop()
                next_to_send = self.base
            else:
                # print('Shifting window')
                self.window_size = min(self.window_size,num_packets-self.base)
            # self.mutex.release()
        # Send empty packet as sentinel
        # self.send(self.packet.make_empty(), self.sock, self.RECEIVER_ADDR)
        self.sock.sendto(make_empty(), (self.server_address, self.server_port))
        print("File transfer complete.")
        
    def receive(self,sock):
        while True:
            pkt, _ = self.recv(sock)
            ack, _ = extract(pkt)
            # If we get an ACK for the first in-flight packet
            print('Got ACK', ack)
            if (ack == self.base):
                self.mutex.acquire()
                self.base = ack + 1
                # print('Base updated', self.base)
                self.send_timer.stop()
                self.mutex.release()


    def selective_repeat(self):
        # Implement Go-Back-N protocol
        num_packets = len(self.packets)
        # print('I gots', num_packets)
        self.window_size = min(self.window_size,num_packets-self.base)
        next_to_send = 0
        self.base = 0
        self.sr_buffer = set()

        # Start the receiver thread
        _thread.start_new_thread(self.receive_sr, (self.sock,))
        while self.base < num_packets:
            self.mutex.acquire()
            # Send all the packets in the window
            while next_to_send < self.base + self.window_size:
                if(next_to_send in self.sr_buffer):#############################################3
                    # print("Skipping ",next_to_send)
                    next_to_send += 1
                    continue
                # print('Sending packet', next_to_send)
                # self.send(self.packets[next_to_send], self.sock, self.RECEIVER_ADDR)
                if(next_to_send>=num_packets):
                    break
                self.sock.sendto(self.packets[next_to_send], (self.server_address, self.server_port))
                next_to_send += 1
            self.mutex.release()
            # Start the timer
            if not self.send_timer.running():
                # print('Starting timer')
                self.send_timer.start()

            # Wait until a timer goes off or we get an ACK
            while self.send_timer.running() and not self.send_timer.timeout():
                # self.mutex.release()
                # print('Sleeping')
                # time.sleep(self.SLEEP_INTERVAL)
                # self.mutex.acquire()
                pass

            if self.send_timer.timeout():
                # Looks like we timed out
                # print('Timeout')
                print(f"Timeout for packet {self.seq_num}, resending...")
                self.send_timer.stop()
                next_to_send = self.base
            else:
                # print('Shifting window')
                window_size = min(self.window_size,num_packets-self.base)
            # self.mutex.release()
        # Send empty packet as sentinel
        # self.send(self.packet.make_empty(), self.sock, self.RECEIVER_ADDR)
        self.sock.sendto(make_empty(), (self.server_address, self.server_port))
        print("File transfer complete.")
###############################################3
    def receive_sr(self,sock):
        while True:
            pkt, _ = self.recv(sock)
            ack, _ = extract(pkt)
            print('Got ACK', ack)
            if (ack == self.base):
                self.mutex.acquire()
                self.base = ack + 1
                print('Base updated', self.base)
                self.send_timer.stop()
                while(self.base in self.sr_buffer):
                    self.sr_buffer.remove(self.base)
                    self.base += 1
                self.mutex.release()
            else:
                self.mutex.acquire()
                # print("current buffer ",self.sr_buffer)
                self.sr_buffer.add(ack)
                self.mutex.release()

def run(protocol):
    # System A
    
    #Parameters
    filename = 'loco.jpg'
    packet_size = 25*1024  # 25KB
    rto = 1 #200ms
    window_size = 5

    generator = PacketGenerator(filename, packet_size)
    packets = generator.generate_packets()
    print(len(packets))
    
    s=Sender(protocol,window_size,rto,packets,'127.0.0.1',2000,packet_size)
    # s=Sender('SW',window_size,rto,packets,'127.0.0.1',12345,packet_size)
    # s=Sender('GBN',window_size,rto,packets,'127.0.0.1',12345,packet_size)
    # s=Sender('SR',window_size,rto,packets,'127.0.0.1',12345,packet_size)
    s.send_packets()

    # System B
    # server_address = '127.0.0.1'
    # server_port = 12345
    
    

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print('Expected filename as command line argument')
        exit()
    protocol = sys.argv[1]
    run(protocol)