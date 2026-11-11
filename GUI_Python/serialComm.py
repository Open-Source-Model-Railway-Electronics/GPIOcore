import serial
import serial.tools.list_ports

class SerialManager:
    def __init__(self):
        self.ser = None

    def list_ports(self):
        return [port.device for port in serial.tools.list_ports.comports()]

    def connect(self, port):
        try:
            self.ser = serial.Serial(port, 115200, timeout=1)
            print(f"Connected to {port}")
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False

    def disconnect(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("Disconnected.")

    def send(self, data):
        if self.ser and self.ser.is_open:
            self.ser.write(data)

    def receive(self):
        if self.ser and self.ser.is_open:
            return self.ser.readline()
        return None
