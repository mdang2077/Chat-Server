# Chat Server

A lightweight HTTP chat server written in C. Supports posting messages, reacting to them, editing them, and viewing the full chat history — all over plain HTTP.

## Requirements
- GCC with C11 support
- macOS or Linux
- `make`

## Build
```bash
make clean && make
```
This produces a `chat-server` binary in the project root.

## Run
```bash
./chat-server <port>
```
If no port is provided, the OS assigns one automatically. The assigned port is printed on startup:
```
Server started on port 8080
```

## API Reference
All requests are plain HTTP GET. Replace `<port>` with the port the server is running on.

### GET `/chats`
Returns all current chats and their reactions as plain text.
```
http://localhost:<port>/chats
```
Response format:
```
[#<id> <timestamp>]      <username>: <message>
      (<user>)  <reaction>
```

### GET `/post?user=<username>&message=<message>`
Creates a new chat message.
```
http://localhost:<port>/post?user=alice&message=hello
```
| Parameter | Max Length | Required |
|-----------|-----------|----------|
| `user`    | 15 chars  | Yes      |
| `message` | 255 chars | Yes      |

Returns 400 if parameters are missing, empty, or exceed limits. Supports up to 100,000 total messages.

### GET `/react?user=<username>&message=<reaction>&id=<id>`
Adds a reaction to an existing message by its ID.
```
http://localhost:<port>/react?user=bob&message=+1&id=3
```
| Parameter  | Max Length    | Required |
|------------|---------------|----------|
| `user`     | 15 chars      | Yes      |
| `message`  | 15 chars      | Yes      |
| `id`       | valid chat ID | Yes      |

Returns 400 if the ID is invalid or any field is missing/out of range. Each message supports up to 100 reactions.

### GET `/edit?id=<id>&message=<message>`
Replaces the content of an existing message.
```
http://localhost:<port>/edit?id=2&message=updated+text
```
| Parameter | Max Length    | Required |
|-----------|---------------|----------|
| `id`      | valid chat ID | Yes      |
| `message` | 255 chars     | Yes      |

Returns 400 if the ID does not exist or the message is empty/too long.

### GET `/reset`
Deletes all messages and reactions, resetting the server to its initial state.
```
http://localhost:<port>/reset
```
Returns 200 OK with an empty body on success.

## Notes
- Usernames and reactions are URL-decoded, so `%20` becomes a space.
- Chat IDs are assigned sequentially starting at 1.
- Timestamps are recorded at the time of posting in local time (`YYYY-MM-DD HH:MM:SS`).