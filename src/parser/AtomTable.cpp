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

#include "AtomTable.hpp"

#include "common/Hash.hpp"
#include "logging/Logger.hpp"

namespace Cial {

    Atom AtomTable::intern(const char *s, const size_t len) {
        if(len == 0) {
            return ATOM_INVALID; // Handle empty string as invalid, adjust if needed
        }

        const uint32_t h = fnv1a(s, len);
        const uint64_t key = static_cast<uint64_t>(h) << 32 | static_cast<uint64_t>(len);

        const auto it = _map.find(key);
        if(it != _map.end()) {
            for(const Atom a : it->second) {
                if(const AtomEntry *e = _atoms[a.v]; memcmp(e->str, s, len) == 0) {
                    return a;
                }
            }
            // Hash collision (same hash and length, different content) - proceed to create new entry
        }

        // Create new atom
        auto *e = new AtomEntry;
        e->hash = h;
        e->length = len;
        e->str = static_cast<char *>(malloc(len + 1));
        if(!e->str) {
            delete e;
            throw std::bad_alloc();
        }

        memcpy(e->str, s, len);
        e->str[len] = '\0';

        const Atom a{ _atoms.size() };
        _atoms.push_back(e);
        if(it == _map.end()) {
            _map[key] = { a };
        } else {
            it->second.push_back(a);
        }
        return a;
    }

    AtomEntry *AtomTable::get(const Atom a) const {
        if(a.v == ATOM_INVALID.v || a.v >= _atoms.size()) {
            CLL_LOG_WARN("Atom str not found in cache");
            return nullptr;
        }
        return _atoms[a.v];
    }
} // namespace Cial