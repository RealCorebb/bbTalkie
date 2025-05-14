#!/usr/bin/env python3
import socket
import pyaudio
import audioop
import threading
import queue
import argparse
import time

# Audio settings - match with ESP32 configuration
SAMPLE_RATE = 16000  # Update this to match your ESP32 SAMPLE_RATE
BIT_DEPTH = 16       # Update this to match your ESP32 BIT_DEPTH
CHANNELS = 1         # Your ESP configuration shows mono audio
CHUNK_SIZE = 512     # Buffer size for audio playback
BUFFER_SIZE = 10     # Number of chunks to buffer

class G711Player:
    def __init__(self, host='0.0.0.0', port=8765):
        self.host = host
        self.port = port
        self.socket = None
        self.audio = None
        self.stream = None
        self.audio_queue = queue.Queue(maxsize=BUFFER_SIZE)
        self.running = False
        
    def start(self):
        """Start the TCP server and audio playback"""
        self.running = True
        
        # Initialize PyAudio
        self.audio = pyaudio.PyAudio()
        self.stream = self.audio.open(
            format=pyaudio.paInt16,
            channels=CHANNELS,
            rate=SAMPLE_RATE,
            output=True,
            frames_per_buffer=CHUNK_SIZE
        )
        
        # Start the playback thread
        playback_thread = threading.Thread(target=self.playback_worker)
        playback_thread.daemon = True
        playback_thread.start()
        
        # Start the TCP server
        self.start_tcp_server()
        
    def start_tcp_server(self):
        """Start TCP server to receive audio data"""
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind((self.host, self.port))
        self.socket.listen(1)
        
        print(f"Listening for ESP32 connection on {self.host}:{self.port}")
        
        while self.running:
            try:
                client_socket, address = self.socket.accept()
                print(f"Connection from ESP32 established: {address}")
                self.handle_client(client_socket)
            except Exception as e:
                print(f"Server error: {e}")
                if not self.running:
                    break
    
    def handle_client(self, client_socket):
        """Handle incoming connection and data"""
        buffer = bytearray()
        
        while self.running:
            try:
                data = client_socket.recv(1024)
                if not data:
                    print("Connection closed by ESP32")
                    break
                    
                # Add received data to buffer
                buffer.extend(data)
                
                # Process complete chunks
                while len(buffer) >= CHUNK_SIZE // 2:  # G711 is 8-bit (PCM is 16-bit)
                    chunk = buffer[:CHUNK_SIZE // 2]
                    buffer = buffer[CHUNK_SIZE // 2:]
                    
                    # Decode G711 A-law to PCM
                    pcm_data = self.g711a_to_pcm(chunk)
                    
                    # Add to playback queue
                    try:
                        self.audio_queue.put(pcm_data, block=False)
                    except queue.Full:
                        # Drop oldest frame if queue is full
                        self.audio_queue.get()
                        self.audio_queue.put(pcm_data)
                        
            except Exception as e:
                print(f"Error handling client data: {e}")
                break
                
        client_socket.close()
    
    def g711a_to_pcm(self, g711_data):
        """Convert G711 A-law data to PCM"""
        # Using audioop's alaw2lin function to convert from A-law to PCM
        pcm_data = audioop.alaw2lin(g711_data, 2)  # 2 bytes per sample (16-bit)
        return pcm_data
    
    def playback_worker(self):
        """Worker thread for audio playback"""
        print("Audio playback thread started")
        
        # Wait for some initial buffering
        print("Buffering audio...")
        while self.running and self.audio_queue.qsize() < BUFFER_SIZE // 2:
            time.sleep(0.1)
            
        print("Starting playback")
        
        while self.running:
            try:
                # Get audio from queue with timeout
                pcm_data = self.audio_queue.get(timeout=1.0)
                self.stream.write(pcm_data)
            except queue.Empty:
                # No data available, output silence
                silence = b'\x00' * CHUNK_SIZE * CHANNELS * (BIT_DEPTH // 8)
                self.stream.write(silence)
                print("Audio buffer underrun (silence inserted)")
            except Exception as e:
                print(f"Playback error: {e}")

    def stop(self):
        """Stop the player and clean up resources"""
        self.running = False
        
        # Close socket
        if self.socket:
            self.socket.close()
            
        # Close audio
        if self.stream:
            self.stream.stop_stream()
            self.stream.close()
            
        if self.audio:
            self.audio.terminate()
            
        print("G711 Player stopped")

def main():
    parser = argparse.ArgumentParser(description='ESP32 G711 Audio Player')
    parser.add_argument('--host', default='0.0.0.0', help='Host IP to bind to')
    parser.add_argument('--port', type=int, default=8765, help='Port to listen on')
    args = parser.parse_args()
    
    player = G711Player(host=args.host, port=args.port)
    
    try:
        player.start()
    except KeyboardInterrupt:
        print("\nStopping player...")
    finally:
        player.stop()

if __name__ == "__main__":
    main()