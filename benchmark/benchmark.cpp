#include "datastore.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::size_t operations = 100000;
    if (argc > 1) {
        operations = static_cast<std::size_t>(std::stoull(argv[1]));
    }

    redisclone::DataStore store;

    const auto started = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < operations; ++i) {
        store.set("key:" + std::to_string(i), "value:" + std::to_string(i));
    }
    for (std::size_t i = 0; i < operations; ++i) {
        (void)store.get("key:" + std::to_string(i));
    }
    const auto finished = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = finished - started;
    const double total_ops = static_cast<double>(operations * 2);

    std::cout << static_cast<std::size_t>(total_ops) << " ops in " << elapsed.count() << " sec\n"
              << static_cast<std::size_t>(total_ops / elapsed.count()) << " ops/sec\n";

    return 0;
}

