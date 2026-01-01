#include "termihui/client_storage.h"

ClientStorage::ClientStorage(const std::filesystem::path& dbPath)
    : storage(createClientStorage(dbPath.string()))
{
    storage.sync_schema();
}

void ClientStorage::set(const std::string& key, const std::string& value) {
    storage.replace(KeyValue{key, value});
}

std::optional<std::string> ClientStorage::get(const std::string& key) {
    auto result = storage.get_pointer<KeyValue>(key);
    if (result) {
        return result->value;
    }
    return std::nullopt;
}

void ClientStorage::remove(const std::string& key) {
    storage.remove<KeyValue>(key);
}

void ClientStorage::setUInt64(const std::string& key, uint64_t value) {
    set(key, std::to_string(value));
}

std::optional<uint64_t> ClientStorage::getUInt64(const std::string& key) {
    auto str = get(key);
    if (str) {
        try {
            return std::stoull(*str);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

