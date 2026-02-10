# C++ Bot API V2 Documentation

**Base URL:** `/api/v1`
**Version:** 2.0.0
**Protocol:** HTTP/1.1 (RESTful)

---

## 1. System Status

### Get System Statistics
Returns the bot's current uptime, status, and identity.

- **Endpoint:** `/stats`
- **Method:** `GET`
- **Success Response:** `200 OK`

**Response Body:**
```json
{
  "status": true,
  "uptime": {
    "days": 12,
    "hours": 4,
    "minutes": 32
  },
  "username": "MyCppBot",
  "user_id": 123456789
}
```

---

## 2. Hardware Monitor

### Get Hardware Usage
Returns real-time CPU, RAM, Disk usage, and OS information of the host machine.

- **Endpoint:** `/hardware`
- **Method:** `GET`
- **Success Response:** `200 OK`

**Response Body:**
```json
{
  "success": true,
  "cpu": {
    "usage_percent": 12.5,
    "core_count": 4,
    "name": "ARMv7 Processor rev 4 (v7l)"
  },
  "memory": {
    "total_mbytes": 976,
    "used_mbytes": 450
  },
  "disk": {
    "total_gbytes": 32,
    "used_gbytes": 10
  },
  "os": {
    "name": "Linux",
    "version": "5.10.160-legacy",
    "hostname": "nanopineo",
    "uptime_seconds": 1054320
  }
}
```

---

## 3. Messaging

### Send Message
Sends a text message or a file attachment to a specific chat.

- **Endpoint:** `/messages`
- **Method:** `POST`
- **Content-Type:** `application/json`

**Body Parameters:**

| Parameter | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| `chat_id` | `int64` | Yes | Target Chat ID. |
| `text` | `string` | No | Message caption or text content. |
| `file_type` | `string` | No | `photo`, `video`, `audio`, `document`, `sticker`, `gif`, `dice`. |
| `file_path` | `string` | No | Local server path (e.g., `/var/www/img.png`). |
| `file_id` | `string` | No | Existing Telegram File ID. |

**Example Request:**
```json
{
  "chat_id": -100123456789,
  "text": "Check out this image!",
  "file_type": "photo",
  "file_path": "/var/www/uploads/image.jpg"
}
```

---

## 4. Chat Manager

### Map Chat ID to Name
Create or update a human-readable alias for a numeric Chat ID.

- **Endpoint:** `/chats/{chat_id}`
- **Method:** `PUT`

**Request Body:**
```json
{
  "chat_name": "MyGroupAlias"
}
```

### Delete Chat Alias
Remove the alias mapping for a Chat ID.

- **Endpoint:** `/chats/{chat_id}`
- **Method:** `DELETE`

### Resolve Chat Name
Find a Chat ID using its alias.

- **Endpoint:** `/chats`
- **Method:** `GET`
- **Query Param:** `?chat_name={alias}`

**Response:**
```json
{
  "success": true,
  "chat_id": -100123456789
}
```

---

## 5. Media Manager

### Map Media ID to Alias
Map a Telegram File ID to a human-readable alias (e.g., "welcome_gif").

- **Endpoint:** `/media/{media_id}`
- **Method:** `PUT`

**Body Parameters:**

| Parameter | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| `alias` | `string[]` | Yes | List of alias strings to assign. |
| `media_type` | `string` | Yes | The type of media (photo, video, etc.). |
| `media_unique_id` | `string` | No | Telegram's unique file identifier (for deduplication). |

**Example Request:**
```json
{
  "media_type": "sticker",
  "alias": ["welcome", "hello_sticker"],
  "media_unique_id": "AQADHgAD..."
}
```

### Delete Media Alias
Remove the alias mapping for a File ID.

- **Endpoint:** `/media/{media_id}`
- **Method:** `DELETE`

### Resolve Media Alias
Find a File ID using its alias.

- **Endpoint:** `/media`
- **Method:** `GET`
- **Query Param:** `?alias={alias_name}`

**Response:**
```json
{
  "success": true,
  "media_id": ["CAACAgIAAxkBAA...", "CAACAgIAAxkBAB..."]
}
```

---

## 6. Voting

### Cast Vote
Simple up/down voting mechanism for bot feedback.

- **Endpoint:** `/votes`
- **Method:** `POST`

**Request Body:**
```json
{
  "votes": "up"  // or "down"
}
```