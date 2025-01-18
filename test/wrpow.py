import socket
import sys

def main():
    host = '127.0.0.1'  # Localhost
    port = 80         # Port number

    try:
        # Create a TCP socket
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
            print(f"Connecting to {host}:{port}...")
            client_socket.connect((host, port))  # Connect to the server
            print("Connected! Type your message and press Enter to send.")
            print("Type 'exit' to quit.")

            
            # Take input from the terminal
            user_input = input("You: ")
                

             # Send input to the server
            client_socket.sendall(user_input.encode('utf-8'))
            print(f"Sent: {user_input}")

    except KeyboardInterrupt:
        print("\nInterrupted by user. Exiting...")
    except ConnectionRefusedError:
        print(f"Could not connect to {host}:{port}. Is the server running?")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        print("Client closed.")

if __name__ == "__main__":
    main()
