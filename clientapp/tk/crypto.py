from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import hashes, hmac
from cryptography.hazmat.backends import default_backend

# Encrypt the header with AES-GCM
def encrypt_data(header: bytes, key: bytes, iv: bytes) -> tuple[bytes, bytes]:
    cipher = Cipher(algorithms.AES(key), modes.GCM(iv), backend=default_backend())
    encryptor = cipher.encryptor()
    ciphertext = encryptor.update(header) + encryptor.finalize()
    return ciphertext, encryptor.tag

# Generate HMAC for the encrypted header
def generate_hmac(data: bytes, key: bytes) -> bytes:
    h = hmac.HMAC(key, hashes.SHA256(), backend=default_backend())
    h.update(data)
    return h.finalize()

# Decrypt the header with AES-GCM
def decrypt_data(ciphertext: bytes, tag: bytes, key: bytes, iv: bytes) -> bytes:
    cipher = Cipher(algorithms.AES(key), modes.GCM(iv, tag), backend=default_backend())
    decryptor = cipher.decryptor()
    return decryptor.update(ciphertext) + decryptor.finalize()

# Verify HMAC for integrity
def verify_hmac(data: bytes, received_hmac: bytes, key: bytes):
    h = hmac.HMAC(key, hashes.SHA256(), backend=default_backend())
    h.update(data)
    h.verify(received_hmac)  # Will raise an InvalidSignature exception if HMAC doesn't match
