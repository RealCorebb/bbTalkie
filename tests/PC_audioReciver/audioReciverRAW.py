import socket
import pyaudio
import threading

HOST = '0.0.0.0'
PORT = 8765
SAMPLE_RATE = 16000
CHANNELS = 1
BUFFER_SIZE = 1024  # Match ESP32 send size (can adjust)

class AudioPlayer:
    def __init__(self):
        self.pyaudio_instance = pyaudio.PyAudio()
        self.stream = self.pyaudio_instance.open(
            format=pyaudio.paInt16,
            channels=CHANNELS,
            rate=SAMPLE_RATE,
            output=True,
            frames_per_buffer=BUFFER_SIZE
        )

    def play_pcm(self, pcm_data):
        self.stream.write(pcm_data)

    def close(self):
        self.stream.stop_stream()
        self.stream.close()
        self.pyaudio_instance.terminate()

def handle_client(conn, addr, player):
    print(f"[+] Connection from {addr}")
    try:
        while True:
            data = conn.recv(BUFFER_SIZE)
            if not data:
                break
            print(f"Received {len(data)} bytes")
            player.play_pcm(data)
    except Exception as e:
        print(f"[!] Error with client {addr}: {e}")
    finally:
        print(f"[-] Client {addr} disconnected")
        conn.close()

def main():
    player = AudioPlayer()
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen(1)
        print(f"[i] Server listening on {HOST}:{PORT}")
        try:
            while True:
                conn, addr = s.accept()
                threading.Thread(target=handle_client, args=(conn, addr, player), daemon=True).start()
        except KeyboardInterrupt:
            print("\n[!] Stopping server...")
        finally:
            player.close()

if __name__ == "__main__":
    main()
