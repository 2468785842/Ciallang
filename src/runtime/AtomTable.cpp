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

#include "Runtime.hpp"
#include "common/Hash.hpp"
#include "logging/Logger.hpp"

namespace Cial {

    Atom AtomTable::intern(const String &str) { return intern(nullptr, str.getData(), str.length()); }

    Atom AtomTable::intern(const char *s, const std::uint32_t len) { return intern(nullptr, s, len); }

    Atom AtomTable::intern(Runtime *rt, const char *s, std::uint32_t len) {
        if(len == 0) {
            return ATOM_INVALID; // Handle empty string as invalid, adjust if needed
        }

        const uint32_t h = fnv1a(s, len);
        const uint64_t key = static_cast<uint64_t>(h) << 32 | static_cast<uint64_t>(len);

        const auto it = _map.find(key);
        if(it != _map.end()) {
            for(const Atom a : it->second) {
                if(const AtomEntry *e = _atoms[a.v]; memcmp(e->str->getData(), s, len) == 0) {
                    return a;
                }
            }
            // Hash collision (same hash and length, different content) - proceed to create new entry
        }

        const Atom a{ _atoms.size() };

        // Create new atom
        AtomEntry *e{};
        if(rt) {
            e = rt->create<AtomEntry>(h, len, new String{ s, len });
        } else {
            e = new AtomEntry{ h, len, new String{ s, len } };
            _handleAtoms.push_back(a);
        }
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