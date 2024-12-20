import struct
import zlib
import socket
import os
import hashlib
import logging_cfg
from crypto import encrypt_data, generate_hmac, decrypt_data, verify_hmac
from cryptography.exceptions import InvalidSignature
from enum import IntEnum
from dataclasses import dataclass
from pathlib import Path
from typing import Optional
from json import JSONEncoder, JSONDecoder, JSONDecodeError
import time

logging_cfg.setup_logging()

import logging

logger = logging.getLogger(__name__)

ALIGNMENT = 8
MAX_PATH_SIZE = 256
MAX_MSG_SIZE = 256
TEXT_ENCODING = "utf-8"


# Command enumeration
class Command(IntEnum):
    CMD_INVALID = 0
    CMD_WRITE_MSG_TO_CHAT_ID = 1
    CMD_CTRL_SPAMBLOCK = 2
    CMD_OBSERVE_CHAT_ID = 3
    CMD_SEND_FILE_TO_CHAT_ID = 4
    CMD_OBSERVE_ALL_CHATS = 5
    CMD_GET_UPTIME = 6
    CMD_TRANSFER_FILE = 7
    CMD_TRANSFER_FILE_REQUEST = 8

    # Below are internal commands
    CMD_GET_UPTIME_CALLBACK = 100
    CMD_GENERIC_ACK = 101
    CMD_OPEN_SESSION = 102
    CMD_OPEN_SESSION_ACK = 103
    CMD_CLOSE_SESSION = 104


class PayloadType(IntEnum):
    BINARY = 0
    JSON = 1

# Enums
class FileType(IntEnum):
    TYPE_PHOTO = 0
    TYPE_VIDEO = 1
    TYPE_GIF = 2
    TYPE_DOCUMENT = 3
    TYPE_DICE = 4
    TYPE_STICKER = 5
    TYPE_MAX = 6


class CtrlSpamBlock(IntEnum):
    OFF = 0
    LOGGING_ONLY = 1
    PURGE = 2
    PURGE_AND_MUTE = 3
    MAX = 4


class AckType(IntEnum):
    SUCCESS = 0
    ERROR_TGAPI_EXCEPTION = 1
    ERROR_INVALID_ARGUMENT = 2
    ERROR_COMMAND_IGNORED = 3
    ERROR_RUNTIME_ERROR = 4
    ERROR_CLIENT_ERROR = 5


def align_str(string: str, size: int) -> bytes:
    strbuf = string.encode(TEXT_ENCODING)
    if len(strbuf) > size:
        logger.warning(
            f"Message too long: {len(strbuf)} bytes, truncating to {size} bytes"
        )
        strbuf = strbuf[: size - 1]
    return strbuf.ljust(size, b"\x00")


def manual_pad(count: int) -> bytes:
    return b"\x00" * count


if False:
    orig_unpack = struct.unpack

    def unpack(format: str, data: bytes) -> tuple:
        try:
            return orig_unpack(format, data)
        except struct.error as e:
            logger.error(f"{str(e)}, had {len(data)} bytes")
            raise

    struct.unpack = unpack


@dataclass
class WriteMsgToChatId:
    chat: int  # Chat ID (int64)
    message: str

    SIZE = 264
    FORMAT = f"q{MAX_MSG_SIZE}s"  # 4-byte int + 256-byte string
    assert (
        struct.calcsize(FORMAT) == SIZE
    ), f"Mismatch in size, got {struct.calcsize(FORMAT)}"

    def pack(self):
        return struct.pack(
            self.FORMAT, self.chat, align_str(self.message, MAX_MSG_SIZE)
        )


@dataclass
class ObserveChatId:
    chat: int
    observe: bool

    SIZE = 16
    FORMAT = "q?"
    MANUAL_PADDING = SIZE - struct.calcsize(FORMAT)

    def pack(self):
        return struct.pack(self.FORMAT, self.chat, self.observe) + manual_pad(
            self.MANUAL_PADDING
        )


@dataclass
class SendFileToChatId:
    chat: int
    fileType: int  # Assuming FileType is an enum stored as an integer
    filePath: str

    SIZE = 272
    FORMAT = f"qII{MAX_PATH_SIZE}s"  # 4-byte int + 4-byte int + 256-byte string
    assert (
        struct.calcsize(FORMAT) == SIZE
    ), f"Mismatch in size, got {struct.calcsize(FORMAT)}"

    def pack(self):
        return struct.pack(
            self.FORMAT,
            self.chat,
            self.fileType,
            0,  # Padding
            align_str(self.filePath, MAX_PATH_SIZE),
        )


@dataclass
class ObserveAllChats:
    observe: bool

    SIZE = 8
    FORMAT = "b"  # 4-byte int + 1-byte boolean
    MANUAL_PADDING = SIZE - struct.calcsize(FORMAT)

    def pack(self):
        return struct.pack(self.FORMAT, self.observe) + manual_pad(self.MANUAL_PADDING)


@dataclass
class DeleteControllerById:
    controller_id: int

    SIZE = 8
    FORMAT = "I"  # Single 4-byte integer
    MANUAL_PADDING = SIZE - struct.calcsize(FORMAT)

    def pack(self):
        return struct.pack(self.FORMAT, self.controller_id) + manual_pad(
            self.MANUAL_PADDING
        )


@dataclass
class TransferFileOptions:
    overwrite: bool = False
    hash_ignore: bool = False
    dry_run: bool = False

    FORMAT = "bbb"  # 1-byte booleans


@dataclass
class TransferFile:
    destFilePath: str
    srcFilePath: str
    sha256Hash: bytes
    options: TransferFileOptions

    # Two 256-byte strings + 32-byte hash + options + 5 bytes padding
    FORMAT = f"{MAX_PATH_SIZE}s{MAX_PATH_SIZE}s32s{TransferFileOptions.FORMAT}?I"
    SIZE = 552
    assert (
        struct.calcsize(FORMAT) == SIZE
    ), f"Mismatch in size, got {struct.calcsize(FORMAT)}"

    def pack(self):
        assert (
            len(self.sha256Hash) == 32
        ), f"SHA-256 hash is 32 bytes, got {len(self.sha256Hash)}"
        return struct.pack(
            self.FORMAT,
            align_str(self.destFilePath, MAX_PATH_SIZE),
            align_str(self.srcFilePath, MAX_PATH_SIZE),
            self.sha256Hash,
            self.options.overwrite,
            self.options.hash_ignore,
            self.options.dry_run,
            False,
            0,
        )

    @classmethod
    def unpack(cls, buffer: bytes):
        destFilePath, srcFilePath, sha256Hash, overwrite, hash_ignore, dry_run, _, _ = (
            struct.unpack(cls.FORMAT, buffer)
        )
        return TransferFile(
            destFilePath=destFilePath.decode(TEXT_ENCODING),
            srcFilePath=srcFilePath.decode(TEXT_ENCODING),
            sha256Hash=sha256Hash,
            options=TransferFileOptions(overwrite, hash_ignore, dry_run),
        )


@dataclass
class TransferFileFull(TransferFile):
    buf: bytes
    # FORMAT is inherited from UploadFileDry

    def pack(self):
        return super().pack() + self.buf


@dataclass
class GetUptimeCallback:
    uptime: str

    SIZE = 24
    FORMAT = f"{SIZE}s"  # Fixed 24-byte string


@dataclass
class GenericAck:
    result: int  # Assuming AckType is stored as an integer
    error_msg: Optional[str]
    SIZE = 264
    FORMAT = f"II{MAX_MSG_SIZE}s"  # 4-byte int + 256-byte string
    assert (
        struct.calcsize(FORMAT) == SIZE
    ), f"Mismatch in size, got {struct.calcsize(FORMAT)}"

    def isOk(self):
        return AckType(self.result) == AckType.SUCCESS

    def __str__(self):
        return f"GenericAck(result={AckType(self.result).name}, error_msg='{self.error_msg}')"

    @classmethod
    def unpack(cls, buffer: bytes, buffer_type: PayloadType):
        match buffer_type:
            case PayloadType.BINARY:
                # Unpack the generic ack
                result, _, error_msg = struct.unpack(cls.FORMAT, buffer[: cls.SIZE])

                return cls(result, error_msg.rstrip(b"\0").decode(TEXT_ENCODING))
            case PayloadType.JSON:
                dec = JSONDecoder()
                error_msg = None
                jsonstr = buffer.decode(TEXT_ENCODING)
                try:
                    root = dec.decode(jsonstr)
                except JSONDecodeError as ex:
                    logging.error(f"Decoding JSON {jsonstr}: {str(ex)}")
                    return cls(
                        AckType.ERROR_CLIENT_ERROR, "Cannot decode JSON response"
                    )
                if root["result"] == True:
                    result = AckType.SUCCESS
                else:
                    match root["error_type"]:
                        case "CLIENT_ERROR":
                            result = AckType.ERROR_CLIENT_ERROR
                        case "COMMAND_IGNORED":
                            result = AckType.ERROR_COMMAND_IGNORED
                        case "INVALID_ARGUMENT":
                            result = AckType.ERROR_INVALID_ARGUMENT
                        case "RUNTIME_ERROR":
                            result = AckType.ERROR_RUNTIME_ERROR
                        case "TGAPI_EXCEPTION":
                            result = AckType.ERROR_TGAPI_EXCEPTION
                        case _:
                            raise ValueError(
                                f"Invalid error type: {root['error_type']}"
                            )
                    error_msg = root["error_msg"]
                return cls(result, error_msg)
            case _:
                raise ValueError(f"Invalid buffer type: {buffer_type}")


class Packet:
    MAGIC_VALUE_BASE = 0xDEADFACE
    DATA_VERSION = 12
    MAGIC_VALUE = MAGIC_VALUE_BASE + DATA_VERSION

    @dataclass
    class Header:
        magic: int  # int64
        command: int  # int
        data_type: int  # int
        data_length: int  # uint
        session_token: bytes  # 32 bytes array
        # Padding (int)
        nounce: int  # int64
        hmac: bytes  # 64 bytes array
        init_vector: bytes  # 12 bytes
        # Padding2 (int)

        AES_GCM_TAG_SIZE = 16
        HMAC_LENGTH = 32 # The HmacSha256 actually has a length of 32 bytes
        INIT_VECTOR_LENGTH = 12

        FORMAT = "qIII32sIq64s12sI"
        SIZE = 144
        assert (
            struct.calcsize(FORMAT) == SIZE
        ), f"Mismatch in size, got {struct.calcsize(FORMAT)}"

        @classmethod
        def unpack_from(cls, data: bytearray):
            # Unpack the header
            magic, command, type, length, token, _, nounce, hmac, iv, _ = struct.unpack(
                cls.FORMAT, data[: cls.SIZE]
            )

            return cls(magic, command, type, length, token, nounce, hmac, iv)

        def __str__(self):
            if self.magic - Packet.MAGIC_VALUE_BASE > 0:
                magicstr = f"magic=[version {self.magic - Packet.MAGIC_VALUE_BASE}]"
            else:
                magicstr = f"magic={self.magic}"
            return f"Header({magicstr}, command={Command(self.command).name}, type={PayloadType(self.data_type).name}, length={self.data_length})"

    def __init__(
        self,
        command: Command,
        data: bytes,
        payload_type: PayloadType,
        session_key: Optional[bytes] = None,
    ):
        self.data = data
        if (
            payload_type is PayloadType.BINARY and command != Command.CMD_CTRL_SPAMBLOCK
        ):  # TODO: CPP bindings bug
            assert (
                len(self.data) % ALIGNMENT == 0
            ), f"Mismatched alignment: {len(self.data)}"

        iv = self.update_encryption(session_key)

        self.header = self.Header(
            magic=self.MAGIC_VALUE,
            command=command.value,
            data_type=payload_type,
            data_length=len(self.data),
            session_token=session_key or b"\0" * 32,
            nounce=self.nounce,
            hmac=self.hmac,
            init_vector=iv,
        )
        self.command = command
        logging.debug(
            f"Created Packet: Command: {Command(self.command).name}"
        )

    def update_encryption(self, session_key: Optional[bytes]) -> bytes:
        iv = os.urandom(self.Header.INIT_VECTOR_LENGTH)
        if session_key and len(self.data) > 0:
            logging.debug(f"Encrypting data: {len(self.data)} bytes")
            self.data, tag = encrypt_data(self.data, session_key, iv)
            self.data += tag
            logging.debug(f"Encrypted data: {len(self.data)} bytes, key is {len(tag)} bytes")
            self.hmac = generate_hmac(self.data, session_key)
        else:
            logging.debug("No session key, skipping encryption")
            self.hmac = b"\0" * 64
        self.nounce = int(time.time())
        return iv

    # Pack, with optional size to match
    def pack(self, size: Optional[int]):
        if (
            size
            and self.header.data_type == PayloadType.BINARY
            and len(self.data) < size
        ):
            pad = size - len(self.data)
            logging.info(f"Padding missing bytes: {size} - {len(self.data)} = {pad}")
            self.data += b"\0" * pad
            # Update metadata
            self.header.data_length += pad
            self.update_encryption(self.header.session_token)

        header_packed = struct.pack(
            self.Header.FORMAT,
            self.header.magic,
            self.header.command,
            self.header.data_type,
            self.header.data_length,
            self.header.session_token,
            0,
            self.header.nounce,
            self.header.hmac,
            self.header.init_vector,
            0,
        )
        if len(header_packed) != self.header.SIZE:
            logging.error(f"Invalid header size: {len(header_packed)}")
            return None

        buf = header_packed + self.data
        logger.debug(f"{self.header} size: {self.header.SIZE}, Total size: {len(buf)}")
        return buf


class Sender:
    def __init__(self, ipaddr: str, ipType: socket.AddressFamily, port: Optional[int]):
        self.address = ipaddr
        self.type = ipType
        self.port = port

        match os.name:
            case "nt":
                supp = [
                    socket.AF_INET,
                    socket.AF_INET6,
                ]
            case _:
                supp = [
                    socket.AF_INET,
                    socket.AF_INET6,
                    socket.AF_UNIX,
                ]
        if self.type not in supp:
            raise ValueError("Unsupported socket type: %s" % self.type)
        logger.info(f"Address: {self.address}, Port: {self.port}, Type: {self.type}")

    @staticmethod
    def recv_into_all(sock: socket.socket, buffer: bytearray, size: int):
        if size == 0:
            logger.error("Buffer size is 0")
            return 0

        if len(buffer) < size:
            raise ValueError("Buffer is smaller than the requested size")

        total_received = 0
        while total_received < size:
            received = sock.recv_into(
                memoryview(buffer)[total_received:], size - total_received
            )
            if received == 0:  # Connection closed
                raise ConnectionError(
                    "Socket connection closed before receiving all data"
                )
            total_received += received
        return total_received

    def _send_common(
        self, packet: Packet, size: Optional[int], expect_cmd: Optional[Command]
    ):
        # Send it via socket
        with socket.socket(self.type, socket.SOCK_STREAM) as client:
            logger.debug("Waiting for connect")
            try:
                match self.type:
                    case socket.AF_INET | socket.AF_INET6:
                        assert self.port, "Port cannot be None"
                        client.connect((self.address, self.port))
                    case socket.AF_UNIX:
                        client.connect(self.address)
            except OSError as e:
                logger.error(f"Failed to connect: {e}")
                return None, None

            logger.debug("Connected")

            logger.info(f"Sending packet with command {Command(packet.command).name}")
            pkt = packet.pack(size)
            if not pkt:
                logger.error("Failed to pack packet")
                return None, None

            try:
                client.sendall(pkt)
            except OSError as e:
                logger.error(f"Failed to send packet: {e}")
                return None, None

            if expect_cmd is None:
                return None, None

            buffer = bytearray(Packet.Header.SIZE)
            try:
                self.recv_into_all(client, buffer, Packet.Header.SIZE)
            except OSError as e:
                logger.error(f"Failed to recv header: {e}")
                return None, None

            try:
                p = Packet.Header.unpack_from(buffer)
            except ValueError as e:
                logger.error(e)
                return None, None

            if Command(p.command) != expect_cmd:
                logger.warning(
                    f"Invalid cmd value: Got {Command(p.command).name} but expected {expect_cmd.name}"
                )
                return None, None

            logging.debug(f"Got data length: {p.data_length} bytes")

            if p.data_length == 0:
                return p, bytes()

            buffer = bytearray(p.data_length)
            try:
                self.recv_into_all(client, buffer, p.data_length)
            except OSError as e:
                logger.error(f"Failed to receive data: {e}")
                return None, None

            logging.debug(f"Recieved data")
            buffer = bytes(buffer)

            try:
                verify_hmac(buffer, p.hmac[:Packet.Header.HMAC_LENGTH], p.session_token)
            except InvalidSignature as e:
                logger.error(f"Invalid HMAC: {e}")
                return None, None

            buffer = decrypt_data(
                bytes(buffer[: -Packet.Header.AES_GCM_TAG_SIZE]),
                buffer[-Packet.Header.AES_GCM_TAG_SIZE :],
                p.session_token,
                p.init_vector,
            )

            return p, buffer

    def open_session(self) -> Optional[bytes]:
        packet = Packet(Command.CMD_OPEN_SESSION, b"", PayloadType.BINARY)
        header, buffer = self._send_common(packet, None, Command.CMD_OPEN_SESSION_ACK)
        if header and buffer:
            j = JSONDecoder()
            try:
                root = j.decode(buffer.decode(TEXT_ENCODING))
            except JSONDecodeError as e:
                logger.error(f"Failed to decode JSON: {e}")
                return None
            token: str = root["session_token"]
            logger.debug(f"Payload of OpenSession: {root}")
            logger.info(f'Session key is "{token}"')
            return token.encode(TEXT_ENCODING)
        else:
            logger.warning("Failed to open session")
        return None

    def close_session(self, session_key: bytes):
        packet = Packet(Command.CMD_CLOSE_SESSION, b"", PayloadType.BINARY, session_key)
        self._send_common(packet, None, None)

    @staticmethod
    def session_function(func):
        def wrapper(self, *args, **kwargs):
            session_key = self.open_session()
            if session_key:
                ret = func(self, *args, **kwargs, session_key=session_key)
                self.close_session(session_key)
                return ret
            else:
                logger.error("Failed to open session")
                return None

        return wrapper

    @session_function
    def send_message(
        self, text: str, chat_id: int, session_key: bytes, text_mode: bool = False
    ) -> bool:
        # Create a packet
        if text_mode:
            encoder = JSONEncoder()
            data, type = (
                encoder.encode({"chat": chat_id, "message": text}).encode(
                    TEXT_ENCODING
                ),
                PayloadType.JSON,
            )
        else:
            data, type = (
                WriteMsgToChatId(chat=chat_id, message=text).pack(),
                PayloadType.BINARY,
            )
        packet = Packet(Command.CMD_WRITE_MSG_TO_CHAT_ID, data, type, session_key)
        header, buffer = self._send_common(
            packet, WriteMsgToChatId.SIZE, Command.CMD_GENERIC_ACK
        )
        if header and buffer:
            generic_ack = GenericAck.unpack(
                buffer, buffer_type=PayloadType(header.data_type)
            )
            if generic_ack.isOk():
                logger.info(f"Message sent successfully")
                return True
            else:
                logger.error(f"Error sending message: {generic_ack}")
        else:
            logger.warning("Failed to send packet")
        return False

    @session_function
    def get_uptime(self, session_key: bytes):
        packet = Packet(Command.CMD_GET_UPTIME, b"", PayloadType.BINARY, session_key)
        header, buffer = self._send_common(
            packet, None, Command.CMD_GET_UPTIME_CALLBACK
        )
        if header and buffer:
            uptime_callback = GetUptimeCallback(
                uptime=buffer.rstrip(b"\0").decode(TEXT_ENCODING)
            )
        else:
            logger.warning("Failed to get uptime")
            uptime_callback = None
        return uptime_callback

    @session_function
    def control_spamblock(self, config: CtrlSpamBlock, session_key: bytes) -> bool:
        data = struct.pack("I", config.value)

        # Create a packet
        packet = Packet(
            Command.CMD_CTRL_SPAMBLOCK, data, PayloadType.BINARY, session_key
        )
        header, buffer = self._send_common(packet, 4, Command.CMD_GENERIC_ACK)
        if header and buffer:
            generic_ack = GenericAck.unpack(
                buffer, buffer_type=PayloadType(header.data_type)
            )
            if generic_ack.isOk():
                logger.info("Spamblock control successful")
                return True
            else:
                logger.error(f"Error controlling spamblock: {generic_ack}")
        else:
            logger.warning("Failed to control spamblock")
        return False

    @session_function
    def send_file(
        self, file: str, type: FileType, chat_id: int, session_key: bytes
    ) -> bool:
        # Create a packet
        data = SendFileToChatId(chat=chat_id, fileType=type.value, filePath=file)
        packet = Packet(
            Command.CMD_SEND_FILE_TO_CHAT_ID,
            data.pack(),
            PayloadType.BINARY,
            session_key,
        )
        header, buffer = self._send_common(
            packet, SendFileToChatId.SIZE, Command.CMD_GENERIC_ACK
        )
        if header and buffer:
            generic_ack = GenericAck.unpack(
                buffer, buffer_type=PayloadType(header.data_type)
            )
            if generic_ack.isOk():
                logger.info(f"File sent successfully")
                return True
            else:
                logger.error(f"Error sending file: {generic_ack}")
        else:
            logger.warning("Failed to send packet")
        return False

    @session_function
    def upload_file(
        self,
        filepath: Path,
        destfilepath: Path,
        session_key: bytes,
        overwrite: bool = False,
        hash_ignore: bool = False,
    ):
        if not filepath.exists():
            logger.error(f"File does not exist: {filepath}")
            return False

        # Calculate SHA256 hash
        with open(filepath, "rb") as f:
            file_data = f.read()
            data_hash = hashlib.sha256(file_data).digest()
            logger.info(f"SHA256 hash: {hashlib.sha256(file_data).hexdigest()}")

        # Create dry send packet
        data = TransferFile(
            srcFilePath=str(filepath),
            destFilePath=str(destfilepath),
            sha256Hash=data_hash,
            options=TransferFileOptions(
                overwrite=overwrite,
                hash_ignore=hash_ignore,
                dry_run=True,
            ),
        )
        packet = Packet(
            Command.CMD_TRANSFER_FILE, data.pack(), PayloadType.BINARY, session_key
        )
        header, buffer = self._send_common(packet, None, Command.CMD_GENERIC_ACK)
        if header and buffer:
            upload_file_dry_callback = GenericAck.unpack(buffer, PayloadType.BINARY)
            if upload_file_dry_callback.isOk():
                logger.info("Server said, it is fine to upload")
            else:
                logger.error(f"Server rejected: {upload_file_dry_callback}")
                return False
        else:
            logger.warning("Failed to send packet")
            return False

        # Create upload packet
        data = TransferFileFull(
            srcFilePath=str(filepath),
            destFilePath=str(destfilepath),
            sha256Hash=data_hash,
            options=TransferFileOptions(
                overwrite=overwrite,
                hash_ignore=hash_ignore,
                dry_run=False,
            ),
            buf=file_data,
        )
        packet = Packet(
            Command.CMD_TRANSFER_FILE, data.pack(), PayloadType.BINARY, session_key
        )
        header, buffer = self._send_common(packet, None, Command.CMD_GENERIC_ACK)
        if header and buffer:
            upload_file_callback = GenericAck.unpack(
                buffer, buffer_type=PayloadType(header.data_type)
            )
            if upload_file_callback.isOk():
                logger.info("File uploaded successfully")
            else:
                logger.error(f"Error uploading file: {upload_file_callback}")
        else:
            logger.warning("Failed to send packet")
        return False

    @session_function
    def download_file(
        self, srcfilepath: Path, destfilepath: Path, session_key: bytes
    ) -> bool:
        # Create a packet
        data = TransferFile(
            srcFilePath=str(srcfilepath),
            destFilePath=str(destfilepath),
            sha256Hash=b"",
            options=TransferFileOptions(),
        )
        packet = Packet(
            Command.CMD_TRANSFER_FILE_REQUEST,
            data.pack(),
            PayloadType.BINARY,
            session_key,
        )
        header, buffer = self._send_common(packet, None, Command.CMD_TRANSFER_FILE)
        if header and buffer:
            with open(destfilepath, "wb") as f:
                f.write(buffer[TransferFile.SIZE :])
            logger.info(f"File downloaded successfully: {destfilepath}")
            return True
        return False
