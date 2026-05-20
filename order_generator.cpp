#include <arpa/inet.h>
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
    uint8_t side;        // 1 = BUY, 2 = SELL
    uint8_t orderType;   // 1 = MARKET, 2 = LIMIT
    uint32_t qty;
    uint64_t priceMicros;
    uint64_t timestampNs;
};
#pragma pack(pop)

static uint64_t nowNs() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();
}

static bool sendAll(int sock, const char* data, size_t size) {
    size_t sentTotal = 0;

    while (sentTotal < size) {
        ssize_t sent = send(sock, data + sentTotal, size - sentTotal, 0);

        if (sent <= 0) {
            perror("send");
            return false;
        }

        sentTotal += static_cast<size_t>(sent);
    }

    return true;
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
        std::cerr << "Usage: ./order_generator <port> <order_count> [LOG|GUI]\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    uint64_t orderCount = std::stoull(argv[2]);
    std::string outputMode = argc >= 4 ? argv[3] : "";
    bool logEnabled = outputMode == "LOG";
    bool guiEnabled = outputMode == "GUI";
    TerminalBlotter blotter("LOW LATENCY ORDER GENERATOR");

    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        perror("socket");
        return 1;
    }

    int reuse = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    //BIND SOCKET TO PORT
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        perror("bind");
        close(serverSock);
        return 1;
    }

    if (listen(serverSock, 1) < 0) {
        perror("listen");
        close(serverSock);
        return 1;
    }

    std::cout << "Order Generator Service started\n";
    std::cout << "Listening on port: " << port << "\n";
    std::cout << "OrderMessage size: " << sizeof(OrderMessage) << " bytes\n";
    std::cout << "Waiting for ingestion client...\n";

    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);

    int clientSock = accept(
        serverSock,
        reinterpret_cast<sockaddr*>(&clientAddr),
        &clientLen
    );

    if (clientSock < 0) {
        perror("accept");
        close(serverSock);
        return 1;
    }

    int flag = 1;
    setsockopt(clientSock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    std::cout << "Ingestion client connected\n";

    auto start = std::chrono::steady_clock::now();

    for (uint64_t i = 1; i <= orderCount; i++) {
        OrderMessage order{};
        order.clOrdId = i;
        order.accountId = 1001;
        order.symbolId = static_cast<uint32_t>((i % 1000) + 1);
        order.side = (i % 2 == 0) ? 1 : 2;
        order.orderType = 2;
        order.qty = 100 + static_cast<uint32_t>(i % 1000);
        order.priceMicros = 125'500'000;
        order.timestampNs = nowNs();

        if (logEnabled) {
            std::cout << (order.side == 1 ? "\033[32m" : "\033[31m")
                      << "SEND " << formatOrder(order)
                      << "\033[0m\n";
        }

        if (guiEnabled) {
            auto now = std::chrono::steady_clock::now();
            double seconds = std::chrono::duration<double>(now - start).count();
            uint64_t throughput = seconds > 0.0 ? static_cast<uint64_t>(i / seconds) : 0;
            blotter.pushRow({
                order.clOrdId,
                order.symbolId,
                order.side,
                order.orderType,
                order.qty,
                order.priceMicros,
                order.timestampNs,
                0,
                false
            });
            blotter.updateStatus(i, seconds, throughput, 0, 0, 0, false);
            blotter.render();
        }

        const char* data = reinterpret_cast<const char*>(&order);

        if (!sendAll(clientSock, data, sizeof(OrderMessage))) {
            std::cerr << "Failed while sending order " << i << "\n";
            close(clientSock);
            close(serverSock);
            return 1;
        }
    }

    auto end = std::chrono::steady_clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();
    double ordersPerSecond = orderCount / seconds;

    if (guiEnabled) {
        blotter.updateStatus(orderCount, seconds, static_cast<uint64_t>(ordersPerSecond), 0, 0, 0, false);
        blotter.render();
    }

    std::cout << "Sent orders: " << orderCount << "\n";
    std::cout << "Elapsed seconds: " << seconds << "\n";
    std::cout << "Throughput orders/sec: " << static_cast<uint64_t>(ordersPerSecond) << "\n";

    close(clientSock);
    close(serverSock);

    return 0;
}
