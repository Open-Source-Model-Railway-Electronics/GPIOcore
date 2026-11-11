# Transceiver.py

import serial
import time

OPCODE_ACK  = 0x70
OPCODE_NACK = 0x71

NACK_INVALID_CHECKSUM = 1
NACK_INVALID_LENGTH   = 2
NACK_TIMEOUT           = 3

class Message:
    def __init__(self):
        self.command = 0
        self.payload = bytearray(32)

class Transceiver:
    def __init__(self, port=None, baudrate=115200):
        self.ser = None
        self.messageReceived = None
        if port:
            self.ser = serial.Serial(port, baudrate=baudrate, timeout=0.01)

        self.state = 0
        self.len = 0
        self.byteCounter = 0
        self.mess = Message()
        self.checksum = 0
        self.lastByteTime = 0
        self.maxLen = 32

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def sendMessage(self, mess2send):
        length = mess2send.command & 0x0F
        if length == 0x0F:
            length = mess2send.payload[0]

        checksum = mess2send.command
        for i in range(length):
            checksum ^= mess2send.payload[i]

        self.ser.write(bytes([mess2send.command]))
        for i in range(length):
            self.ser.write(bytes([mess2send.payload[i]]))
        self.ser.write(bytes([checksum]))

    def sendAck(self):
        self.ser.write(bytes([OPCODE_ACK]))

    def sendNack(self, reason):
        self.ser.write(bytes([OPCODE_NACK, reason]))

    def receive(self):
        while self.ser.in_waiting:
            b = self.ser.read(1)[0]
            now = time.ticks_ms()

            # timeout check (5 ms tussen bytes)
            if self.state != 0 and time.ticks_diff(now, self.lastByteTime) > 5:
                self.sendNack(NACK_TIMEOUT)
                self.state = 0
                return 0

            self.lastByteTime = now

            if self.state == 0:
                self.mess.command = b
                self.len = b & 0x0F
                self.byteCounter = 0
                self.checksum = b

                if self.len == 0x00:
                    return self._handleMessage()
                elif self.len == 0x0F:
                    self.state = 1
                elif self.len > self.maxLen:
                    self.sendNack(NACK_INVALID_LENGTH)
                    self.state = 0
                else:
                    self.state = 2

            elif self.state == 1:
                if b > self.maxLen:
                    self.sendNack(NACK_INVALID_LENGTH)
                    self.state = 0
                    return 0
                self.len = b
                self.mess.payload[0] = b
                self.byteCounter = 1
                self.checksum ^= b
                self.state = 2

            elif self.state == 2:
                if self.byteCounter >= self.maxLen:
                    self.sendNack(NACK_INVALID_LENGTH)
                    self.state = 0
                    return 0

                self.mess.payload[self.byteCounter] = b
                self.checksum ^= b
                self.byteCounter += 1

                if self.byteCounter == self.len:
                    self.state = 3  # wacht op checksum

            elif self.state == 3:
                if b == self.checksum:
                    self.sendAck()
                    return self._handleMessage()
                else:
                    self.sendNack(NACK_INVALID_CHECKSUM)
                    self.state = 0
                    return 0

        return 0

    def _handleMessage(self):
        if self.messageReceived:
            self.messageReceived(self.mess, self.len)
        self.state = 0
        return 1
