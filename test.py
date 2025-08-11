
import threading
import socket
import ssl
import struct
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes


UDP_PORT = 51234
CLIENT_CERT = "client.crt"
CLIENT_KEY = "client.key"

def aes_gcm_decrypt(ciphertext: bytes, key: bytes, iv: bytes, tag: bytes) -> bytes:
    if len(key) != 32:
        raise ValueError("Key must be 32 bytes for AES-256-GCM")

    decryptor = Cipher(
        algorithms.AES(key),
        modes.GCM(iv, tag)
    ).decryptor()

    plaintext = decryptor.update(ciphertext) + decryptor.finalize()
    return plaintext

def parse_input(user_input):
    try:
        hex_part, string_part = user_input.strip().split(' ', 1)
        
        if len(hex_part) != 2:
            raise ValueError("Hex part must be exactly two characters.")
        
        hex_value = int(hex_part, 16)
        
        return hex_value, string_part
    
    except ValueError as e:
        print(f"Invalid input: {e}")
        return None, None
    
def listen_udp_port():
    print(f"Listening on UDP port {UDP_PORT}...")
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.bind(('0.0.0.0', UDP_PORT))

    while True:
        data, addr = udp_sock.recvfrom(4096)

        print(f"UDP Message from {addr}: {data.decode(errors='replace')}")

def pad_to_32_bytes(data: bytes) -> bytes:
    """Ensure data is exactly 32 bytes: pad with null bytes or truncate."""
    return data[:32].ljust(32, b'\x00')

HOST = '127.0.0.1'
PORT = 61234

context = ssl.create_default_context(ssl.Purpose.SERVER_AUTH)
context.check_hostname = False
context.verify_mode = ssl.CERT_NONE  # For testing; set to CERT_REQUIRED if needed

context.load_cert_chain(certfile=CLIENT_CERT, keyfile=CLIENT_KEY)

udp_thread = threading.Thread(target=listen_udp_port, daemon=True)
udp_thread.start()

with socket.create_connection((HOST, PORT)) as sock:
    with context.wrap_socket(sock, server_hostname=HOST) as ssock:
        print("[*] Connected via TLS")

        port_bytes = struct.pack('>H', UDP_PORT)
        message = pad_to_32_bytes(port_bytes + b'open_request')
        ssock.sendall(message)
        print(f"[*] Sent: {message}")

        reply = ssock.recv(4096)
        print(f"[<] Server replied: {reply}")

        while True:
            try:
                user_input = input("Enter message: ")
                hex_val, text_val = parse_input(user_input)
                input_bytes = pad_to_32_bytes(bytes([hex_val]) + text_val.encode())
                ssock.sendall(input_bytes)
                response = ssock.recv(4096)
                print(f"[<] Server replied: {[f'0x{b:02x}' for b in response]}")
                print(len(response))
            except (KeyboardInterrupt, EOFError):
                print("\n[!] Exiting.")
                break
