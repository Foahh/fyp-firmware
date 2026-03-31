"""Serial communication for firmware and power monitor."""

import struct
import sys
import threading
import time

import serial


def open_serial_binary(port: str, baud: int, timeout_s: float) -> serial.Serial:
    """Open UART for length-prefixed protobuf (8N1, no flow control, binary-safe).

    Uses exclusive access on Linux when supported so ModemManager/other daemons
    cannot probe the same ttyACM device concurrently (a common cause of
    "connected" ports with no valid frames).
    """
    kw: dict = {
        "port": port,
        "baudrate": baud,
        "timeout": timeout_s,
        "write_timeout": timeout_s,
        "bytesize": serial.EIGHTBITS,
        "parity": serial.PARITY_NONE,
        "stopbits": serial.STOPBITS_ONE,
        "xonxoff": False,
        "rtscts": False,
        "dsrdtr": False,
    }
    try:
        ser = serial.Serial(**kw, exclusive=True)
    except (TypeError, ValueError, AttributeError):
        ser = serial.Serial(**kw)
    try:
        ser.dtr = False
        ser.rts = False
    except (AttributeError, OSError, ValueError):
        pass
    return ser


class SerialLink:
    def __init__(self, port: str, baud: int, timeout_s: float):
        self._ser = open_serial_binary(port, baud, timeout_s)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        self._tx_lock = threading.Lock()
        self._rx_buf = bytearray()
        self._max_frame_len = 16 * 1024

    def close(self) -> None:
        self._ser.close()

    def recv_device_message(self, messages_pb2):
        while True:
            while len(self._rx_buf) < 4:
                chunk = self._ser.read(4 - len(self._rx_buf))
                if not chunk:
                    raise IOError("Serial read timeout")
                self._rx_buf.extend(chunk)

            prefix = bytes(self._rx_buf[:4])
            length = struct.unpack("<I", prefix)[0]

            if length == 0 or length > self._max_frame_len:
                del self._rx_buf[0]
                continue

            need = 4 + length
            while len(self._rx_buf) < need:
                chunk = self._ser.read(need - len(self._rx_buf))
                if not chunk:
                    raise IOError("Serial read timeout")
                self._rx_buf.extend(chunk)

            payload = bytes(self._rx_buf[4:need])
            msg = messages_pb2.DeviceMessage()
            try:
                msg.ParseFromString(payload)
            except Exception as exc:
                print(f"[firmware] Protobuf parse error: {exc}", file=sys.stderr)
                del self._rx_buf[0]
                continue

            del self._rx_buf[:need]
            return msg

    def send_host_message(self, msg) -> None:
        payload = msg.SerializeToString()
        frame = struct.pack("<I", len(payload)) + payload
        with self._tx_lock:
            view = memoryview(frame)
            while view:
                written = self._ser.write(view)
                if written is None or written <= 0:
                    raise IOError("Serial write failed")
                view = view[written:]
            self._ser.flush()


class PowerLink:
    """Length-prefixed nanopb PowerSample frames over serial."""

    MAX_FRAME_LEN = 64
    HANDSHAKE_REQUEST = b"PM_PING\n"
    HANDSHAKE_ACK_PREFIX = "PM_ACK"

    def __init__(self, port: str, baud: int, timeout_s: float):
        self._ser = open_serial_binary(port, baud, timeout_s)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        self._handshake(timeout_s)
        self._ser.timeout = None

    def close(self) -> None:
        self._ser.close()

    def _read_exact(self, n: int) -> bytes:
        data = b""
        while len(data) < n:
            chunk = self._ser.read(n - len(data))
            if not chunk:
                raise IOError("Power serial read timeout")
            data += chunk
        return data

    def _handshake(self, timeout_s: float) -> None:
        """Verify monitor responds before binary frame decoding begins."""
        deadline = time.monotonic() + max(0.5, timeout_s)
        self._ser.write(self.HANDSHAKE_REQUEST)
        self._ser.flush()
        while time.monotonic() < deadline:
            line = self._ser.readline()
            if not line:
                continue
            text = line.decode("utf-8", errors="ignore").strip()
            if not text:
                continue
            if text.startswith(self.HANDSHAKE_ACK_PREFIX):
                print(f"[power] ACK: {text}")
                self._ser.reset_input_buffer()
                return
        raise IOError("Power monitor handshake timeout (missing PM_ACK)")

    def recv_power_sample(self, power_sample_pb2):
        """Returns a parsed PowerSample protobuf message."""
        while True:
            prefix = self._read_exact(4)
            length = struct.unpack("<I", prefix)[0]
            if length == 0 or length > self.MAX_FRAME_LEN:
                self._ser.read(1)
                continue
            payload = self._read_exact(length)
            sample = power_sample_pb2.PowerSample()
            try:
                sample.ParseFromString(payload)
            except Exception as exc:
                print(f"[power] Protobuf parse error: {exc}", file=sys.stderr)
                continue
            return sample


def probe_power_monitor_pm_ping(
    port: str, baud: int, handshake_timeout_s: float
) -> bool:
    """Return True if the port responds to PM_PING with a PM_ACK line (ESP32 monitor)."""
    ser = open_serial_binary(port, baud, 0.1)
    try:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        ser.write(PowerLink.HANDSHAKE_REQUEST)
        ser.flush()
        deadline = time.monotonic() + max(0.5, handshake_timeout_s)
        while time.monotonic() < deadline:
            line = ser.readline()
            if not line:
                continue
            text = line.decode("utf-8", errors="ignore").strip()
            if text.startswith(PowerLink.HANDSHAKE_ACK_PREFIX):
                return True
        return False
    finally:
        ser.close()
