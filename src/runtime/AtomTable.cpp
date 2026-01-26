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

namespace cial {

    Atom AtomTable::intern(const String &str) { return intern(nullptr, str.getData(), str.length()); }

    Atom AtomTable::intern(const char *s, const u32 len) { return intern(nullptr, s, len); }

    Atom AtomTable::intern(Runtime *rt, const char *s, u32 len) {
        if(len == 0) {
            return ATOM_INVALID; // Handle empty string as invalid, adjust if needed
        }

        const u32 h = fnv1a(s, len);
        const u64 key = static_cast<u64>(h) << 32 | static_cast<u64>(len);

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
            e = rt->create<AtomEntry>(h, len, new String{ s, len }).get();
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
        if(a.v >= _atoms.size()) {
            CLL_LOG_WARN("Atom str not found in cache");
            return nullptr;
        }
        return _atoms[a.v];
    }
} // namespace cial