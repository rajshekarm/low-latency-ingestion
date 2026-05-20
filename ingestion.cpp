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

#include "blotter_view.h"

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

static std::string formatOrder(const OrderMessage& order) {
    return
        "clOrdId=" + std::to_string(order.clOrdId) +
        " accountId=" + std::to_string(order.accountId) +
        " symbolId=" + std::to_string(order.symbolId) +
        " side=" + std::to_string(order.side) +
        " orderType=" + std::to_string(order.orderType) +
        " qty=" + std::to_string(order.qty) +
        " priceMicros=" + std::to_string(order.priceMicros) +
        " timestampNs=" + std::to_string(order.timestampNs);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: ./cpp_ingestion_client <host> <port> [LOG|GUI]\n";
        return 1;
    }

    std::string host = argv[1];
    int port = std::stoi(argv[2]);
    std::string outputMode = argc >= 4 ? argv[3] : "";
    bool logEnabled = outputMode == "LOG";
    bool guiEnabled = outputMode == "GUI";
    TerminalBlotter blotter("LOW LATENCY INGESTION CLIENT");

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

        if (logEnabled) {
            std::cout << (order.side == 1 ? "\033[32m" : "\033[31m")
                      << "RECV " << formatOrder(order)
                      << " latencyNs=" << latencyNs
                      << "\033[0m\n";
        }

        totalLatencyNs += latencyNs;

        if (latencyNs < minLatencyNs) {
            minLatencyNs = latencyNs;
        }

        if (latencyNs > maxLatencyNs) {
            maxLatencyNs = latencyNs;
        }

        if (guiEnabled) {
            auto now = std::chrono::steady_clock::now();
            double seconds = std::chrono::duration<double>(now - start).count();
            uint64_t throughput = seconds > 0.0 ? static_cast<uint64_t>(receivedCount / seconds) : 0;
            uint64_t avgLatency = totalLatencyNs / receivedCount;
            blotter.pushRow({
                order.clOrdId,
                order.symbolId,
                order.side,
                order.orderType,
                order.qty,
                order.priceMicros,
                order.timestampNs,
                latencyNs,
                true
            });
            blotter.updateStatus(
                receivedCount,
                seconds,
                throughput,
                avgLatency,
                minLatencyNs,
                maxLatencyNs,
                true
            );
            blotter.render();
        }
    }

    auto end = std::chrono::steady_clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();
    double ordersPerSecond = receivedCount / seconds;

    if (guiEnabled && receivedCount > 0) {
        blotter.updateStatus(
            receivedCount,
            seconds,
            static_cast<uint64_t>(ordersPerSecond),
            totalLatencyNs / receivedCount,
            minLatencyNs,
            maxLatencyNs,
            true
        );
        blotter.render();
    }

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
