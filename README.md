*This project has been created as part of the 42 curriculum by asalucci, lzorzit.*

# ft_irc - Internet Relay Chat Server

A C++98 Internet Relay Chat (IRC) server fully compliant with RFC requirements, featuring non-blocking I/O multiplexing and socket programming.

---

## 📋 Description

`ft_irc` is an IRC server implemented in C++98 as part of the 42 Network curriculum. The core objective of this project is to build a fully functional, real-time chat server capable of handling multiple concurrent client connections without blocking or relying on multithreading.

The server implements network communication using BSD socket APIs and non-blocking I/O multiplexing with `poll()`. It handles incoming client connections, parses IRC commands, manages client states and channels, and broadcasts messages in real time adhering to IRC network protocol standards.

---

## 🛠️ Instructions

### Requirements
- **Compiler**: `g++` or `clang++` with standard C++98 support (`-std=c++98`).
- **Operating System**: Linux / macOS / Unix-like environment.
- **Build Tools**: GNU `make`.

### Compilation

Clone the repository and build the binary using the provided `Makefile`:

```bash
make
```

#### Available Makefile Rules:
- `make` or `make all`: Compiles the source files and produces the executable `ircserv`.
- `make clean`: Removes all compiled object files (`.o`).
- `make fclean`: Removes object files and the generated executable `ircserv`.
- `make re`: Cleans and recompiles the entire project.

### Execution

Run the server executable by passing a valid port number and a connection password:

```bash
./ircserv <port> <password>
```

#### Example:
```bash
./ircserv 6667 mysecretpassword
```

### Connecting to the Server

#### 1. Using an IRC Client (e.g., Irssi)
```bash
irssi -c 127.0.0.1 -p 6667 -w mysecretpassword
```

#### 2. Using Netcat (NC) for Manual Testing
```bash
nc 127.0.0.1 6667
```
Once connected via `nc`, send the authentication sequence:
```text
PASS mysecretpassword
NICK mynickname
USER myusername 0 * :Real Name
```

---

## ✨ Features & Architecture

### Key Technical Characteristics
- **Single-Threaded Event Loop**: Uses `poll()` for non-blocking I/O multiplexing across all client sockets and the master server socket.
- **Buffer Management**: Partial read/write buffering for network packets with support for both `CRLF` (`\r\n`) and standard `LF` (`\n`) message delimiters.
- **Channel Operations**: Creation, dynamic joining, leaving, operator administration, and automatic destruction of empty channels.

### Supported IRC Commands
- `PASS`: Authenticates client with the server password.
- `NICK`: Sets or updates the user's nickname.
- `USER`: Registers username and real name.
- `JOIN`: Joins or creates a channel.
- `PRIVMSG`: Sends direct private messages to users or channel broadcasts.
- `TOPIC`: Displays or modifies (if you're operator) channel topics.
- `INVITE`: Invites a user to an invite-only channel (operator privileges required).
- `KICK`: Expels a client from a channel (operator privileges required).
- `MODE`: Manages channel modes (`i`, `t`, `k`, `o`, `l`) (operator privileges required).
- `PING / PONG`: Handles server-client connection keepalive checks.

### Supported Channel Modes
- `+i` / `-i`: **Invite-only** channel flag.
- `+t` / `-t`: **Topic restriction** (operators only).
- `+k` / `-k`: **Channel password/key** protection.
- `+o` / `-o`: **Operator privilege** assignment/revocation.
- `+l` / `-l`: **User capacity limit** restriction.

---

## 📚 Resources

### Documentation & Standards
- [RFC 1459 - Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 - Internet Relay Chat: Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [Irssi Documentation & Command Reference](https://irssi.org/documentation/)

### Artificial Intelligence Usage

In accordance with 42 project guidelines, AI tools (such as Large Language Models) were utilized during the project life cycle for the following specific tasks:

1. **RFC Interpretation**: Analyzing and clarifying complex sections of RFC 2812 and RFC 1459 protocols to ensure proper formatting of numeric replies and command parameters.
2. **Architecture Planning**: Assisting in the initial structural design of socket state machines, non-blocking `poll()` event loop strategies, and output queue buffering.
3. **Edge Case Analysis**: Identifying edge cases regarding partial TCP packet delivery, buffer boundary conditions, and socket cleanup on abrupt client disconnections.
4. **Documentation**: Generating initial drafts for project documentation and `README.md` structure.
