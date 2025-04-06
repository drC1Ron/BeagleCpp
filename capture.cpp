#define bg_open c_bg_open
#define bg_close c_bg_close
#define bg_enable c_bg_enable
#define bg_disable c_bg_disable
#define bg_spi_read c_bg_spi_read
#define bg_spi_configure c_bg_spi_configure
#define bg_samplerate c_bg_samplerate
#define bg_timeout c_bg_timeout
#define bg_latency c_bg_latency
#define bg_target_power c_bg_target_power
#define bg_status_string c_bg_status_string

#include <beagle.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdint>
#include <csignal>
#include <atomic>
#include <string>

constexpr int BYTES_PER_TRANSACTION = 2;
constexpr int DEFAULT_NUM_TRANSACTIONS = 1000000;
constexpr int FLUSH_INTERVAL = 1000; // flush every N transactions

std::atomic<bool> stop_requested(false);

void signalHandler(int signum) {
    stop_requested = true;
}

int16_t bytesToInt16LE(const uint8_t* data) {
    return static_cast<int16_t>(data[0] | (data[1] << 8));
}

int16_t bytesToInt16BE(const uint8_t* data) {
    return static_cast<int16_t>((data[0] << 8) | data[1]);
}

int main(int argc, char* argv[]) {
    int numTransactions = DEFAULT_NUM_TRANSACTIONS;
    const char* filename = "output.csv";

    if (argc >= 2) numTransactions = std::stoi(argv[1]);
    if (argc >= 3) filename = argv[2];

    signal(SIGINT, signalHandler);

    Beagle beagle = bg_open(0);
    if (beagle <= 0) {
        std::cerr << "Failed to open Beagle device.\n";
        return 1;
    }

    bg_samplerate(beagle, 50000); // 50 MHz
    bg_timeout(beagle, 500);
    bg_latency(beagle, 1); // low latency

    bg_spi_configure(beagle, BG_SPI_SS_ACTIVE_LOW, BG_SPI_SCK_SAMPLING_EDGE_RISING, BG_SPI_BITORDER_MSB);
    bg_target_power(beagle, BG_TARGET_POWER_OFF);

    if (bg_enable(beagle, BG_PROTOCOL_SPI) != BG_OK) {
        std::cerr << "Failed to enable SPI capture.\n";
        bg_close(beagle);
        return 1;
    }

    std::ofstream outfile(filename);
    if (!outfile) {
        std::cerr << "Failed to open output file.\n";
        return 1;
    }
    outfile << "Transaction,Timestamp(ns),Value_LE,Value_BE,Status\n";

    int samplerate_khz = bg_samplerate(beagle, 0);
    std::vector<std::string> buffer;
    buffer.reserve(FLUSH_INTERVAL);

    int count = 0;
    uint8_t mosi[BYTES_PER_TRANSACTION];
    uint8_t miso[BYTES_PER_TRANSACTION];

    while (!stop_requested && (count < numTransactions || numTransactions == 0)) {
        u32 status;
        u64 timeSop = 0, duration = 0;
        u32 offset = 0;

        int ret_count = bg_spi_read(
            beagle,
            &status,
            &timeSop,
            &duration,
            &offset,
            BYTES_PER_TRANSACTION,
            mosi,
            BYTES_PER_TRANSACTION,
            miso
        );

        if (ret_count < 0) {
            std::cerr << "Error reading from device.\n";
            break;
        }
        if (ret_count == 0) continue; // no data

        if (ret_count >= BYTES_PER_TRANSACTION) {
            uint64_t time_ns = (timeSop * 1000000ULL) / samplerate_khz;
            int16_t val_le = bytesToInt16LE(mosi);
            int16_t val_be = bytesToInt16BE(mosi);
            std::string status_str = (status == BG_READ_OK) ? "OK" : "ERR";

            buffer.emplace_back(std::to_string(count) + "," + std::to_string(time_ns) + "," +
                                 std::to_string(val_le) + "," + std::to_string(val_be) + "," + status_str);

            if (++count % FLUSH_INTERVAL == 0) {
                for (const auto& line : buffer) outfile << line << "\n";
                outfile.flush();
                buffer.clear();
            }
        }
    }

    for (const auto& line : buffer) outfile << line << "\n";
    outfile.flush();
    outfile.close();
    bg_disable(beagle);
    bg_close(beagle);

    std::cout << "Capture complete.\n";
    return 0;
}
