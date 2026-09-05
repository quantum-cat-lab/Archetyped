#include "SchemaFactory.h"
#include <SDK/archetyped/hash/hash.h>
#include <cstdio>

SchemaFactory& SchemaFactory::instance() {
    static SchemaFactory inst;
    return inst;
}

void SchemaFactory::registerSchema(std::string_view domain, std::string_view name, const std::vector<std::pair<uint32_t, std::string>>& fields) {
    std::lock_guard<std::mutex> lk(m_);
    uint32_t domainHash = fnv1aHash(domain);
    auto it = registries_.find(domainHash);
    if (it == registries_.end()) {
        auto reg = std::make_unique<SchemaInstance>();
        reg->registerSchema(name, fields);
        registries_.emplace(domainHash, std::move(reg));
    } else {
        it->second->registerSchema(name, fields);
    }
}

const SchemaDef* SchemaFactory::getSchema(std::string_view domain, uint32_t hash) const {
    std::lock_guard<std::mutex> lk(m_);
    uint32_t domainHash = fnv1aHash(domain);
    auto it = registries_.find(domainHash);
    if (it == registries_.end()) return nullptr;
    return it->second->getSchema(hash);
}

const SchemaDef* SchemaFactory::getSchema(std::string_view domain, std::string_view name) const {
    return getSchema(domain, fnv1aHash(name));
}

uint32_t SchemaFactory::getSchemaFieldOffset(std::string_view domain, uint32_t schemaHash, uint64_t fieldHash) const {
    std::lock_guard<std::mutex> lk(m_);
    uint32_t domainHash = fnv1aHash(domain);
    auto it = registries_.find(domainHash);
    if (it == registries_.end()) return 0;
    return it->second->getSchemaFieldOffset(schemaHash, fieldHash);
}

std::vector<uint8_t> SchemaFactory::dumpSchema(std::string_view domain) const {
    std::lock_guard<std::mutex> lk(m_);
    uint32_t domainHash = fnv1aHash(domain);
    auto it = registries_.find(domainHash);
    if (it == registries_.end()) return {};
    return it->second->dumpSchema();
}

std::vector<std::vector<uint8_t>> SchemaFactory::dumpAllSchemas() const {
    std::lock_guard<std::mutex> lk(m_);
    std::vector<std::vector<uint8_t>> out;
    out.reserve(registries_.size());
    for (const auto& [domainHash, registry] : registries_) {
        (void)domainHash;
        out.push_back(registry->dumpSchema());
    }
    return out;
}

void SchemaFactory::compose(std::string_view targetDomain, std::string_view importDomain) {
    uint32_t th = fnv1aHash(targetDomain);
    uint32_t ih = fnv1aHash(importDomain);
    if (th == ih) return;
    std::lock_guard<std::mutex> lk(m_);
    auto tit = registries_.find(th);
    auto iit = registries_.find(ih);
    if (tit == registries_.end() || iit == registries_.end()) return;
    tit->second->mergeFrom(*iit->second);
}

bool SchemaFactory::hasDomain(std::string_view domain) const {
    std::lock_guard<std::mutex> lk(m_);
    return registries_.find(fnv1aHash(domain)) != registries_.end();
}

void SchemaFactory::invalidateDomain(std::string_view domain) {
    std::lock_guard<std::mutex> lk(m_);
    uint32_t domainHash = fnv1aHash(domain);
    registries_.erase(domainHash);
}

void SchemaFactory::invalidateAll() {
    std::lock_guard<std::mutex> lk(m_);
    registries_.clear();
}