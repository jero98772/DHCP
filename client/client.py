# client.py
import socket

HOST = '127.0.0.1'  # The server's hostname or IP address
PORT = 8080         # The port used by the server

def main():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        message = "Hello from Python client"
        s.sendall(message.encode())
        data = s.recv(1024)
        print(f"Received from server: {data.decode()}")

if __name__ == "__main__":
    main()
