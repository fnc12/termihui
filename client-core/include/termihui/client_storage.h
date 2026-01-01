#pragma once

#include <sqlite_orm/sqlite_orm.h>
#include <string>
#include <optional>
#include <filesystem>

struct KeyValue {
    std::string key;
    std::string value;
};

inline auto createClientStorage(std::string path) {
    using namespace sqlite_orm;
    return make_storage(std::move(path),
        make_table("key_value",
            make_column("key", &KeyValue::key, primary_key()),
            make_column("value", &KeyValue::value)
        )
    );
}

using ClientStorageType = decltype(createClientStorage(""));

class ClientStorage {
public:
    explicit ClientStorage(const std::filesystem::path& dbPath);
    
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    void remove(const std::string& key);
    
    // Convenience methods for common types
    void setUInt64(const std::string& key, uint64_t value);
    std::optional<uint64_t> getUInt64(const std::string& key);

private:
    ClientStorageType storage;
};

