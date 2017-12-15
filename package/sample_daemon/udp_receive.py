#!/usr/bin/env python

import socket, sys

from sbp.client import Handler, Framer
from sbp.client.drivers.base_driver import BaseDriver

from sbp.system import SBP_MSG_HEARTBEAT

UDP_BIND_IP = ""
UDP_PORT = 56666

class UDPDriver(BaseDriver):
    def __init__(self):
        self.handle = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.handle.bind((UDP_BIND_IP, UDP_PORT))
        self.handle.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self._data = ''
    def _slice(self, size):
        data = self._data[:size]
        self._data = self._data[size:]
        return data
    def read(self, size):
        if len(self._data) > 0 and size <= len(self._data):
            return self._slice(size)
        data, addr = self.handle.recvfrom(max(size, 1024))
        self._data += data
        return self._slice(size)
    def write(self, b):
        raise NotImplemented()
    def flush(self):
        pass

def message_callback(msg, **metadata):
    print(msg)

with UDPDriver() as driver:
    with Handler(Framer(driver.read, None, verbose=True)) as source:
        try:
            while True:
                source.wait_callback(message_callback)
        except KeyboardInterrupt:
            pass
