/*
 * Copyright (c) 2024/5/6 下午8:16
 *
 * /\  _` \   __          /\_ \  /\_ \
 * \ \ \/\_\ /\_\     __  \//\ \ \//\ \      __      ___      __
 *  \ \ \/_/_\/\ \  /'__`\  \ \ \  \ \ \   /'__`\  /' _ `\  /'_ `\
 *   \ \ \L\ \\ \ \/\ \L\.\_ \_\ \_ \_\ \_/\ \L\.\_/\ \/\ \/\ \L\ \
 *    \ \____/ \ \_\ \__/.\_\/\____\/\____\ \__/.\_\ \_\ \_\ \____ \
 *     \/___/   \/_/\/__/\/_/\/____/\/____/\/__/\/_/\/_/\/_/\/___L\ \
 *                                                            /\____/
 *                                                            \_/__/
 *
 */

#include "OctetTable.hpp"

#include "common/Hash.hpp"

namespace cial {

    OctetIdx OctetTable::intern(const std::uint8_t *data, const std::uint32_t size) {
        if(size == 0)
            return OCTET_INVALID;

        const uint32_t h = fnv1a(reinterpret_cast<const char *>(data), size); // 用字节计算哈希
        const uint64_t key = (static_cast<uint64_t>(h) << 32) | static_cast<uint64_t>(size);

        const auto it = _map.find(key);
        if(it != _map.end()) {
            for(const OctetIdx idx : it->second) {
                if(const OctetEntry *o = _octets[idx.v]; memcmp(o->data, data, size) == 0) {
                    return idx; // 内容匹配
                }
            }
        }

        // 创建新 Octet
        auto *o = new OctetEntry; // 使用你的 Octet 构造函数
        o->hash = h;
        o->size = size;
        o->data = static_cast<std::uint8_t *>(malloc(size));
        if(!o->data) {
            delete o;
            throw std::bad_alloc();
        }

        memcpy(o->data, data, sizeof(OctetEntry));
        const OctetIdx idx{ static_cast<std::uint32_t>(_octets.size()) };
        _octets.push_back(o);

        if(it == _map.end()) {
            _map[key] = { idx };
        } else {
            it->second.push_back(idx);
        }
        return idx;
    }

    OctetEntry *OctetTable::get(const OctetIdx o) const {
        if(o.v == OCTET_INVALID.v || o.v >= _octets.size()) {
            return nullptr;
        }
        return _octets[o.v];
    }
} // namespace cial