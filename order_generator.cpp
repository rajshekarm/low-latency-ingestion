#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <string>

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

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: ./order_generator <host> <port> <order_count>\n";
        return 1;
    }

    std::string host = argv[1];
    int port = std::stoi(argv[2]);
    uint64_t orderCount = std::stoull(argv[3]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &server.sin_addr) <= 0) {
        std::cerr << "Invalid host\n";
        return 1;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
        perror("connect");
        return 1;
    }

    std::cout << "Connected to ingestion service\n";
    std::cout << "OrderMessage size = " << sizeof(OrderMessage) << " bytes\n";

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

        const char* data = reinterpret_cast<const char*>(&order);
        size_t remaining = sizeof(OrderMessage);

        while (remaining > 0) {
            ssize_t sent = send(sock, data, remaining, 0);
            if (sent <= 0) {
                perror("send");
                close(sock);
                return 1;
            }

            data += sent;
            remaining -= sent;
        }
    }

    auto end = std::chrono::steady_clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();
    double ordersPerSecond = orderCount / seconds;

    std::cout << "Sent orders: " << orderCount << "\n";
    std::cout << "Elapsed seconds: " << seconds << "\n";
    std::cout << "Throughput orders/sec: " << static_cast<uint64_t>(ordersPerSecond) << "\n";

    close(sock);
    return 0;
}