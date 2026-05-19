# Low Latency Ingestion

This project is a small benchmark-style prototype for measuring message-ingestion performance over TCP.

It currently contains:

- `order_generator.cpp`: a TCP server that generates and streams packed `OrderMessage` records.
- `ingestion.cpp`: a TCP client that receives those records and reports throughput plus min/avg/max latency.
- `ingestion.java`: a Java placeholder program that currently only prints `Hello, Java 21!`.

## How The Project Works

The C++ flow is built around a fixed binary message:

- `clOrdId`
- `accountId`
- `symbolId`
- `side`
- `orderType`
- `qty`
- `priceMicros`
- `timestampNs`

The generator timestamps each message right before send. The ingestion client reads the binary payload, compares the embedded timestamp against its local receive time, and prints:

- total messages received
- elapsed time
- throughput in orders per second
- average latency in nanoseconds
- minimum latency in nanoseconds
- maximum latency in nanoseconds

## Current Status

### C++

The C++ implementation is functional, but it uses POSIX networking headers such as:

- `arpa/inet.h`
- `sys/socket.h`
- `unistd.h`

That means it is intended for Linux or macOS as written. It does not compile directly with standard Windows `g++` without a Winsock port.

### Java

The Java implementation is not yet a real ingestion client. Right now it is only a starter file:

```java
public class ingestion {
    public static void main(String[] args) {
        System.out.println("Hello, Java 21!");
    }
}
```

So the Java section below explains how to build and run the current file, but it does not yet participate in the TCP benchmark.

## Run With Docker Desktop On Windows

If you are on Windows, Docker Desktop is the easiest way to run the C++ benchmark because the code already targets a Linux-style socket environment.

### 1. Make sure Docker Desktop is running

Check:

```powershell
docker --version
docker compose version
```

### 2. Open a terminal in the project folder

```powershell
cd low-latency-ingestion
```

### 3. Build the Docker image

```powershell
docker compose build
```

This compiles both C++ programs inside a Linux container.

### 4. Run the benchmark

```powershell
docker compose up
```

What happens:

- `order-generator` starts first
- it listens on port `9000`
- `ingestion-client` connects using the Docker service name `order-generator`
- the generator sends `1000000` orders
- both containers print throughput and latency stats

### 5. Stop and clean up

If the containers are still running:

```powershell
docker compose down
```

### 6. Change the test size if needed

The default command sends `1000000` orders. To change that, update the command in `compose.yaml`:

```yaml
command: ["./order_generator", "9000", "500000"]
```

Then rebuild and run again:

```powershell
docker compose up --build
```

## Build And Run: C++

These steps are for Linux or macOS, or for a POSIX-compatible environment where the socket headers are available.

### 1. Open a terminal in the project folder

```bash
cd low-latency-ingestion
```

### 2. Build the order generator

```bash
g++ -std=c++20 -O2 -o order_generator order_generator.cpp
```

### 3. Build the ingestion client

```bash
g++ -std=c++20 -O2 -o cpp_ingestion_client ingestion.cpp
```

### 4. Start the order generator in terminal 1

Example:

```bash
./order_generator 9000 1000000
```

Arguments:

- `9000` = TCP port
- `1000000` = number of orders to send

### 5. Start the ingestion client in terminal 2

```bash
./cpp_ingestion_client 127.0.0.1 9000
```

Arguments:

- `127.0.0.1` = server host
- `9000` = server port

### 6. Review the output

Expected generator output:

- service started
- listening port
- message size
- total sent orders
- elapsed seconds
- throughput

Expected client output:

- connected to order generator
- message size
- total received orders
- elapsed seconds
- throughput
- average/min/max latency

## Build And Run: Java

These steps describe the current Java file exactly as it exists today.

### 1. Install JDK 21 or later

Make sure both commands work:

```bash
javac -version
java -version
```

### 2. Open a terminal in the project folder

```bash
cd low-latency-ingestion
```

### 3. Compile the Java file

```bash
javac ingestion.java
```

### 4. Run the Java program

```bash
java ingestion
```

### 5. Expected output

```text
Hello, Java 21!
```

## Notes

- On this Windows workspace, the C++ sources did not compile because POSIX headers were unavailable.
- On this Windows workspace, Java was not installed, so the Java commands were documented but not executed here.
- Docker Desktop is a good Windows-friendly option because it gives the C++ code a Linux runtime without needing a local port to Winsock.
- If you want, the next useful improvement would be to implement the Java ingestion client so it can connect to `order_generator.cpp` and report the same latency metrics as the C++ version.
