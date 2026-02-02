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

#include <unordered_map>
#include <vector>

#include "gc/gc.hpp"
#include "types/String.hpp"
#include "types/Types.hpp"

namespace cial {
    class Runtime;

    struct Atom {
        u64 v{};

        bool operator==(const Atom &) const noexcept = default;
    };

    static constexpr Atom ATOM_INVALID{};

    struct AtomEntry : MarkSweepHeader {
        u32 hash;
        u32 length;
        String *str;

        explicit AtomEntry(const u32 hash, const u32 length, String *str) : hash(hash), length(length), str(str) {}

        ~AtomEntry() noexcept override { delete str; }
    };

    class AtomTable {
    public:
        explicit AtomTable() {
            _atoms.push_back(new AtomEntry{ 0, 0, new String{} }); // Reserve index 0 as invalid
        }

        ~AtomTable() {
            for(auto [v] : _handleAtoms) {
                delete _atoms[v];
            }
        }

        AtomTable(const AtomTable &) = delete;
        AtomTable &operator=(const AtomTable &) = delete;

        AtomTable(AtomTable &&rhs) noexcept :
            _handleAtoms(std::move(rhs._handleAtoms)), _atoms(std::move(rhs._atoms)), _map(std::move(rhs._map)) {}

        AtomTable &operator=(AtomTable &&rhs) noexcept {
            if(this != &rhs) {
                _handleAtoms = std::move(rhs._handleAtoms);
                _atoms = std::move(rhs._atoms);
                _map = std::move(rhs._map);
            }
            return *this;
        }

        Atom intern(const String &str);
        Atom intern(const char *s, u32 len);
        Atom intern(Runtime *rt, const char *s, u32 len);

        [[nodiscard]] AtomEntry *get(Atom a) const;

    private:
        Vec<Atom> _handleAtoms{};
        Vec<AtomEntry *> _atoms{}; // index = Atom, atoms[0] = nullptr
        Map<u64, std::vector<Atom>> _map{}; // key to list of Atoms for collision handling
    };

} // namespace cial

template <>
struct std::hash<cial::Atom> {
    cial::u64 operator()(const cial::Atom v) const noexcept { return std::hash<cial::u64>{}(v.v); }
};
