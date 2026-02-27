#!/usr/bin/env python3
"""
MVP-W Test Server
Simple WebSocket server with GUI for testing audio streaming

Usage:
    python main.py [--host HOST] [--port PORT]

Requirements:
    pip install websockets PyQt6 pyaudio numpy
"""

import sys
import asyncio
import json
import base64
import argparse
from datetime import datetime
from collections import deque

# Qt imports
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
                             QHBoxLayout, QTextEdit, QPushButton, QLabel,
                             QSlider, QGroupBox, QComboBox, QSpinBox)
from PyQt6.QtCore import QThread, pyqtSignal, Qt, QTimer
from PyQt6.QtGui import QFont, QColor

# Try to import pyaudio
try:
    import pyaudio
    PYAUDIO_AVAILABLE = True
except ImportError:
    PYAUDIO_AVAILABLE = False
    print("Warning: pyaudio not available, audio playback disabled")

# Try to import websockets
try:
    import websockets
    from websockets.server import serve
    WEBSOCKETS_AVAILABLE = True
except ImportError:
    WEBSOCKETS_AVAILABLE = False
    print("Error: websockets not available. Install with: pip install websockets")


class AudioPlayer:
    """Simple audio player using PyAudio"""

    def __init__(self, sample_rate=16000, channels=1):
        self.sample_rate = sample_rate
        self.channels = channels
        self.audio = None
        self.stream = None

        if PYAUDIO_AVAILABLE:
            self.audio = pyaudio.PyAudio()

    def play(self, data: bytes):
        """Play audio data"""
        if not self.audio:
            return False

        try:
            if self.stream is None:
                self.stream = self.audio.open(
                    format=pyaudio.paInt16,
                    channels=self.channels,
                    rate=self.sample_rate,
                    output=True
                )
            self.stream.write(data)
            return True
        except Exception as e:
            print(f"Audio playback error: {e}")
            return False

    def close(self):
        """Close audio stream"""
        if self.stream:
            self.stream.stop_stream()
            self.stream.close()
            self.stream = None
        if self.audio:
            self.audio.terminate()


class WebSocketThread(QThread):
    """WebSocket server running in background thread"""

    # Signals
    client_connected = pyqtSignal(str)
    client_disconnected = pyqtSignal(str)
    message_received = pyqtSignal(str, str)  # client_id, message
    audio_received = pyqtSignal(str, int)  # client_id, size
    log_signal = pyqtSignal(str)
    server_started = pyqtSignal(int)
    server_stopped = pyqtSignal()

    def __init__(self, host='0.0.0.0', port=8766):
        super().__init__()
        self.host = host
        self.port = port
        self.running = False
        self.clients = {}
        self.audio_player = AudioPlayer()
        self.loop = None

    async def handle_client(self, websocket):
        """Handle a WebSocket client"""
        client_addr = websocket.remote_address
        client_id = f"{client_addr[0]}:{client_addr[1]}"
        self.clients[websocket] = client_id

        self.client_connected.emit(client_id)
        self.log_signal.emit(f"Client connected: {client_id}")

        try:
            async for message in websocket:
                await self.process_message(websocket, client_id, message)
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            if websocket in self.clients:
                del self.clients[websocket]
            self.client_disconnected.emit(client_id)
            self.log_signal.emit(f"Client disconnected: {client_id}")

    async def process_message(self, websocket, client_id, message):
        """Process incoming message"""
        try:
            # Try to parse as JSON
            data = json.loads(message)
            msg_type = data.get('type', 'unknown')

            if msg_type == 'audio':
                # Handle audio data
                audio_b64 = data.get('data', '')
                if audio_b64:
                    try:
                        audio_data = base64.b64decode(audio_b64)
                        self.audio_received.emit(client_id, len(audio_data))
                        self.audio_player.play(audio_data)
                    except Exception as e:
                        self.log_signal.emit(f"Audio decode error: {e}")

            elif msg_type == 'audio_end':
                self.log_signal.emit(f"[{client_id}] Audio stream ended")

            else:
                # Other message types
                self.message_received.emit(client_id, json.dumps(data, indent=2)[:500])

        except json.JSONDecodeError:
            # Not JSON, log as raw
            self.message_received.emit(client_id, message[:200])

    async def async_main(self):
        """Main async function"""
        self.running = True

        try:
            async with websockets.serve(self.handle_client, self.host, self.port):
                self.server_started.emit(self.port)
                self.log_signal.emit(f"Server started on ws://{self.host}:{self.port}")
                while self.running:
                    await asyncio.sleep(0.1)
        except Exception as e:
            self.log_signal.emit(f"Server error: {e}")
        finally:
            self.server_stopped.emit()

    def run(self):
        """Run the thread"""
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        try:
            self.loop.run_until_complete(self.async_main())
        finally:
            self.loop.close()

    def stop(self):
        """Stop the server"""
        self.running = False
        self.audio_player.close()

    async def broadcast(self, message: str):
        """Broadcast message to all clients"""
        if not self.clients:
            return
        for ws in list(self.clients.keys()):
            try:
                await ws.send(message)
            except:
                pass


class MainWindow(QMainWindow):
    """Main application window"""

    def __init__(self, host='0.0.0.0', port=8766):
        super().__init__()
        self.host = host
        self.port = port
        self.ws_thread = None

        self.setWindowTitle("MVP-W Test Server")
        self.setMinimumSize(600, 500)

        # Stats
        self.audio_packets = 0
        self.audio_bytes = 0

        self.setup_ui()

    def setup_ui(self):
        """Setup the UI"""
        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)

        # Server control
        server_group = QGroupBox("Server Control")
        server_layout = QHBoxLayout(server_group)

        self.start_btn = QPushButton("Start Server")
        self.start_btn.clicked.connect(self.start_server)
        server_layout.addWidget(self.start_btn)

        self.stop_btn = QPushButton("Stop Server")
        self.stop_btn.clicked.connect(self.stop_server)
        self.stop_btn.setEnabled(False)
        server_layout.addWidget(self.stop_btn)

        self.status_label = QLabel("Stopped")
        self.status_label.setStyleSheet("color: red; font-weight: bold;")
        server_layout.addWidget(self.status_label)

        self.client_label = QLabel("Clients: 0")
        server_layout.addWidget(self.client_label)

        layout.addWidget(server_group)

        # Test commands
        cmd_group = QGroupBox("Test Commands")
        cmd_layout = QHBoxLayout(cmd_group)

        self.servo_btn = QPushButton("Send Servo Cmd")
        self.servo_btn.clicked.connect(self.send_servo_command)
        self.servo_btn.setEnabled(False)
        cmd_layout.addWidget(self.servo_btn)

        self.display_btn = QPushButton("Send Display Cmd")
        self.display_btn.clicked.connect(self.send_display_command)
        self.display_btn.setEnabled(False)
        cmd_layout.addWidget(self.display_btn)

        self.status_btn = QPushButton("Send Status Cmd")
        self.status_btn.clicked.connect(self.send_status_command)
        self.status_btn.setEnabled(False)
        cmd_layout.addWidget(self.status_btn)

        layout.addWidget(cmd_group)

        # Audio stats
        audio_group = QGroupBox("Audio Statistics")
        audio_layout = QHBoxLayout(audio_group)

        self.packets_label = QLabel("Packets: 0")
        audio_layout.addWidget(self.packets_label)

        self.bytes_label = QLabel("Bytes: 0")
        audio_layout.addWidget(self.bytes_label)

        self.reset_btn = QPushButton("Reset")
        self.reset_btn.clicked.connect(self.reset_stats)
        audio_layout.addWidget(self.reset_btn)

        layout.addWidget(audio_group)

        # Log
        log_group = QGroupBox("Log")
        log_layout = QVBoxLayout(log_group)

        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setFont(QFont("Consolas", 9))
        log_layout.addWidget(self.log_text)

        self.clear_btn = QPushButton("Clear Log")
        self.clear_btn.clicked.connect(lambda: self.log_text.clear())
        log_layout.addWidget(self.clear_btn)

        layout.addWidget(log_group)

    def start_server(self):
        """Start WebSocket server"""
        if not WEBSOCKETS_AVAILABLE:
            self.add_log("Error: websockets library not available")
            return

        self.ws_thread = WebSocketThread(self.host, self.port)
        self.ws_thread.client_connected.connect(self.on_client_connected)
        self.ws_thread.client_disconnected.connect(self.on_client_disconnected)
        self.ws_thread.message_received.connect(self.on_message_received)
        self.ws_thread.audio_received.connect(self.on_audio_received)
        self.ws_thread.log_signal.connect(self.add_log)
        self.ws_thread.server_started.connect(self.on_server_started)
        self.ws_thread.server_stopped.connect(self.on_server_stopped)

        self.ws_thread.start()

        self.start_btn.setEnabled(False)
        self.stop_btn.setEnabled(True)

    def stop_server(self):
        """Stop WebSocket server"""
        if self.ws_thread:
            self.ws_thread.stop()
            self.ws_thread.wait(2000)

        self.start_btn.setEnabled(True)
        self.stop_btn.setEnabled(False)

    def on_server_started(self, port):
        """Handle server started"""
        self.status_label.setText(f"Running on :{port}")
        self.status_label.setStyleSheet("color: green; font-weight: bold;")
        self.servo_btn.setEnabled(True)
        self.display_btn.setEnabled(True)
        self.status_btn.setEnabled(True)

    def on_server_stopped(self):
        """Handle server stopped"""
        self.status_label.setText("Stopped")
        self.status_label.setStyleSheet("color: red; font-weight: bold;")
        self.client_label.setText("Clients: 0")
        self.servo_btn.setEnabled(False)
        self.display_btn.setEnabled(False)
        self.status_btn.setEnabled(False)

    def on_client_connected(self, client_id):
        """Handle client connection"""
        self.add_log(f"✅ Connected: {client_id}")
        self.update_client_count()

    def on_client_disconnected(self, client_id):
        """Handle client disconnection"""
        self.add_log(f"❌ Disconnected: {client_id}")
        self.update_client_count()

    def update_client_count(self):
        """Update client count label"""
        if self.ws_thread:
            count = len(self.ws_thread.clients)
            self.client_label.setText(f"Clients: {count}")

    def on_message_received(self, client_id, message):
        """Handle received message"""
        self.add_log(f"[{client_id}] {message}")

    def on_audio_received(self, client_id, size):
        """Handle received audio"""
        self.audio_packets += 1
        self.audio_bytes += size
        self.packets_label.setText(f"Packets: {self.audio_packets}")
        self.bytes_label.setText(f"Bytes: {self.audio_bytes:,}")

    def reset_stats(self):
        """Reset audio statistics"""
        self.audio_packets = 0
        self.audio_bytes = 0
        self.packets_label.setText("Packets: 0")
        self.bytes_label.setText("Bytes: 0")

    def send_servo_command(self):
        """Send servo test command"""
        msg = json.dumps({"type": "servo", "x": 90, "y": 45})
        self.send_command(msg)

    def send_display_command(self):
        """Send display test command"""
        msg = json.dumps({"type": "display", "text": "Hello from PC!", "emoji": "happy", "size": 24})
        self.send_command(msg)

    def send_status_command(self):
        """Send status test command"""
        msg = json.dumps({"type": "status", "state": "thinking", "message": "Processing audio..."})
        self.send_command(msg)

    def send_command(self, msg):
        """Send command to all clients"""
        if self.ws_thread and self.ws_thread.loop:
            asyncio.run_coroutine_threadsafe(
                self.ws_thread.broadcast(msg),
                self.ws_thread.loop
            )
            self.add_log(f"Sent: {msg[:100]}")

    def add_log(self, message):
        """Add log message"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.append(f"[{timestamp}] {message}")

    def closeEvent(self, event):
        """Handle window close"""
        self.stop_server()
        event.accept()


def main():
    parser = argparse.ArgumentParser(description='MVP-W Test Server')
    parser.add_argument('--host', default='0.0.0.0', help='Host to bind to')
    parser.add_argument('--port', type=int, default=8766, help='Port to listen on')
    args = parser.parse_args()

    app = QApplication(sys.argv)
    app.setStyle('Fusion')

    window = MainWindow(args.host, args.port)
    window.show()

    sys.exit(app.exec())


if __name__ == '__main__':
    main()
