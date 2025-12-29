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
#include <stack>
#include <unordered_map>
#include <vector>

namespace Cial {

    struct Atom {
        std::uint64_t v{ 0 };

        constexpr bool operator==(const Atom a) const noexcept { return v == a.v; }
    };

    static constexpr Atom ATOM_INVALID{};

    struct AtomEntry {
        std::uint32_t hash;
        std::uint32_t length;
        char *str; // null-terminated
    };

    class AtomTable {
    public:
        explicit AtomTable() {
            _atoms.push_back(nullptr); // Reserve index 0 as invalid
        }

        ~AtomTable() {
            for(size_t i = 1; i < _atoms.size(); ++i) {
                if(_atoms[i]) {
                    free(_atoms[i]->str);
                    delete _atoms[i];
                }
            }
        }

        AtomTable(const AtomTable &) = delete;
        AtomTable &operator=(const AtomTable &) = delete;

        Atom intern(const char *s, size_t len);
        [[nodiscard]] AtomEntry *get(Atom a) const;

    private:
        std::vector<AtomEntry *> _atoms{}; // index = Atom, atoms[0] = nullptr
        std::unordered_map<std::uint64_t, std::vector<Atom>> _map{}; // key to list of Atoms for collision handling
    };

} // namespace Cial

template <>
struct std::hash<Cial::Atom> {
    size_t operator()(const Cial::Atom v) const noexcept { return v.v; }
};
