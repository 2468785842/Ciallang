// Copyright (c) 2026/11/11
//
// 文档词法索引（tools/lsp/cial_lsp.py 的 DefinitionIndex C++ 移植，核心集）：
// 掩码字面量/注释后按词法扫描建立保守绑定索引——括号平衡校验、函数/类/属性/
// catch/for/with 作用域、参数与 var/const 声明、可见性与遮蔽解析、保守类型推导。
// 含 @ 预处理指令或括号不平衡的源码整体拒绝（宁缺毋错）。
// API 事实不经此文件：内置类型名与成员一律来自声明索引（doc/lsp.md）。
#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cial::lsp {
    // 隐藏字面量与注释体，保留每个偏移与换行（词法扫描与括号校验共用）。
    std::string maskedSource(std::string_view text);

    // 标识符字节判定（ASCII 词字符 + $_ + 高字节）；Server 的前缀扫描复用。
    bool isIdentifierByte(unsigned char ch);

    struct Position {
        int line{};
        int character{};
    };

    struct Range {
        Position start;
        Position end;
    };

    // 行偏移索引：字节偏移 ↔ LSP 行/UTF-16 列（诊断、声明、定义共用）。
    class LineIndex {
    public:
        explicit LineIndex(std::string_view text) : _text(text) {
            _lineStarts.push_back(0);
            for(size_t i = 0; i < _text.size(); ++i)
                if(_text[i] == '\n')
                    _lineStarts.push_back(i + 1);
        }
        Range tokenRange(size_t start, size_t end) const; // UTF-8 字节半开区间
        Position positionAt(size_t offset) const;
        // LSP 位置 → 字节偏移（超界返回 npos）
        [[nodiscard]] size_t offsetAt(Position position) const;
        [[nodiscard]] const std::string &text() const noexcept { return _text; }

    private:
        std::string _text;
        std::vector<size_t> _lineStarts;
    };

    struct DocumentDeclaration {
        std::string name;
        std::string keyword; // function/var/const/class/property/namespace
        size_t nameStart{};
        size_t nameEnd{};
        Range range; // 名称范围的 LSP 行列
    };

    // 文档符号与普通补全用的词法声明列表（掩码后正则扫描）。
    std::vector<DocumentDeclaration> documentDeclarations(const LineIndex &lines);

    class DocumentIndex {
    public:
        enum class ScopeKind { Root, Block, Function, Catch, For, With, Class, Property };

        // externalType/externalReturn：本文件解析不到时向声明索引查询
        // （变量值类型 / owner 点分路径的返回类型），返回空串表示未知。
        DocumentIndex(const std::string &text, std::function<std::string(const std::string &)> externalType = {},
                      std::function<std::string(const std::string &)> externalReturn = {});

        [[nodiscard]] bool valid() const noexcept { return _valid; }

        struct Binding {
            size_t token{}; // 声明名 token 下标
            size_t visible{}; // 自该字节偏移起可见
            bool isClass{};
            bool isProperty{};
            bool hasBody{};
            size_t bodyStart{}, bodyEnd{}; // token 下标（函数体）
            std::string annotation;
            struct Init {
                size_t equals{}, first{}, terminator{};
            };
            std::optional<Init> init;
        };

        struct ResolveResult {
            const Binding *binding{}; // 命中的绑定；null = 未解析到
            bool free{}; // 自由全局标识符（名字在 freeName）
            std::string freeName;
        };

        // 光标 LSP 位置 → 绑定/自由标识符；动态上下文（.x、字典键、with、
        // 根作用域下的动态链）一律未解析。
        [[nodiscard]] ResolveResult resolve(Position position) const;
        [[nodiscard]] ResolveResult resolveToken(size_t cursor) const;

        // `receiver.` / `receiver.word` 补全形态：返回接收者 token 下标与已输入前缀。
        [[nodiscard]] std::optional<std::pair<int, std::string>> memberContext(Position position) const;

        // 接收者标识符的类型名（内置类型或声明中的 class/namespace 名），未知为空。
        [[nodiscard]] std::string inferReceiverType(int receiverToken) const;

        // 绑定声明名的字节范围（跳转定义定位用）。
        [[nodiscard]] std::pair<size_t, size_t> tokenSpan(size_t tokenIndex) const;

    private:
        struct Token {
            std::string value;
            size_t start{};
            size_t end{};
        };
        struct Scope {
            size_t start{}, end{};
            ScopeKind kind{ ScopeKind::Block };
            int parent{ -1 };
            std::map<std::string, std::vector<Binding>> bindings;
        };

        void build();
        bool identifier(size_t index) const;
        [[nodiscard]] size_t pairAt(size_t tokenIndex) const; // 开括号配对，无配对返回 npos
        std::vector<std::pair<size_t, std::optional<Binding::Init>>> declarators(size_t cursor, size_t end) const;
        int scopeAt(size_t offset) const;
        Binding &addBinding(int scope, size_t tokenIndex, size_t visible);
        std::string inferFromBinding(const Binding &binding, int depth) const;
        std::string inferExpression(const Binding::Init &init, int depth) const;
        std::string functionReturnType(const Binding &binding, int depth) const;
        bool spend() const;
        int tokenCursor(Position position) const;

        std::string _text;
        std::function<std::string(const std::string &)> _externalType;
        std::function<std::string(const std::string &)> _externalReturn;
        std::vector<Token> _tokens;
        std::vector<Scope> _scopes;
        std::map<size_t, size_t> _pairs; // 开括号 token → 闭括号 token
        std::map<size_t, size_t> _reversePairs;
        std::map<size_t, const Binding *> _bindingAt; // 声明名起始偏移 → 绑定
        std::map<size_t, int> _tokenScope; // token 下标 → 最内层作用域
        bool _valid{};
        mutable int _budget{};
    };
} // namespace cial::lsp
