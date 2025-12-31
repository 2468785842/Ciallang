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

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Cial {

    struct OctetIdx {
        std::uint64_t v;

        constexpr bool operator==(const OctetIdx b) const noexcept { return v == b.v; }
    };
    static constexpr OctetIdx OCTET_INVALID{};

    struct OctetEntry {
        std::uint32_t hash;
        std::uint32_t size;
        std::uint8_t *data;
    };

    class OctetTable {
    public:
        explicit OctetTable() {
            _octets.push_back(nullptr); // index 0 invalid
        }

        ~OctetTable() {
            for(size_t i = 1; i < _octets.size(); ++i) {
                free(_octets[i]);
                delete _octets[i];
            }
        }

        OctetTable(const OctetTable &) = delete;
        OctetTable &operator=(const OctetTable &) = delete;

        OctetIdx intern(const std::uint8_t *data, std::uint32_t size);

        [[nodiscard]] OctetEntry *get(OctetIdx o) const;

    private:
        std::vector<OctetEntry *> _octets{}; // index = OctetIndex
        std::unordered_map<uint64_t, std::vector<OctetIdx>> _map; // key to list for collisions
    };

} // namespace Cial


template <>
struct std::hash<Cial::OctetIdx> {
    size_t operator()(const Cial::OctetIdx v) const noexcept { return v.v; }
};