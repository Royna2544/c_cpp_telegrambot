# TgBot Socket Protocol (v12) — GitHub‑Friendly Reference  
*Little‑endian, 8‑byte alignment; AES‑GCM encryption; trailing HMAC‑SHA‑256.*

---

## Legend

| Column | Meaning |
|--------|---------|
| **Dir.** | `C → S` (client → server) · `S → C` (server → client) |
| **Type** | `Json` = UTF‑8 JSON body · `Binary` = packed C++ struct |
| **Notes** | Short description. Full examples are in collapsible blocks below each table. |

---

## 1  Packet Envelope (binary)

| Field | Size | Purpose |
|-------|------|---------|
| `magic` | 8 B | `0xDEADFACE + 12` — protocol & version guard |
| `cmd` | 4 B | Command enum |
| `data_type` | 4 B | 0 = Binary, 1 = Json |
| `data_size` | 4 B | Bytes of *encrypted* payload |
| `session_token` | 32 B | Session key seed |
| `nonce` | 8 B | Anti‑replay counter / timestamp |
| `init_vector` | 12 B | AES‑GCM IV |

> After the encrypted payload comes a **32‑byte HMAC‑SHA‑256** over **Header + Encrypted Payload**.

---

## 2  Session Lifecycle

| Cmd | Dir. | Type | Notes |
|-----|------|------|-------|
| **CMD_OPEN_SESSION** | C → S | Json | Request new session token |
| **CMD_OPEN_SESSION_ACK** | S → C | Json | Returns 32‑byte token |
| **CMD_CLOSE_SESSION** | C → S | Json | Cleanly close session |

<details><summary>Examples</summary>

```jsonc
// CMD_OPEN_SESSION
{ "client": "desktop-v1" }

// CMD_OPEN_SESSION_ACK
{ "session_token": "base64-32bytes" }
```
</details>

---

## 3  Acknowledgement & Time

| Cmd | Dir. | Type | Notes |
|-----|------|------|-------|
| **CMD_GENERIC_ACK** | both | Json | Success / failure wrapper |
| **CMD_GET_UPTIME** | C → S | Json | Ask for uptime |
| **CMD_GET_UPTIME_CALLBACK** | S → C | Json or Binary | Returns uptime data |

<details><summary>Examples</summary>

```jsonc
// CMD_GENERIC_ACK (success)
{ "result": true }

// CMD_GENERIC_ACK (error)
{ "result": false, "error_type": "CLIENT_ERROR", "error_msg": "details…" }

// CMD_GET_UPTIME_CALLBACK (JSON)
{ "uptime": "Uptime: 999h 99m 99s",
  "start_time": 1710000000,
  "current_time": 1710288888 }
```

```cpp
// Binary struct for callback
struct GetUptimeCallback {
    char uptime[24]; // "Uptime: …"
};
```
</details>

---

## 4  Chat Commands

| Cmd | Dir. | Type | Notes |
|-----|------|------|-------|
| **CMD_WRITE_MSG_TO_CHAT_ID** | C → S | Json / Binary | Send text message |
| **CMD_OBSERVE_CHAT_ID** | C → S | Json / Binary | Enable / disable watch on one chat |
| **CMD_OBSERVE_ALL_CHATS** | C → S | Json / Binary | Toggle watch on every chat |

<details><summary>JSON examples</summary>

```jsonc
// Write message
{ "chat": 123456789, "message": "Hello!" }

// Observe single chat
{ "chat": 123456789, "observe": true }

// Observe all chats
{ "observe": false }
```
</details>

---

## 5  Simple File Push

| Cmd | Dir. | Type | Notes |
|-----|------|------|-------|
| **CMD_SEND_FILE_TO_CHAT_ID** | C → S | Json / Binary | Upload media/document in one shot |

<details><summary>JSON example</summary>

```jsonc
{ "chat": 123456789,
  "fileType": 3,               // DOCUMENT
  "filePath": "/tmp/report.pdf" }
```
</details>

---

## 6  Chunked File‑Transfer Suite

| Cmd | Dir. | Type | Notes |
|-----|------|------|-------|
| **CMD_TRANSFER_FILE_BEGIN** | C → S | Json | Announces new upload session |
| **CMD_TRANSFER_FILE** | C → S | Json + Binary | Single chunk (JSON header + 0xFF + raw bytes) |
| **CMD_TRANSFER_FILE_RESULT** | S → C | Json | Per‑chunk ACK / NACK |
| **CMD_TRANSFER_FILE_END** | C → S | Json | Marks completion |

<details><summary>Begin / End payloads</summary>

```jsonc
// CMD_TRANSFER_FILE_BEGIN
{
  "id": "uuid-1234",
  "srcpath": "/home/user/pic.jpg",
  "destpath": "/srv/upload/pic.jpg",
  "filesize": 1048576,
  "chunk_size": 65536,
  "chunks": 16,
  "sha256_full": "0d4f…",
  "options": { "overwrite": false, "hash_ignore": false }
}

// CMD_TRANSFER_FILE_END
{ "id": "uuid-1234", "status": "complete" }
```
</details>

<details><summary>Per‑chunk exchange</summary>

```jsonc
// JSON sub‑header inside CMD_TRANSFER_FILE
{ "id": "uuid-1234", "index": 5, "offset": 327680, "sha256": "ab12…" }
```

After this JSON comes byte `0xFF`, then ≤ `chunk_size` raw file bytes.
</details>

<details><summary>Result example</summary>

```jsonc
{ "id": "uuid-1234",
  "index": 5,
  "result": false,
  "reason": "sha mismatch" }
```
</details>

---

## 7  End‑to‑End Upload Flow

```text
C ▶ CMD_OPEN_SESSION ------------------------------------------▶ S
C ◀ CMD_OPEN_SESSION_ACK --------------------------------------◀ S

C ▶ CMD_TRANSFER_FILE_BEGIN -----------------------------------▶ S
C ◀ CMD_GENERIC_ACK (true) ------------------------------------◀ S

(loop chunks)
    C ▶ CMD_TRANSFER_FILE (#n) --------------------------------▶ S
    C ◀ CMD_TRANSFER_FILE_RESULT (n,true) ----------------------◀ S
(end loop)

C ▶ CMD_TRANSFER_FILE_END -------------------------------------▶ S
C ◀ CMD_GENERIC_ACK (true) ------------------------------------◀ S

C ▶ CMD_CLOSE_SESSION -----------------------------------------▶ S
```

Every payload ↑ is AES‑GCM encrypted; every packet ends with a 32‑byte HMAC that covers **Header + Encrypted Payload**.
