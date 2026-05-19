#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#pragma pack(push, 1)
struct OrderMessage {
    uint64_t clOrdId;
    uint32_t accountId;
    uint32_t symbolId;
    uint8_t side;
    uint8_t orderType;
    uint32_t qty;
    uint64_t priceMicros;
    uint64_t timestampNs;
};
#pragma pack(pop)

static bool recvAll(int sock, char* buffer, size_t size) {
    size_t receivedTotal = 0;

    while (receivedTotal < size) {
        ssize_t received = recv(sock, buffer + receivedTotal, size - receivedTotal, 0);

        if (received == 0) {
            return false; // server closed connection
        }

        if (received < 0) {
            perror("recv");
            return false;
        }

        receivedTotal += static_cast<size_t>(received);
    }

    return true;
}

static uint64_t nowNs() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: ./cpp_ingestion_client <host> <port>\n";
        return 1;
    }

    std::string host = argv[1];
    int port = std::stoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    std::string portString = std::to_string(port);

    int lookupStatus = getaddrinfo(host.c_str(), portString.c_str(), &hints, &result);
    if (lookupStatus != 0) {
        std::cerr << "Host lookup failed: " << gai_strerror(lookupStatus) << "\n";
        close(sock);
        return 1;
    }

    bool connected = false;
    for (addrinfo* addr = result; addr != nullptr; addr = addr->ai_next) {
        if (connect(sock, addr->ai_addr, static_cast<socklen_t>(addr->ai_addrlen)) == 0) {
            connected = true;
            break;
        }
    }

    freeaddrinfo(result);

    if (!connected) {
        perror("connect");
        close(sock);
        return 1;
    }

    std::cout << "Connected to order generator\n";
    std::cout << "OrderMessage size: " << sizeof(OrderMessage) << " bytes\n";

    uint64_t receivedCount = 0;
    uint64_t totalLatencyNs = 0;
    uint64_t minLatencyNs = UINT64_MAX;
    uint64_t maxLatencyNs = 0;

    auto start = std::chrono::steady_clock::now();

    while (true) {
        OrderMessage order{};

        bool ok = recvAll(sock, reinterpret_cast<char*>(&order), sizeof(OrderMessage));

        if (!ok) {
            break;
        }

        receivedCount++;

        uint64_t receiveTimeNs = nowNs();
        uint64_t latencyNs = receiveTimeNs - order.timestampNs;

        totalLatencyNs += latencyNs;

        if (latencyNs < minLatencyNs) {
            minLatencyNs = latencyNs;
        }

        if (latencyNs > maxLatencyNs) {
            maxLatencyNs = latencyNs;
        }
    }

    auto end = std::chrono::steady_clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();
    double ordersPerSecond = receivedCount / seconds;

    std::cout << "Received orders: " << receivedCount << "\n";
    std::cout << "Elapsed seconds: " << seconds << "\n";
    std::cout << "Throughput orders/sec: " << static_cast<uint64_t>(ordersPerSecond) << "\n";

    if (receivedCount > 0) {
        std::cout << "Avg latency ns: " << (totalLatencyNs / receivedCount) << "\n";
        std::cout << "Min latency ns: " << minLatencyNs << "\n";
        std::cout << "Max latency ns: " << maxLatencyNs << "\n";
    }

    close(sock);
    return 0;
}
