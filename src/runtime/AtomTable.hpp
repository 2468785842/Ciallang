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

#include "gc/GC.hpp"
#include "types/String.hpp"

namespace Cial {
    class Runtime;

    struct Atom {
        std::uint64_t v{ 0 };

        constexpr bool operator==(const Atom a) const noexcept { return v == a.v; }
    };

    static constexpr Atom ATOM_INVALID{};

    struct AtomEntry : MarkSweepHeader {
        std::uint32_t hash;
        std::uint32_t length;
        String *str;

        explicit AtomEntry(const std::uint32_t hash, const std::uint32_t length, String *str) :
            hash(hash), length(length), str(str) {}

        ~AtomEntry() noexcept override { delete str; }
    };

    class AtomTable {
    public:
        explicit AtomTable() {
            _atoms.push_back(nullptr); // Reserve index 0 as invalid
        }

        ~AtomTable() {
            for(auto [v] : _handleAtoms) {
                delete _atoms[v];
            }
        }

        AtomTable(const AtomTable &) = delete;
        AtomTable &operator=(const AtomTable &) = delete;

        Atom intern(const String &str);
        Atom intern(const char *s, std::uint32_t len);
        Atom intern(Runtime *rt, const char *s, std::uint32_t len);

        [[nodiscard]] AtomEntry *get(Atom a) const;

    private:
        std::vector<Atom> _handleAtoms{};
        std::vector<AtomEntry *> _atoms{}; // index = Atom, atoms[0] = nullptr
        std::unordered_map<std::uint64_t, std::vector<Atom>> _map{}; // key to list of Atoms for collision handling
    };

} // namespace Cial

template <>
struct std::hash<Cial::Atom> {
    size_t operator()(const Cial::Atom v) const noexcept { return v.v; }
};
