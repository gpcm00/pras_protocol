import socket

# Define target address and ports
target_address = ('localhost', 80)
source_port = 512
message = "POWERMODULE_ADC_DEVICE_STREAM_REQUEST_0123456789"

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Bind the socket to the source port (512)
sock.bind(('localhost', source_port))

try:
    # Send the message to the target address (localhost:80)
    print(f"Sending message from port {source_port} to {target_address[0]}:{target_address[1]}")
    sock.sendto(message.encode(), target_address)

    # Loop to receive responses indefinitely
    print("Listening for responses...")
    while True:
        data, server = sock.recvfrom(4096)  # Buffer size is 4096 bytes
        print(f"Received {len(data)} bytes response: {data} from {server}")

except KeyboardInterrupt:
    print("Stopped by user.")
finally:
    sock.close()
