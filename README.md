# Exchange Simulator

This is an exchange simulator server.

## Architecture

The exchange server is built using a multi-threaded architecture to ensure low latency and high throughput. It consists of four main threads communicating via shared memory and lock-free queues.

```mermaid
flowchart TD
    subgraph External
        Trader[Client]
    end

    subgraph Server [Exchange Server]
        Accept[Accept Thread]
        Epoll[Epoll Event Loop Thread]
        SessionManager[(Session Manager)]
        SPSC[SPSC Queue]
        Matching[Matching Engine Thread]
        MarketState[(Market State)]
        GUI[GUI Thread]
    end

    Trader -->|1. Connect & Login| Accept
    Trader -->|2. Send Orders| Epoll
    
    Accept -.->|Register FD| Epoll
    Accept -.->|Register Trader| SessionManager
    
    Epoll -->|Raw Message| SPSC
    
    SPSC -->|Pop Raw Message| Matching
    
    Matching -.->|Update| MarketState
    Matching -->|3. Send Confirms & MD| Trader
    
    MarketState -.->|Read Snapshot| GUI
    GUI -->|Print| Terminal[Terminal Output]
```

### Threads
- **Accept Thread**: Uses a TCP Server to accept incoming client connections, process login requests, and register active sockets with the epoll instance.
- **Epoll Event Loop (Main Thread)**: Asynchronously reads incoming network packets from all connected clients, frames the raw messages, and pushes them into a lock-free SPSC (Single Producer Single Consumer) queue.
- **Matching Engine Thread**: Pops raw messages from the SPSC queue, processes new/cancel/modify order requests using the `MatchingEngine`, updates the `MarketState`, and sends private order confirmations and public market data updates directly back to clients.
- **Terminal GUI Thread**: Periodically wakes up to read a snapshot of the `MarketState` and renders a live terminal dashboard showing the order book and trade information.

### Core Components
- **SPSC Queue**: A lock-free, single-producer, single-consumer queue that safely transfers framed messages from the network reading thread to the matching engine thread with minimal latency.
- **Epoll**: Used for efficient I/O multiplexing, allowing a single thread to handle data from many active client connections concurrently.
- **Matching Engine**: Core business logic that maintains limit order books for multiple symbols and matches incoming buy/sell orders.

## Compilation

To compile the code, open a terminal in the project directory and run:

```sh
mkdir build
cd build
cmake ..
make
```

## Running

After compiling, you can run the server using:

```sh
./build/exchange_server
```

## Using the Client

The project includes an interactive client to connect to the server and send orders.

1. Open a new terminal and run the client:
```sh
./build/client/exchange_client
```

2. When prompted, enter the connection details (IP, Port, Trader ID):
```text
127.0.0.1 9001 1
```

3. Once connected, you can send orders using the following formats:
- **New Order** (`N <internal_id> <symbol_id> <BUY|SELL> <price> <qty>`):
  ```text
  N 1 0 BUY 100 10
  ```
- **Modify Order** (`M <internal_id> <symbol_id> <new_price> <new_qty>`):
  ```text
  M 1 0 105 10
  ```
- **Cancel Order** (`C <internal_id> <symbol_id>`):
  ```text
  C 1 0
  ```
*Note: `symbol_id` is a number from 0 to 4 (e.g., 0=RELIANCE, 1=TCS).*
