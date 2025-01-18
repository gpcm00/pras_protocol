import socket

listen_ip = "127.0.0.1"
listen_port = 52021

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

sock.bind((listen_ip, listen_port))
print(f"Listening for UDP messages on {listen_ip}:{listen_port}")

try:
    while True:
        data, addr = sock.recvfrom(1024) 
        print(f"Received message from {addr}: {data.decode()}")
except KeyboardInterrupt:
    print("\nStopped listening.")
finally:
    sock.close()
