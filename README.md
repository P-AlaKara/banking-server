# banking-server

A concurrent connection-oriented TCP banking server written in C, using `fork()` to handle multiple clients simultaneously. Each client is assigned a separate process to handle communication with the client. The parent/master/listening server only accepts connection requests. Supports basic banking commands (open, close, deposit, withdraw, etc.) over a custom protocol. Accounts are persisted to disk between sessions.

## Features

- Concurrent client handling using the `fork()` system call
- File-backed account persistence
- Command parser supporting:
  - `OPEN`, `CLOSE`
  - `DEPOSIT`, `WITHDRAW`
  - `BALANCE`, `STATEMENT`
  - `QUIT`
- Custom application layer protocol 
- Signal handling for zombie process reaping

## Build Instructions

### Prerequisites

- GCC or Clang (or any C compiler)
- Linux environment
- Basic socket programming knowledge helps 

### 1. Clone the Repo

```bash
git clone https://github.com/P-AlaKara/banking-server/
cd banking-server
```

### 2. Compile the server
#### Note: Ensure you have banking.c and bank.h in the same folder as the server. Change the IP address in client.c (line 10) to your own server's IP.

```bash
gcc server.c banking.c -o server
````

### 2. Compile the client 

```bash
gcc client.c -o client
```

## Protocol Format

Each command sent by the client must end with a semicolon (`;`).

### Example Commands

```text
OPEN,John,123456789,savings,2000,4321;
DEPOSIT,ACC1234,4321,1000;
BALANCE,ACC1234,4321;
STATEMENT,ACC1234,4321;
QUIT;
```

> Commands and responses are comma-separated. Case-insensitive. Server responds with either `OK,...;` or `ERROR <code> <message>;`.

## Running It

1. **Start the server**:

```bash
./server
```

2. **Connect the client**:

```bash
./client
```

3. **Send commands** (DO NOT FORGET THE SEMICOLON!!)


## Appendix

* Server saves accounts to `accounts_data.txt`. Don't delete it.
* Signal handler prevents zombie processes. Server doesn't need to be manually reaped.

## Known Limitations

* Each connection forks a process → Not very scalable for 1000+ clients
* No mutexes or IPC between processes → Race conditions can occur when modifying the same account concurrently

## Credits
banking.c was developed by @NajmaMohamed

