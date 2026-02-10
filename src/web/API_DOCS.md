# TgBot Web Server API Documentation

**Base URL**: `/api/v1`  
**Content-Type**: `application/json`

---

## **Authentication & Headers**

The following headers are logged and processed for client verification on every request:

* `X-Real-IP`: Client IP address.
* `X-Client-Verify`: Verification token.
* `X-Client-DN`: Distinguished Name.
* `X-Client-Fingerprint`: Client fingerprint.

---

## **1. Messages**

### **Send a Message**
Sends a text message or media file to a specific chat.

**Endpoint:**
`POST /messages`

**Request Body:**

| Field | Type | Required | Description |
| :--- | :--- | :--- | :--- |
| `chat_id` | int64 | **Yes** | The target Telegram chat ID. |
| `text` | string | **Yes** | The content of the message. |
| `file_type` | string | No | One of: `photo`, `video`, `audio`, `document`, `sticker`, `gif`, `dice`. |
| `file_id` | string | No | The Telegram file ID (if sending existing media). |
| `file_path` | string | No | *Unimplemented* (Server path). |
| `file_data` | string | No | *Unimplemented* (Base64 data). |

**Example Request:**
```json
{
  "chat_id": 123456789,
  "text": "Hello world",
  "file_type": "photo",
  "file_id": "AgACAgIA..."
}