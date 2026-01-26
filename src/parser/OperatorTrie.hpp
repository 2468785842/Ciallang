/*
 * Copyright (c) 2026/1/13.
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

#include <array>

#include "Token.hpp"
#include "types/String.hpp"
#include "types/Types.hpp"

namespace cial::Syntax {

    struct OpTrieNode {
        TokenType token = TokenType::Invalid;
        std::array<int, 128> next{};
    };

    class OperatorTrie {
        std::vector<OpTrieNode> _opTrie{};
        int _opTrieRoot = -1;

    public:
        explicit OperatorTrie(std::initializer_list<std::pair<String, TokenType>> list) {
            _opTrie.emplace_back();
            _opTrie.back().token = TokenType::Invalid;
            _opTrie.back().next.fill(-1);
            _opTrieRoot = 0;

            for(auto &[str, type] : list) {
                insert(str, type);
            }
        }

        void insert(const String &op, const TokenType type) {
            int node = _opTrieRoot;

            for(const u8 b : op) {
                assert(b < 0x80);
                int &next = _opTrie[node].next[b];
                if(next == -1) {
                    next = static_cast<int>(_opTrie.size());
                    _opTrie.emplace_back();
                    _opTrie.back().token = TokenType::Invalid;
                    _opTrie.back().next.fill(-1);
                }
                node = next;
            }

            _opTrie[node].token = type;
        }

        [[nodiscard]] int getRoot() const { return _opTrieRoot; }

        [[nodiscard]] const OpTrieNode &getNode(const size_t nodeIndex) const { return _opTrie[nodeIndex]; }
    };
} // namespace cial::Syntax