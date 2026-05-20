#pragma once

#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

struct BlotterOrderRow {
    uint64_t clOrdId;
    uint32_t symbolId;
    uint8_t side;
    uint8_t orderType;
    uint32_t qty;
    uint64_t priceMicros;
    uint64_t timestampNs;
    uint64_t latencyNs;
    bool showLatency;
};

class TerminalBlotter {
public:
    explicit TerminalBlotter(std::string title, std::size_t maxRows = 12)
        : title_(std::move(title)), maxRows_(maxRows) {}

    void updateStatus(
        uint64_t processedCount,
        double elapsedSeconds,
        uint64_t throughput,
        uint64_t avgLatencyNs,
        uint64_t minLatencyNs,
        uint64_t maxLatencyNs,
        bool showLatencyStats
    ) {
        processedCount_ = processedCount;
        elapsedSeconds_ = elapsedSeconds;
        throughput_ = throughput;
        avgLatencyNs_ = avgLatencyNs;
        minLatencyNs_ = minLatencyNs;
        maxLatencyNs_ = maxLatencyNs;
        showLatencyStats_ = showLatencyStats;
    }

    void pushRow(const BlotterOrderRow& row) {
        rows_.push_front(row);
        if (rows_.size() > maxRows_) {
            rows_.pop_back();
        }
    }

    void render() const {
        std::cout << "\033[2J\033[H";
        std::cout << "\033[40m\033[97m" << title_ << "\033[0m\n";
        std::cout << "Processed: " << processedCount_
                  << " | Elapsed(s): " << fixed(elapsedSeconds_, 6)
                  << " | Throughput: " << throughput_;

        if (showLatencyStats_) {
            std::cout << " | AvgLat(ns): " << avgLatencyNs_
                      << " | MinLat(ns): " << minLatencyNs_
                      << " | MaxLat(ns): " << maxLatencyNs_;
        }

        std::cout << "\n";
        std::cout << repeat('-', showLatencyStats_ ? 112 : 96) << "\n";
        std::cout << pad("OrderId", 12)
                  << pad("Side", 8)
                  << pad("Type", 8)
                  << pad("Qty", 10)
                  << pad("Symbol", 10)
                  << pad("Price", 16)
                  << pad("TimestampNs", 18);

        if (showLatencyStats_) {
            std::cout << pad("LatencyNs", 14);
        }

        std::cout << "\n";
        std::cout << repeat('-', showLatencyStats_ ? 112 : 96) << "\n";

        for (const BlotterOrderRow& row : rows_) {
            std::cout << colorForSide(row.side)
                      << pad(std::to_string(row.clOrdId), 12)
                      << pad(sideText(row.side), 8)
                      << pad(typeText(row.orderType), 8)
                      << pad(std::to_string(row.qty), 10)
                      << pad(std::to_string(row.symbolId), 10)
                      << pad(std::to_string(row.priceMicros), 16)
                      << pad(std::to_string(row.timestampNs), 18);

            if (showLatencyStats_) {
                std::cout << pad(row.showLatency ? std::to_string(row.latencyNs) : "-", 14);
            }

            std::cout << "\033[0m\n";
        }

        std::cout.flush();
    }

private:
    static std::string pad(const std::string& value, std::size_t width) {
        std::ostringstream out;
        out << std::left << std::setw(static_cast<int>(width)) << value;
        return out.str();
    }

    static std::string fixed(double value, int precision) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision) << value;
        return out.str();
    }

    static std::string repeat(char ch, std::size_t count) {
        return std::string(count, ch);
    }

    static const char* colorForSide(uint8_t side) {
        if (side == 1) {
            return "\033[32m";
        }

        if (side == 2) {
            return "\033[31m";
        }

        return "\033[0m";
    }

    static std::string sideText(uint8_t side) {
        if (side == 1) {
            return "BUY";
        }

        if (side == 2) {
            return "SELL";
        }

        return "UNK";
    }

    static std::string typeText(uint8_t orderType) {
        if (orderType == 1) {
            return "MKT";
        }

        if (orderType == 2) {
            return "LMT";
        }

        return "UNK";
    }

    std::string title_;
    std::size_t maxRows_;
    std::deque<BlotterOrderRow> rows_;
    uint64_t processedCount_ = 0;
    double elapsedSeconds_ = 0.0;
    uint64_t throughput_ = 0;
    uint64_t avgLatencyNs_ = 0;
    uint64_t minLatencyNs_ = 0;
    uint64_t maxLatencyNs_ = 0;
    bool showLatencyStats_ = false;
};
