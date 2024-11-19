# receiver.py - The receiver in the reliable data transer protocol
# import packet
import socket
import sys
import random
import time
# import udt

DROP_PROB=0.001

def send(packet, sock, addr):
    # if random.randint(0, DROP_PROB) > 0:
        sock.sendto(packet, addr)
    # return

# Receive a packet from the unreliable channel
def recv(sock):
    packet, addr = sock.recvfrom(25*1024*2)
    return packet, addr
    

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



arr=[]
reordering = dict()


# Receive packets from the sender
def receive(sock, filename):
    # Open the file for writing
    try:
        file = open(filename, 'wb')
    except IOError:
        print('Unable to open', filename)
        return
    
    expected_num = 0
    not_started = True
    while True:
        # Get the next packet from the sender
        pkt, addr = recv(sock)
        if(not_started):
            start_time = time.time()
            not_started = False
        if not pkt:
            break
        seq_num, data = extract(pkt)
        # print('Got packet', seq_num)
        
        if(data==b''):
            break

        reordering[seq_num]=data
        
        # Send back an ACK
        if seq_num == expected_num:
            # print('Got expected packet')
            # print('Sending ACK', expected_num)
            pkt = make(expected_num)
            send(pkt, sock, addr)
            expected_num += 1
            arr.append(data)
            # file.write(data)
        ################################################
        else:
            # print('Sending ACK', seq_num)
            pkt = make(seq_num)
            send(pkt, sock, addr)

    end_time = time.time()

    # with open("values.txt","a") as f:
    #     f.write(f"{end_time-start_time}s")
    print(f"{end_time-start_time}s")

    key_list = sorted(reordering.keys())
    for key in key_list:
        file.write(reordering[key])
    file.close()
# Main function
if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Expected filename as command line argument')
        exit()
    # port = int(sys.argv[2])
    port = 2000
    RECEIVER_ADDR = ('127.0.0.1', port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(RECEIVER_ADDR) 
    filename = sys.argv[1]
    # print("receiver on")
    receive(sock, filename)
    sock.close()

