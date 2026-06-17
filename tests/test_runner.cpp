#include "command_executor.h"
#include "command_parser.h"
#include "datastore.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void testParser() {
    const auto command = redisclone::CommandParser::parse("set greeting \"hello world\"");
    expect(command.type == redisclone::CommandType::Set, "parser recognizes SET case-insensitively");
    expect(command.args.size() == 2, "parser keeps quoted value as one arg");
    expect(command.args[0] == "greeting", "parser stores key");
    expect(command.args[1] == "hello world", "parser stores quoted value");

    const auto empty = redisclone::CommandParser::parse("SET empty \"\"");
    expect(empty.args.size() == 2, "parser preserves empty quoted value");
    expect(empty.args[1].empty(), "empty quoted value is empty string");
}

void testBasicCommands() {
    redisclone::DataStore store;
    redisclone::CommandExecutor executor(store, "unused.rdb");

    expect(executor.execute("SET name Aditya").response == "OK", "SET returns OK");
    expect(executor.execute("GET name").response == "Aditya", "GET returns stored value");
    expect(executor.execute("EXISTS name").response == "1", "EXISTS returns 1 for present key");
    expect(executor.execute("DEL name").response == "OK", "DEL returns OK");
    expect(executor.execute("EXISTS name").response == "0", "EXISTS returns 0 after delete");
    expect(executor.execute("GET name").response == "(nil)", "GET returns nil for missing key");
}

void testExpiration() {
    redisclone::DataStore store;
    store.set("token", "abc");
    expect(store.expire("token", std::chrono::seconds(1)), "EXPIRE succeeds for existing key");
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    expect(!store.exists("token"), "expired key is removed");
}

void testSaveLoad() {
    const auto path = std::filesystem::temp_directory_path() / "redisclone_test_dump.rdb";

    {
        redisclone::DataStore store;
        store.set("city", "Mumbai");
        store.set("phrase", "hello world");
        std::string error;
        expect(store.save(path, &error), "save succeeds: " + error);
    }

    {
        redisclone::DataStore store;
        std::string error;
        expect(store.load(path, &error), "load succeeds: " + error);
        expect(store.get("city").value_or("") == "Mumbai", "load restores simple value");
        expect(store.get("phrase").value_or("") == "hello world", "load restores quoted value");
    }

    std::filesystem::remove(path);
}

} // namespace

int main() {
    testParser();
    testBasicCommands();
    testExpiration();
    testSaveLoad();

    if (failures != 0) {
        std::cerr << failures << " test failure(s)\n";
        return 1;
    }

    std::cout << "All RedisClone tests passed\n";
    return 0;
}
