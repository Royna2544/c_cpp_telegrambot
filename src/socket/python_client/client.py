"""
Simple Tkinter-based socket client for the TgBot socket API.

Features:
- Connect to the TgBot socket server (TCP, default port 50000)
- Open a session, send a text message command, and close the session
- Visual log output for requests and responses

Requires: cryptography (for AES-GCM)
Install with: pip install cryptography
"""

from __future__ import annotations

import hmac
import json
import os
import random
import socket
import struct
import threading
import time
from dataclasses import dataclass
import errno
from hashlib import sha256
from typing import Any
from tkinter import (
    BooleanVar,
    BOTH,
    DISABLED,
    END,
    NORMAL,
    StringVar,
    Tk,
    messagebox,
    ttk,
)
from tkinter.scrolledtext import ScrolledText

try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
except ImportError as exc:  # pragma: no cover - import guard
    raise SystemExit(
        "The 'cryptography' package is required. Install it with 'pip install cryptography'."
    ) from exc


# Protocol constants (mirrored from src/socket/api)
MAGIC_VALUE_BASE = 0xDEADFACE
DATA_VERSION = 13
MAGIC_VALUE = MAGIC_VALUE_BASE + DATA_VERSION

CMD_WRITE_MSG_TO_CHAT_ID = 1
CMD_GENERIC_ACK = 101
CMD_OPEN_SESSION = 102
CMD_OPEN_SESSION_ACK = 103
CMD_CLOSE_SESSION = 104

PAYLOAD_TYPE_BINARY = 0
PAYLOAD_TYPE_JSON = 1

SESSION_TOKEN_LENGTH = 32
IV_LENGTH = 12
TAG_LENGTH = 16
HMAC_LENGTH = 32
HEADER_SIZE = 80


def _random_nonce() -> int:
    return int(time.time() * 1000) + random.randint(0, 999)


def _recv_exact(sock: socket.socket, size: int) -> bytes:
    buf = bytearray()
    while len(buf) < size:
        chunk = sock.recv(size - len(buf))
        if not chunk:
            raise ConnectionError("Socket connection closed unexpectedly")
        buf.extend(chunk)
    return bytes(buf)


@dataclass
class ParsedPacket:
    cmd: int
    data_type: int
    session_token: bytes
    nonce: int
    payload: bytes


class PacketCodec:
    """Encode/decode TgBot socket packets in Python."""

    _header_struct = struct.Struct(">qiiI32sQ12s8s")

    @classmethod
    def build_packet(
        cls,
        cmd: int,
        payload: bytes,
        payload_type: int,
        session_token: bytes | None,
    ) -> bytes:
        token = (session_token or b"")[:SESSION_TOKEN_LENGTH].ljust(
            SESSION_TOKEN_LENGTH, b"\x00"
        )
        token_present = token != b"\x00" * SESSION_TOKEN_LENGTH

        if payload and token_present:
            iv = os.urandom(IV_LENGTH)
            cipher = AESGCM(token)
            encrypted_payload = cipher.encrypt(iv, payload, None)
        else:
            iv = b"\x00" * IV_LENGTH
            encrypted_payload = payload

        header = cls._header_struct.pack(
            MAGIC_VALUE,
            cmd,
            payload_type,
            len(encrypted_payload),
            token,
            _random_nonce(),
            iv,
            b"\x00" * 8,  # padding to 80 bytes
        )

        if token_present:
            digest = hmac.new(token, header + encrypted_payload, sha256).digest()
        else:
            digest = b"\x00" * HMAC_LENGTH

        return header + encrypted_payload + digest

    @classmethod
    def read_packet(cls, sock: socket.socket) -> ParsedPacket:
        header_bytes = _recv_exact(sock, HEADER_SIZE)
        (
            magic,
            cmd,
            data_type,
            data_size,
            token,
            nonce,
            iv,
            _padding,
        ) = cls._header_struct.unpack(header_bytes)

        if magic != MAGIC_VALUE:
            raise ValueError(f"Unexpected magic value: {hex(magic)}")

        payload = _recv_exact(sock, data_size) if data_size else b""
        hmac_bytes = _recv_exact(sock, HMAC_LENGTH)

        token_present = token != b"\x00" * SESSION_TOKEN_LENGTH
        if token_present:
            expected = hmac.new(token, header_bytes + payload, sha256).digest()
            if hmac_bytes != expected:
                raise ValueError("HMAC verification failed")
        elif hmac_bytes != b"\x00" * HMAC_LENGTH:
            raise ValueError("Unexpected HMAC bytes without a session token")

        if payload and token_present:
            cipher = AESGCM(token)
            payload = cipher.decrypt(iv, payload, None)

        return ParsedPacket(cmd=cmd, data_type=data_type, session_token=token, nonce=nonce, payload=payload)


class SocketClient:
    """Thin synchronous socket client for the Tkinter UI."""

    def __init__(self) -> None:
        self.sock: socket.socket | None = None
        self.session_token: bytes | None = None
        self._host: str | None = None
        self._port: int | None = None
        # Wire format uses big-endian per request.
        self.endian: str = ">"

    def connect(self, host: str, port: int) -> None:
        self.close()
        self.sock = socket.create_connection((host, port), timeout=5)
        self.sock.settimeout(5)
        self._host, self._port = host, port

    def open_session(self) -> dict[str, Any]:
        if not self.sock:
            raise ConnectionError("Not connected")
        packet = PacketCodec.build_packet(CMD_OPEN_SESSION, b"", PAYLOAD_TYPE_BINARY, None)
        self.sock.sendall(packet)
        response = PacketCodec.read_packet(self.sock)
        if response.cmd != CMD_OPEN_SESSION_ACK:
            raise ValueError(f"Unexpected command {response.cmd} while opening session")
        info = json.loads(response.payload.decode("utf-8"))
        self.session_token = response.session_token
        return info

    def send_message(self, chat_id: int, message: str) -> ParsedPacket:
        if not self.sock or not self.session_token:
            raise ConnectionError("Session not established")
        payload = json.dumps({"chat": chat_id, "message": message}, ensure_ascii=False).encode("utf-8")
        packet = PacketCodec.build_packet(
            CMD_WRITE_MSG_TO_CHAT_ID, payload, PAYLOAD_TYPE_JSON, self.session_token
        )
        self.sock.sendall(packet)
        return PacketCodec.read_packet(self.sock)

    def close_session(self) -> None:
        if self.sock and self.session_token:
            packet = PacketCodec.build_packet(
                CMD_CLOSE_SESSION, b"", PAYLOAD_TYPE_BINARY, self.session_token
            )
            self.sock.sendall(packet)
        self.close()

    def close(self) -> None:
        if self.sock:
            try:
                self.sock.shutdown(socket.SHUT_RDWR)
            except OSError as exc:
                if exc.errno not in (errno.ENOTCONN, errno.EBADF, errno.ENOTSOCK):
                    raise
            self.sock.close()
        self.sock = None
        self.session_token = None


class SocketClientUI:
    def __init__(self, root: Tk) -> None:
        self.root = root
        self.root.title("TgBot Socket Client (Python + Tkinter)")

        self.host_var = StringVar(value="127.0.0.1")
        self.port_var = StringVar(value="50000")
        self.chat_var = StringVar(value="")
        self.msg_var = StringVar(value="")
        self.session_active = BooleanVar(value=False)

        self.client = SocketClient()
        self._build_layout()

    # Layout helpers -----------------------------------------------------
    def _build_layout(self) -> None:
        top = ttk.Frame(self.root, padding=10)
        top.pack(fill=BOTH, expand=True)

        conn_frame = ttk.LabelFrame(top, text="Connection")
        conn_frame.pack(fill=BOTH, expand=False, pady=(0, 8))

        ttk.Label(conn_frame, text="Host").grid(row=0, column=0, sticky="w", padx=4, pady=4)
        ttk.Entry(conn_frame, textvariable=self.host_var, width=20).grid(row=0, column=1, padx=4, pady=4)
        ttk.Label(conn_frame, text="Port").grid(row=0, column=2, sticky="w", padx=4, pady=4)
        ttk.Entry(conn_frame, textvariable=self.port_var, width=8).grid(row=0, column=3, padx=4, pady=4)

        self.connect_btn = ttk.Button(conn_frame, text="Open Session", command=self._open_session_async)
        self.connect_btn.grid(row=0, column=4, padx=6, pady=4)

        self.close_btn = ttk.Button(conn_frame, text="Close Session", command=self._close_session_async, state=DISABLED)
        self.close_btn.grid(row=0, column=5, padx=6, pady=4)

        form = ttk.LabelFrame(top, text="Send Message")
        form.pack(fill=BOTH, expand=False, pady=(0, 8))

        ttk.Label(form, text="Chat ID").grid(row=0, column=0, sticky="w", padx=4, pady=4)
        ttk.Entry(form, textvariable=self.chat_var, width=18).grid(row=0, column=1, padx=4, pady=4)

        ttk.Label(form, text="Message").grid(row=1, column=0, sticky="nw", padx=4, pady=4)
        ttk.Entry(form, textvariable=self.msg_var, width=50).grid(row=1, column=1, padx=4, pady=4, sticky="we")

        self.send_btn = ttk.Button(form, text="Send", command=self._send_async, state=DISABLED)
        self.send_btn.grid(row=0, column=2, rowspan=2, padx=6, pady=4)

        log_frame = ttk.LabelFrame(top, text="Log")
        log_frame.pack(fill=BOTH, expand=True)

        self.log_area = ScrolledText(log_frame, wrap="word", height=14, state=DISABLED)
        self.log_area.pack(fill=BOTH, expand=True, padx=4, pady=4)

    # Logging helpers ----------------------------------------------------
    def _log(self, text: str) -> None:
        self.log_area.configure(state=NORMAL)
        self.log_area.insert(END, f"{time.strftime('%H:%M:%S')} | {text}\n")
        self.log_area.see(END)
        self.log_area.configure(state=DISABLED)

    def _set_busy(self, busy: bool) -> None:
        state = DISABLED if busy else NORMAL
        self.connect_btn.configure(state=state if not self.session_active.get() else DISABLED)
        self.send_btn.configure(state=state if self.session_active.get() else DISABLED)
        self.close_btn.configure(state=state if self.session_active.get() else DISABLED)

    # Async wrappers -----------------------------------------------------
    def _open_session_async(self) -> None:
        threading.Thread(target=self._open_session, daemon=True).start()

    def _close_session_async(self) -> None:
        threading.Thread(target=self._close_session, daemon=True).start()

    def _send_async(self) -> None:
        threading.Thread(target=self._send_message, daemon=True).start()

    # Actions ------------------------------------------------------------
    def _open_session(self) -> None:
        self._set_busy(True)
        try:
            host = self.host_var.get().strip()
            port = int(self.port_var.get())
            self.client.connect(host, port)
            info = self.client.open_session()
            self.session_active.set(True)
            self._log(f"Session opened. Expires at {info.get('expiration_time', '')}")
        except (ValueError, ConnectionError, OSError, socket.error) as exc:
            messagebox.showerror("Open Session Failed", str(exc))
            self._log(f"Failed to open session: {exc}")
        finally:
            self._update_controls()
            self._set_busy(False)

    def _close_session(self) -> None:
        self._set_busy(True)
        try:
            self.client.close_session()
            self._log("Session closed")
        except (ValueError, ConnectionError, OSError, socket.error) as exc:
            self._log(f"Close session error: {exc}")
        finally:
            self.session_active.set(False)
            self._update_controls()
            self._set_busy(False)

    def _send_message(self) -> None:
        self._set_busy(True)
        try:
            chat_id = int(self.chat_var.get())
            message = self.msg_var.get()
            response = self.client.send_message(chat_id, message)
            if response.data_type == PAYLOAD_TYPE_JSON:
                payload_text = response.payload.decode("utf-8", errors="replace")
            else:
                payload_text = response.payload.hex()
            self._log(f"Sent message command, got cmd {response.cmd} payload: {payload_text}")
        except (ValueError, ConnectionError, OSError, socket.error) as exc:
            messagebox.showerror("Send Failed", str(exc))
            self._log(f"Send failed: {exc}")
        finally:
            self._set_busy(False)

    def _update_controls(self) -> None:
        if self.session_active.get():
            self.connect_btn.configure(state=DISABLED)
            self.send_btn.configure(state=NORMAL)
            self.close_btn.configure(state=NORMAL)
        else:
            self.connect_btn.configure(state=NORMAL)
            self.send_btn.configure(state=DISABLED)
            self.close_btn.configure(state=DISABLED)


def main() -> None:
    root = Tk()
    SocketClientUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()
