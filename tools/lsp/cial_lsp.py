#!/usr/bin/env python3
"""Dependency-free stdio LSP server backed by Ciallang's compile-only checker."""
import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit
from urllib.request import url2pathname

KEYWORDS = {
    "function": "定义函数：function name(parameters) { ... }",
    "var": "声明变量：var name = value;",
    "const": "声明带初值的变量；当前 TJS2 兼容语义仍允许后续赋值。",
    "class": "定义类：class Name extends Base { ... }",
    "property": "定义具有 getter / setter 的属性。",
    "try": "try { ... } catch(e) { ... }",
    "with": "with(object) { .member; }",
    "return": "返回函数结果。",
    "if": "if(condition) statement; else statement;",
    "while": "while(condition) statement;",
    "for": "for(init; condition; step) statement;",
    "switch": "switch(value) { case value: ...; break; default: ...; }",
    "new": "创建类或内置容器实例。",
    "throw": "抛出可由 catch 捕获的脚本异常。",
    "this": "当前调用上下文。",
    "super": "父类上下文。",
    "global": "全局对象。",
    "void": "空值。",
    "true": "逻辑真（整数 1）。",
    "false": "逻辑假（整数 0）。",
    "Array": "数组；支持字面量 []、下标、push、sort 等。",
    "Dictionary": "字典；支持字面量 %[]。",
    "String": "字符串与字符串原生方法。",
    "Math": "数学方法、常量与随机数。",
    "Date": "日期构造、解析及分量方法。",
    "RegExp": "正则表达式，支持 /pattern/flags 字面量。",
    "Exception": "异常对象，包含 message 成员。",
    "Scripts": "宿主脚本存储：execStorage 执行脚本文件，evalStorage 求值脚本文件（src/stdlib/ScriptsLib.cpp）。",
}
DECLARATION = re.compile(r"\b(function|var|const|class|property)\s+([\w$]+)")
WORD = re.compile(r"[\w$]+")
SYMBOL_KINDS = {"function": 12, "class": 5, "property": 7}
MAX_DOCUMENT_BYTES = 2_000_000
MAX_WORKSPACE_BYTES = 20_000_000
MAX_WORKSPACE_FILES = 2000
MAX_WORKSPACE_DIRECTORIES = 10000
MAX_WORKSPACE_SYMBOLS = 200
EXCLUDED_DIRECTORIES = {".git", ".idea", ".vscode", ".venv", ".cache", "build", "bin", "ext", "node_modules", "__pycache__"}
MAX_INFER_DEPTH = 3          # 推导递归深度（函数返回值/变量拷贝链跨绑定层数）
MAX_INFER_STEPS = 20_000     # 单次请求推理总步数预算，超限一律回退 unknown
MAX_CLASS_HIERARCHY = 8      # extends 链展开深度上限
MAX_RENAME_EDITS = 5_000     # 单次 rename 产生的编辑总数上限

# 内置类型成员名单：逐一核对自 src/stdlib 的注册代码（勿凭记忆增删）：
#   Array      src/stdlib/ArrayLib.cpp:17-35 + src/stdlib/SerializeLib.cpp:28-33
#   String     src/stdlib/StringLib.cpp:26-42
#   Dictionary src/stdlib/DictLib.cpp:12-17 + src/stdlib/SerializeLib.cpp:34-38
#   Math       src/stdlib/MathLib.cpp:18-44
#   Date       src/stdlib/DateLib.cpp:17-33
#   RegExp     src/stdlib/RegExpLib.cpp:13-18
#   RandomGen  src/stdlib/RandomLib.cpp:209-214（当前无全局构造入口，仅备查）
#   Exception  src/stdlib/ExceptionLib.hpp:15-21（TJS 自举类，message 成员）
BUILTIN_TYPE_NAMES = {"Array", "String", "Dictionary", "Math", "Date", "RegExp", "Exception", "Scripts"}
MEMBER_METHOD, MEMBER_FIELD, MEMBER_PROPERTY = 2, 5, 10  # LSP CompletionItemKind
BUILTIN_MEMBERS = {
    "Array": {
        "push": (MEMBER_METHOD, "push(value, ...): Integer — 末尾追加元素，返回新 count"),
        "pop": (MEMBER_METHOD, "pop(): 元素 — 移除并返回末尾元素"),
        "unshift": (MEMBER_METHOD, "unshift(value, ...): Integer — 头部插入元素"),
        "shift": (MEMBER_METHOD, "shift(): 元素 — 移除并返回头部元素"),
        "insert": (MEMBER_METHOD, "insert(index, value, ...): Integer — 在 index 处插入"),
        "remove": (MEMBER_METHOD, "remove(index): 元素 — 移除指定位置元素"),
        "erase": (MEMBER_METHOD, "erase() — 清空所有元素"),
        "reverse": (MEMBER_METHOD, "reverse() — 反转元素顺序"),
        "join": (MEMBER_METHOD, 'join(delim = ","): String — 以分隔符连接元素'),
        "split": (MEMBER_METHOD, "split(pattern, string, purgeempty = false): Array — 用 pattern 拆分 string 并替换自身元素"),
        "sort": (MEMBER_METHOD, "sort(order = '+'): Array — 排序（'+'/'-'/比较函数）"),
        "load": (MEMBER_METHOD, "load(filename) — 反序列化读取"),
        "save": (MEMBER_METHOD, "save(filename) — 序列化保存"),
        "loadStruct": (MEMBER_METHOD, "loadStruct(storage) — 从结构化数据恢复"),
        "saveStruct": (MEMBER_METHOD, "saveStruct(storage = void): String — 结构化导出"),
        "pack": (MEMBER_METHOD, "pack(): Octet — 打包为 Octet"),
        "assignStruct": (MEMBER_METHOD, "assignStruct(source) — 从源结构赋值"),
        "count": (MEMBER_PROPERTY, "count: Integer — 元素个数（只读）"),
        "length": (MEMBER_PROPERTY, "length: Integer — 元素个数（只读）"),
    },
    "String": {
        "charAt": (MEMBER_METHOD, "charAt(index): String — 取指定位置字符"),
        "indexOf": (MEMBER_METHOD, "indexOf(subStr, start = 0): Integer — 子串位置，未找到返回 -1"),
        "toUpperCase": (MEMBER_METHOD, "toUpperCase(): String — 转大写"),
        "toLowerCase": (MEMBER_METHOD, "toLowerCase(): String — 转小写"),
        "substring": (MEMBER_METHOD, "substring(start, length = void): String — 取子串"),
        "substr": (MEMBER_METHOD, "substr(start, length = void): String — substring 的别名"),
        "sprintf": (MEMBER_METHOD, "sprintf(...): String — 格式化自身"),
        "replace": (MEMBER_METHOD, "replace(re, replacement): String — 用正则 re 替换"),
        "split": (MEMBER_METHOD, "split(pattern, reserved = void, purgeempty = false): Array — 按模式拆分"),
        "escape": (MEMBER_METHOD, "escape(): String — 转义反斜杠"),
        "trim": (MEMBER_METHOD, "trim(): String — 去除首尾空白"),
        "reverse": (MEMBER_METHOD, "reverse(): String — 反转字符串"),
        "repeat": (MEMBER_METHOD, "repeat(count): String — 重复 count 次"),
        "length": (MEMBER_PROPERTY, "length: Integer — 字符长度（只读）"),
    },
    "Dictionary": {
        "assign": (MEMBER_METHOD, "assign(source) — 复制源字典的全部成员"),
        "clear": (MEMBER_METHOD, "clear() — 清空成员"),
        "erase": (MEMBER_METHOD, "erase(key): Integer — 删除键，返回是否删除"),
        "load": (MEMBER_METHOD, "load(filename) — 反序列化读取（当前为空实现）"),
        "save": (MEMBER_METHOD, "save(filename) — 序列化保存（当前为空实现）"),
        "loadStruct": (MEMBER_METHOD, "loadStruct(storage) — 从结构化数据恢复"),
        "saveStruct": (MEMBER_METHOD, "saveStruct(storage = void): String — 结构化导出"),
        "assignStruct": (MEMBER_METHOD, "assignStruct(source) — 从源结构赋值"),
        "count": (MEMBER_PROPERTY, "count: Integer — 成员个数（只读）"),
    },
    "Math": {
        "abs": (MEMBER_METHOD, "abs(v) — 绝对值"),
        "acos": (MEMBER_METHOD, "acos(v): Real — 反余弦"),
        "asin": (MEMBER_METHOD, "asin(v): Real — 反正弦"),
        "atan": (MEMBER_METHOD, "atan(v): Real — 反正切"),
        "atan2": (MEMBER_METHOD, "atan2(y, x): Real — 二参数反正切"),
        "ceil": (MEMBER_METHOD, "ceil(v): Integer — 向上取整"),
        "exp": (MEMBER_METHOD, "exp(v): Real — e 的幂"),
        "floor": (MEMBER_METHOD, "floor(v): Integer — 向下取整"),
        "log": (MEMBER_METHOD, "log(v): Real — 自然对数"),
        "pow": (MEMBER_METHOD, "pow(base, exp): Real — 幂"),
        "max": (MEMBER_METHOD, "max(...): Real — 最大值"),
        "min": (MEMBER_METHOD, "min(...): Real — 最小值"),
        "random": (MEMBER_METHOD, "random(): Real — [0,1) 随机数"),
        "round": (MEMBER_METHOD, "round(v): Integer — 四舍五入"),
        "sin": (MEMBER_METHOD, "sin(v): Real — 正弦"),
        "cos": (MEMBER_METHOD, "cos(v): Real — 余弦"),
        "sqrt": (MEMBER_METHOD, "sqrt(v): Real — 平方根"),
        "tan": (MEMBER_METHOD, "tan(v): Real — 正切"),
        "E": (MEMBER_PROPERTY, "E: Real — 自然对数底"),
        "LOG2E": (MEMBER_PROPERTY, "LOG2E: Real — log2(e)"),
        "LOG10E": (MEMBER_PROPERTY, "LOG10E: Real — log10(e)"),
        "LN10": (MEMBER_PROPERTY, "LN10: Real — ln(10)"),
        "LN2": (MEMBER_PROPERTY, "LN2: Real — ln(2)"),
        "PI": (MEMBER_PROPERTY, "PI: Real — 圆周率"),
        "SQRT1_2": (MEMBER_PROPERTY, "SQRT1_2: Real — 1/√2"),
        "SQRT2": (MEMBER_PROPERTY, "SQRT2: Real — √2"),
    },
    "Scripts": {
        "execStorage": (MEMBER_METHOD, "execStorage(name): 执行存储中的脚本文件（相对当前脚本目录，全局上下文）"),
        "evalStorage": (MEMBER_METHOD, "evalStorage(name): 值 — 求值存储中的表达式并返回结果"),
    },
    "Date": {
        "getYear": (MEMBER_METHOD, "getYear(): Integer — 年"),
        "getMonth": (MEMBER_METHOD, "getMonth(): Integer — 月（0 起）"),
        "getDate": (MEMBER_METHOD, "getDate(): Integer — 日"),
        "getDay": (MEMBER_METHOD, "getDay(): Integer — 星期"),
        "getHours": (MEMBER_METHOD, "getHours(): Integer — 时"),
        "getMinutes": (MEMBER_METHOD, "getMinutes(): Integer — 分"),
        "getSeconds": (MEMBER_METHOD, "getSeconds(): Integer — 秒"),
        "getTime": (MEMBER_METHOD, "getTime(): Integer — 毫秒时间戳"),
        "setYear": (MEMBER_METHOD, "setYear(year) — 设置年"),
        "setMonth": (MEMBER_METHOD, "setMonth(month) — 设置月"),
        "setDate": (MEMBER_METHOD, "setDate(date) — 设置日"),
        "setHours": (MEMBER_METHOD, "setHours(hours) — 设置时"),
        "setMinutes": (MEMBER_METHOD, "setMinutes(minutes) — 设置分"),
        "setSeconds": (MEMBER_METHOD, "setSeconds(seconds) — 设置秒"),
        "setTime": (MEMBER_METHOD, "setTime(ms) — 设置毫秒时间戳"),
        "parse": (MEMBER_METHOD, "parse(text): Integer — 解析日期文本"),
        "getTimezoneOffset": (MEMBER_METHOD, "getTimezoneOffset(): Integer — 时区偏移（分钟）"),
    },
    "RegExp": {
        "test": (MEMBER_METHOD, "test(target): Integer — 是否匹配"),
        "match": (MEMBER_METHOD, "match(target): Array — 全局匹配结果"),
        "exec": (MEMBER_METHOD, "exec(target): Array — 单次匹配与分组"),
        "compile": (MEMBER_METHOD, "compile(pattern, flags) — 重新编译正则"),
        "replace": (MEMBER_METHOD, "replace(target, replacement): String — 正则替换"),
        "split": (MEMBER_METHOD, "split(target, purgeempty = false): Array — 按正则拆分"),
    },
    "Exception": {
        "message": (MEMBER_FIELD, "message: String — 异常消息"),
    },
    "RandomGen": {
        "randomize": (MEMBER_METHOD, "randomize(seed = void) — 重置种子"),
        "random": (MEMBER_METHOD, "random(): Real — [0,1) 随机数"),
        "random32": (MEMBER_METHOD, "random32(): Integer — 32 位随机整数"),
        "random63": (MEMBER_METHOD, "random63(): Integer — 63 位随机整数"),
        "random64": (MEMBER_METHOD, "random64(): Integer — 64 位随机整数"),
        "serialize": (MEMBER_METHOD, "serialize(): String — 序列化状态"),
    },
}


def file_path(uri):
    parsed = urlsplit(uri)
    if parsed.scheme != "file" or parsed.query or parsed.fragment:
        return None
    # VSCode 把 Windows 盘符冒号编码为 %3A；url2pathname 在解码前
    # 识别盘符，直接传入会得到非绝对路径。只还原盘符，避免文件名二次解码。
    value = re.sub(r"^/([A-Za-z])%3[aA](?=/)", r"/\1:", parsed.path)
    if parsed.netloc and parsed.netloc != "localhost":
        value = "//" + parsed.netloc + value
    path = pathlib.Path(url2pathname(value))
    if not path.is_absolute():
        return None
    try:
        return path.resolve()
    except (OSError, RuntimeError):
        return None


def masked_source(text):
    """Hide literals/comments while preserving every source offset and line break."""
    masked = list(text)
    index, expression = 0, True
    while index < len(text):
        start = index
        character = text[index]
        if character.isspace():
            index += 1
            continue
        if text.startswith("//", index):
            index = text.find("\n", index)
            if index < 0:
                index = len(text)
        elif text.startswith("/*", index):
            end = text.find("*/", index + 2)
            index = len(text) if end < 0 else end + 2
        elif character in "\"'" or (character == "/" and expression):
            quote, in_class = character, False
            index += 1
            while index < len(text):
                current = text[index]
                index += 1
                if current == "\\":
                    index = min(index + 1, len(text))
                elif quote == "/" and current == "[":
                    in_class = True
                elif quote == "/" and current == "]":
                    in_class = False
                elif current == quote and not in_class:
                    break
            expression = False
        else:
            word = WORD.match(text, index)
            if word:
                index = word.end()
                expression = word[0] in {"return", "throw", "case", "delete", "typeof", "in"}
            else:
                expression = character not in ")]}."
                index += 1
            continue
        for offset in range(start, index):
            if masked[offset] != "\n":
                masked[offset] = " "
    return "".join(masked)


def declarations(text):
    """Lexical declarations, with literal/comment bodies hidden but offsets preserved."""
    source = masked_source(text)
    symbols = []
    offset, row, column = 0, 0, 0
    for match in DECLARATION.finditer(source):
        before = text[offset:match.start(2)]
        if "\n" in before:
            row += before.count("\n")
            column = utf16(before.rsplit("\n", 1)[-1])
        else:
            column += utf16(before)
        offset = match.start(2)
        span = {"start": {"line": row, "character": column},
                "end": {"line": row, "character": column + utf16(match[2])}}
        symbols.append({"name": match[2], "kind": SYMBOL_KINDS.get(match[1], 13),
                        "range": span, "selectionRange": span})
    return symbols


def utf16(text):
    return len(text.encode("utf-16-le")) // 2


def source_position(text, line, byte_column):
    """Native diagnostics use UTF-8 byte columns; LSP positions use UTF-16."""
    lines = text.split("\n")
    line = min(max(0, line), len(lines) - 1)
    prefix = lines[line].encode("utf-8")[:max(0, byte_column)].decode("utf-8", errors="ignore")
    return {"line": line, "character": utf16(prefix)}


def line_prefix(text, position):
    lines = text.split("\n")
    line = lines[min(max(position.get("line", 0), 0), len(lines) - 1)]
    units = max(0, position.get("character", 0))
    prefix, used = "", 0
    for character in line:
        width = 2 if ord(character) > 0xFFFF else 1
        if used + width > units:
            break
        prefix += character
        used += width
    return prefix


class LexicalScope:
    def __init__(self, start, end, kind="block"):
        self.start, self.end, self.kind = start, end, kind
        self.parent = None
        self.bindings = {}


class DefinitionIndex:
    """Conservative binding index for balanced, non-preprocessed source."""
    def __init__(self, text):
        self.text = text
        self.source = masked_source(text)
        self.tokens = []
        self.scopes = []
        self.declared = {}
        self.class_decls = {}     # class 名 token 下标 -> (类体 `{` 下标, extends 名下标|None)
        self.token_scope = {}     # token 下标 -> 最内层 LexicalScope（批量解析用）
        self._reverse_pairs = None
        self._infer_budget = MAX_INFER_STEPS
        self.valid = "@" not in self.source
        if not self.valid:
            return
        for token in re.finditer(r"[\w$]+|[^\s]", self.source):
            self.tokens.append(token)
            if len(self.tokens) > 100_000:
                self.valid = False
                return
        tokens = self.tokens
        self.pairs, stack = {}, []
        for index, token in enumerate(tokens):
            value = token[0]
            if value in ("(", "[", "{"):
                stack.append(index)
            elif value in (")", "]", "}"):
                if not stack or tokens[stack[-1]][0] != {")": "(", "]": "[", "}": "{"}[value]:
                    self.valid = False
                    return
                opening = stack.pop()
                self.pairs[opening] = index
        if stack:
            self.valid = False
            return

        self.root = LexicalScope(-1, len(text) + 1, "root")
        braces = {index: LexicalScope(token.start(), tokens[self.pairs[index]].end())
                  for index, token in enumerate(tokens) if token[0] == "{"}
        self.scopes = [self.root, *braces.values()]
        functions, parameters = [], []
        for index, token in enumerate(tokens):
            value = token[0]
            if value in ("function", "getter", "setter", "catch"):
                cursor, name = index + 1, None
                if value == "function" and self.identifier(cursor):
                    name, cursor = cursor, cursor + 1
                if cursor >= len(tokens) or tokens[cursor][0] != "(":
                    continue
                closing = self.pairs[cursor]
                body = closing + 1
                if body + 1 < len(tokens) and tokens[body][0] == ":":
                    body += 2  # Optional TJS return type annotation.
                if body not in braces:
                    continue
                scope = LexicalScope(tokens[cursor].start(), braces[body].end,
                                     "catch" if value == "catch" else "function")
                self.scopes.append(scope)
                parameters.append((scope, cursor + 1, closing))
                if name is not None and (index == 0 or tokens[index - 1][0] in (";", "{", "}")):
                    functions.append((name, scope, body))
            elif value in ("class", "property") and self.identifier(index + 1):
                cursor = index + 2
                extends = None
                while cursor < len(tokens) and tokens[cursor][0] not in ("{", ";", "}"):
                    if value == "class" and tokens[cursor][0] == "extends" and self.identifier(cursor + 1):
                        extends = cursor + 1
                    cursor = self.pairs.get(cursor, cursor) + 1
                if cursor in braces:
                    braces[cursor].kind = value
                    if value == "class":
                        self.class_decls[index + 1] = (cursor, extends)
            elif value in ("for", "with") and index + 1 < len(tokens) and tokens[index + 1][0] == "(":
                closing = self.pairs[index + 1]
                body = closing + 1
                if body >= len(tokens):
                    continue
                end = self.pairs.get(body, body) if tokens[body][0] == "{" else body
                if tokens[body][0] != "{":
                    while end < len(tokens) and tokens[end][0] != ";":
                        if tokens[end][0] in ("if", "for", "while", "with", "try", "do"):
                            self.valid = False
                            return  # Unbraced compound statements need the real parser.
                        end = self.pairs.get(end, end) + 1
                if end < len(tokens):
                    self.scopes.append(LexicalScope(token.start(), tokens[end].end(), value))

        self.scopes.sort(key=lambda scope: (scope.start, -scope.end))
        parents = []
        for scope in self.scopes:
            while parents and not (parents[-1].start <= scope.start and scope.end <= parents[-1].end):
                parents.pop()
            scope.parent = parents[-1] if parents else None
            parents.append(scope)

        # One sweep assigns every token its innermost scope (bulk resolve/rename use).
        next_scope = 0
        for index, token in enumerate(tokens):
            offset = token.start()
            while next_scope < len(self.scopes) and self.scopes[next_scope].start <= offset:
                parents.append(self.scopes[next_scope])
                next_scope += 1
            while parents and not (parents[-1].start <= offset < parents[-1].end):
                parents.pop()
            if parents:
                self.token_scope[index] = parents[-1]

        for scope, start, end in parameters:
            for name, init in self.declarators(start, end):
                self.bind(scope, name, scope.start, init=init)
        for name, body, opening in functions:
            scope = body.parent or self.root
            self.bind(scope, name, scope.start, body=(opening + 1, self.pairs[opening]))
        for index, token in enumerate(tokens):
            if token[0] in ("var", "const") and (index == 0 or tokens[index - 1][0] != "."):
                for name, init in self.declarators(index + 1, len(tokens)):
                    scope = self.scope_at(tokens[name].start())
                    if scope.kind == "property":
                        continue  # property 体成员仍不参与绑定
                    visible = tokens[min(init[2], len(tokens) - 1)].start() if init else tokens[name].end()
                    self.bind(scope, name, visible, init=init)
            elif token[0] in ("class", "property") and self.identifier(index + 1):
                scope = self.scope_at(token.start())
                if scope.kind in ("root", "class"):
                    binding = self.bind(scope, index + 1, scope.start,
                                        is_property=token[0] == "property")
                    info = self.class_decls.get(index + 1)
                    if info is not None:
                        binding["is_class"] = True
                        binding["body_scope"] = braces[info[0]]
                        binding["extends"] = tokens[info[1]][0] if info[1] is not None else None

    def identifier(self, index):
        return index < len(self.tokens) and (self.tokens[index][0][0].isalpha() or self.tokens[index][0][0] in "_$")

    def declarators(self, cursor, end):
        tokens = self.tokens
        while cursor < end and self.identifier(cursor):
            name, cursor = cursor, cursor + 1
            if cursor + 1 < end and tokens[cursor][0] == ":":
                cursor += 2
            initialized = cursor < end and tokens[cursor][0] == "="
            init = None
            if initialized:
                equals, cursor = cursor, cursor + 1
                while cursor < end and tokens[cursor][0] not in (",", ";", ")"):
                    cursor = self.pairs.get(cursor, cursor) + 1
                init = (equals, equals + 1, cursor)  # (=, 首个表达式 token, 终止符 token)
            elif cursor < end and tokens[cursor][0] == "*":
                cursor += 1
            yield name, init
            if cursor >= end or tokens[cursor][0] != ",":
                break
            cursor += 1

    def scope_at(self, offset):
        return next(scope for scope in reversed(self.scopes) if scope.start <= offset < scope.end)

    def bind(self, scope, token_index, visible, **extra):
        token = self.tokens[token_index]
        binding = {"token": token, "visible": visible, "scope": scope}
        binding.update(extra)
        scope.bindings.setdefault(token[0], []).append(binding)
        self.declared[token.start()] = binding
        return binding

    def token_range(self, cursor):
        return self.token_range_at(self.tokens[cursor])

    def token_range_at(self, token):
        before = self.text[:token.start()]
        start = {"line": before.count("\n"), "character": utf16(before.rsplit("\n", 1)[-1])}
        return {"start": start, "end": {"line": start["line"], "character": start["character"] + utf16(token[0])}}

    def location(self, binding, uri):
        return {"uri": uri, "range": self.token_range_at(binding["token"])}

    def token_cursor(self, position):
        lines = self.text.split("\n")
        row, column = position.get("line", -1), position.get("character", -1)
        if not 0 <= row < len(lines) or not 0 <= column <= utf16(lines[row]):
            return None
        offset = sum(len(line) + 1 for line in lines[:row]) + len(line_prefix(self.text, position))
        return next((index for index, token in enumerate(self.tokens)
                     if token.start() <= offset < token.end()), None)

    def resolve(self, position):
        """Return (binding, unresolved_global_name); ambiguous contexts return neither."""
        if not self.valid:
            return None, None
        cursor = self.token_cursor(position)
        if cursor is None or not self.identifier(cursor):
            return None, None
        return self.resolve_token(cursor)

    def resolve_token(self, cursor):
        """Same contract as resolve(), at token granularity (bulk resolve/rename use)."""
        token = self.tokens[cursor]
        if cursor and self.tokens[cursor - 1][0] == ".":
            return None, None
        if token.start() in self.declared:
            return self.declared[token.start()], None
        if token[0] in set(KEYWORDS) - BUILTIN_TYPE_NAMES:
            return None, None
        if cursor + 1 < len(self.tokens) and self.tokens[cursor + 1][0] == ":":
            return None, None  # Dictionary keys and labels are not bare references.
        scope = self.token_scope.get(cursor) or self.scope_at(token.start())
        dynamic = False
        while scope:
            if scope.kind == "with":
                return None, None
            dynamic |= scope.kind in ("class", "property")
            if scope.kind == "root" and dynamic:
                return None, None
            matches = [binding for binding in scope.bindings.get(token[0], [])
                       if scope.kind == "root" or binding["visible"] <= token.start()]
            if matches:
                binding = matches[-1]
                if scope.kind == "root" and len(matches) != 1:
                    return None, None
                return binding, None
            if scope.kind == "function":
                while scope.parent and scope.parent.kind != "root":
                    scope = scope.parent
                    dynamic |= scope.kind in ("class", "property", "with")
            scope = scope.parent
        return None, (None if dynamic else token[0])

    # ---- 保守类型推导（非流敏感：声明与调用点确定性推导，宁可 unknown）----

    def _spend(self):
        self._infer_budget -= 1
        return self._infer_budget >= 0

    def infer_type(self, position):
        """推导入口：光标处标识符的类型名（内置类型或工作区 class 名），未知返回 None。"""
        if not self.valid:
            return None
        cursor = self.token_cursor(position)
        if cursor is None or not self.identifier(cursor):
            return None
        return self.infer_receiver_type(cursor)

    def member_context(self, position):
        """识别 `receiver.` / `receiver.word` 补全形态，返回 (接收者 token 下标, 已输入前缀)。"""
        if not self.valid:
            return None
        lines = self.text.split("\n")
        row, column = position.get("line", -1), position.get("character", -1)
        if not 0 <= row < len(lines) or not 0 <= column <= utf16(lines[row]):
            return None
        offset = sum(len(line) + 1 for line in lines[:row]) + len(line_prefix(self.text, position))
        tokens = self.tokens
        previous = max((index for index, token in enumerate(tokens) if token.end() <= offset), default=None)
        if previous is None:
            return None
        typed, dot = "", None
        if previous and self.identifier(previous) and tokens[previous - 1][0] == ".":
            typed, dot = tokens[previous][0], previous - 1
        elif tokens[previous][0] == ".":
            dot = previous
        if dot is None or tokens[dot].start() < sum(len(line) + 1 for line in lines[:row]):
            return None  # 点号必须与光标同线
        receiver = dot - 1
        if receiver >= 0 and tokens[receiver][0] == ")":
            opening = self.reverse_pairs.get(receiver)
            receiver = -1 if opening is None else opening - 1
        # 仅支持标识符（或单次调用）接收者；`a.b.` 链式与下标结果一律 unknown。
        if receiver < 0 or not self.identifier(receiver) \
                or (receiver and tokens[receiver - 1][0] == "."
                    and "\n" not in self.text[tokens[receiver - 1].end():tokens[receiver].start()]):
            return None
        return receiver, typed

    @property
    def reverse_pairs(self):
        if self._reverse_pairs is None:
            self._reverse_pairs = {closing: opening for opening, closing in self.pairs.items()}
        return self._reverse_pairs

    def infer_receiver_type(self, receiver):
        tokens = self.tokens
        if receiver + 1 < len(tokens) and tokens[receiver + 1][0] == "(" and receiver + 1 in self.pairs:
            binding, unused = self.resolve_token(receiver)  # foo()：函数返回值类型
            return self.infer_from_binding(binding, 0) if binding is not None else None
        binding, free = self.resolve_token(receiver)
        if binding is not None:
            return self.infer_from_binding(binding, 0)
        return free if free in BUILTIN_TYPE_NAMES else None  # Math./Array./... 内置类型直引用

    def infer_from_binding(self, binding, depth):
        if depth > MAX_INFER_DEPTH or not self._spend():
            return None
        if binding.get("init") is not None:
            return self.infer_expression(binding["init"], depth)
        if "body" in binding:
            return self.function_return_type(binding, depth)
        return None

    def infer_expression(self, init, depth):
        """推导 ( = , 首表达式 token, 终止符 ) 三元组的类型；字面量/容器/new/调用/拷贝。"""
        if depth > MAX_INFER_DEPTH or not self._spend():
            return None
        tokens = self.tokens
        equals, first, terminator = init
        if first >= terminator or first >= len(tokens):
            # 表达式整体被遮蔽：字符串与正则字面量
            raw_start = tokens[equals].end() if equals < len(tokens) else len(self.text)
            raw_end = tokens[terminator].start() if terminator < len(tokens) else len(self.text)
            raw = self.text[raw_start:raw_end].lstrip()
            if raw[:1] in ("\"", "'"):
                return "String"
            return "RegExp" if raw[:1] == "/" else None
        last = terminator - 1
        first_token = tokens[first][0]
        if first_token == "new":
            if first + 1 < len(tokens) and self.identifier(first + 1) \
                    and (first + 2 >= terminator or tokens[first + 2][0] == "("):
                return tokens[first + 1][0]  # 内置或工作区类名由 Server.type_members 校验
            return None
        if first_token == "%" and first + 1 < len(tokens) and self.pairs.get(first + 1) == last:
            return "Dictionary"
        if self.pairs.get(first) == last:
            return "Array" if first_token == "[" else None
        if self.identifier(first):
            if first + 1 < len(tokens) and tokens[first + 1][0] == "(" and self.pairs.get(first + 1) == last:
                binding, unused = self.resolve_token(first)  # 函数调用
                return self.infer_from_binding(binding, depth + 1) if binding is not None else None
            if first == last:
                if first_token in BUILTIN_TYPE_NAMES:
                    return first_token
                binding, unused = self.resolve_token(first)  # 变量拷贝链
                return self.infer_from_binding(binding, depth + 1) if binding is not None else None
            return None  # 链式/下标/双目运算：unknown
        if first == last:
            if first_token.isdigit() or re.fullmatch(r"0[xX][\da-fA-F]+", first_token) \
                    or first_token in ("true", "false"):
                return "Integer"
            if re.fullmatch(r"\d+[eE][+-]?\d+", first_token):
                return "Real"
        elif first + 2 == last and first_token.isdigit() and tokens[first + 1][0] == "." \
                and tokens[last][0].isdigit():
            return "Real"  # 词法把 1.5 切成三个 token
        return None

    def function_return_type(self, binding, depth):
        if depth > MAX_INFER_DEPTH or not self._spend():
            return None
        tokens = self.tokens
        start, stop = binding["body"]
        result, seen = None, False
        for index in range(start, min(stop, len(tokens))):
            if tokens[index][0] != "return" or (index and tokens[index - 1][0] == "."):
                continue
            if not self._spend():
                return None  # 预算耗尽：宁可 unknown，不返回部分结果
            expression = index + 1
            while expression < stop and tokens[expression][0] != ";":
                expression += 1
            if expression == index + 1:
                return None  # return; 无表达式：保守放弃
            kind = self.infer_expression((index, index + 1, expression), depth)
            if kind is None or (seen and result != kind):
                return None
            result, seen = kind, True
        return result

    # ---- 绑定级引用收集（rename 用）----

    def references_of_binding(self, binding):
        """解析到该绑定的全部标识符出现（含声明本身）；动态上下文天然被 resolve 拒绝。"""
        name = binding["token"][0]
        return [index for index, token in enumerate(self.tokens) if token[0] == name
                and self.resolve_token(index)[0] is binding]

    def free_references_of_name(self, name):
        """本文件中解析不到任何绑定、且非动态上下文的自由标识符（跨文件全局引用）。"""
        return [index for index, token in enumerate(self.tokens) if token[0] == name
                and self.resolve_token(index) == (None, name)]


class Server:
    def __init__(self, cial, source=None, sink=None):
        self.cial = shutil.which(cial) or str(pathlib.Path(cial).resolve())
        self.source = source or sys.stdin.buffer
        self.sink = sink or sys.stdout.buffer
        self.documents = {}
        self.workspace_roots = set()
        self.symbol_cache = {}
        self.initialized = False
        self.shutdown = False

    def send(self, payload):
        body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.sink.write(b"Content-Length: " + str(len(body)).encode("ascii") + b"\r\n\r\n" + body)
        self.sink.flush()

    def result(self, identifier, value):
        self.send({"jsonrpc": "2.0", "id": identifier, "result": value})

    def error(self, identifier, code, message):
        self.send({"jsonrpc": "2.0", "id": identifier, "error": {"code": code, "message": message}})

    def update_workspace_folders(self, added, removed=()):
        for folders, operation in ((removed, self.workspace_roots.discard), (added, self.workspace_roots.add)):
            for folder in folders:
                path = file_path(folder["uri"])
                if path is not None:
                    operation(path)
        self.symbol_cache.clear()

    def workspace_sources(self):
        def in_workspace(path):
            return not self.workspace_roots or any(path == root or root in path.parents for root in self.workspace_roots)

        # Match files by canonical path, but retain the client's URI for open buffers.
        overlays = {}
        for uri, document in self.documents.items():
            path = file_path(uri)
            if path is not None and path.suffix.lower() == ".tjs" and in_workspace(path):
                overlays[path] = (uri, document["text"])
        sources, visited, total_bytes, file_count, directory_count = [], {}, 0, 0, 0
        # Reserve the budget for current buffers before looking at disk files.
        for path, (uri, text) in sorted(overlays.items()):
            size = len(text.encode("utf-8"))
            if size <= MAX_DOCUMENT_BYTES and total_bytes + size <= MAX_WORKSPACE_BYTES:
                total_bytes += size
                sources.append((uri, declarations(text), text))
        for root in sorted(self.workspace_roots):
            if file_count >= MAX_WORKSPACE_FILES or directory_count >= MAX_WORKSPACE_DIRECTORIES:
                break
            for directory, folders, names in os.walk(root, followlinks=False):
                directory_count += 1
                folders[:] = sorted(name for name in folders if name not in EXCLUDED_DIRECTORIES)
                for name in sorted(names):
                    if file_count >= MAX_WORKSPACE_FILES:
                        break
                    if not name.lower().endswith(".tjs"):
                        continue
                    file_count += 1
                    try:
                        path = (pathlib.Path(directory) / name).resolve()
                        if not in_workspace(path) or path in visited or path in overlays:
                            continue
                        stat = path.stat()
                        if stat.st_size > MAX_DOCUMENT_BYTES or total_bytes + stat.st_size > MAX_WORKSPACE_BYTES:
                            continue
                        signature = (stat.st_mtime_ns, stat.st_size)
                        size = stat.st_size
                        cached = self.symbol_cache.get(path)
                        if cached and cached[0] == signature:
                            symbols = cached[1]
                            source_text = cached[2]
                        else:
                            with path.open("rb") as stream:
                                data = stream.read(MAX_DOCUMENT_BYTES + 1)
                            if len(data) > MAX_DOCUMENT_BYTES:
                                continue
                            size = len(data)
                            source_text = data.decode("utf-8-sig")
                            symbols = declarations(source_text)
                    except (OSError, UnicodeError, RuntimeError):
                        continue
                    if total_bytes + size > MAX_WORKSPACE_BYTES:
                        continue
                    total_bytes += size
                    visited[path] = (signature, symbols, source_text)
                    sources.append((path.as_uri(), symbols, source_text))
                if file_count >= MAX_WORKSPACE_FILES or directory_count >= MAX_WORKSPACE_DIRECTORIES:
                    break
        self.symbol_cache = visited
        return sources

    def workspace_symbols(self, query):
        sources = self.workspace_sources()
        query = query.casefold()
        results = [{"name": symbol["name"], "kind": symbol["kind"],
                    "location": {"uri": uri, "range": symbol["selectionRange"]}}
                   for uri, symbols, unused in sources for symbol in symbols if query in symbol["name"].casefold()]
        results.sort(key=lambda symbol: (symbol["name"].casefold(), symbol["location"]["uri"],
                                         symbol["location"]["range"]["start"]["line"],
                                         symbol["location"]["range"]["start"]["character"]))
        return results[:MAX_WORKSPACE_SYMBOLS]

    def document_index(self, uri):
        document = self.documents.get(uri)
        if document is None or len(document["text"].encode("utf-8")) > MAX_DOCUMENT_BYTES:
            return None
        return DefinitionIndex(document["text"])

    def definition(self, uri, position):
        current = self.document_index(uri)
        if current is None:
            return None
        binding, global_name = current.resolve(position)
        if binding is not None:
            return current.location(binding, uri)
        if global_name is None:
            return None
        matches = []
        for source_uri, symbols, text in self.workspace_sources():
            index = DefinitionIndex(text)
            if not index.valid:
                if any(symbol["name"] == global_name for symbol in symbols):
                    return None  # 无法确认作用域的声明：宁可放弃
                continue
            for candidate in index.root.bindings.get(global_name, []):
                matches.append(index.location(candidate, source_uri))
        if not matches:
            return None
        matches.sort(key=lambda location: (location["uri"] != uri, location["uri"],
                                           location["range"]["start"]["line"],
                                           location["range"]["start"]["character"]))
        return matches[0] if len(matches) == 1 else matches  # 唯一时保持单 Location 兼容

    def unique_workspace_binding(self, name):
        """name 的唯一顶层全局声明 (uri, index, binding)；无/多义/作用域未知返回 None。"""
        found = None
        for source_uri, symbols, text in self.workspace_sources():
            index = DefinitionIndex(text)
            if not index.valid:
                if any(symbol["name"] == name for symbol in symbols):
                    return None  # 作用域未知：视为多义
                continue
            bindings = index.root.bindings.get(name, [])
            if len(bindings) > 1:
                return None
            if bindings and found is not None:
                return None
            if bindings:
                found = (source_uri, index, bindings[0])
        return found

    def unique_workspace_class(self, name, current):
        """当前文件优先、其次工作区唯一的顶层 class 绑定；找不到/不唯一返回 None。"""
        roots = current.root.bindings.get(name, [])
        candidates = [binding for binding in roots if binding.get("is_class")]
        if roots and not candidates:
            return None  # 同名非类顶层绑定：不让工作区类穿透
        if len(candidates) > 1:
            return None
        if candidates:
            return candidates[0]
        found = None
        for source_uri, symbols, text in self.workspace_sources():
            index = DefinitionIndex(text)
            if not index.valid:
                continue
            for binding in index.root.bindings.get(name, []):
                if binding.get("is_class"):
                    if found is not None:
                        return None
                    found = binding
        return found

    @staticmethod
    def class_member_item(name, binding, owner):
        if "body" in binding:
            kind, detail = MEMBER_METHOD, "function " + name + "(...)"
        elif binding.get("is_class"):
            kind, detail = 7, "class " + name
        elif binding.get("is_property"):
            kind, detail = MEMBER_PROPERTY, "property " + name
        else:
            kind, detail = MEMBER_FIELD, "var " + name
        return {"label": name, "kind": kind, "detail": detail + " — " + owner}

    def type_members(self, type_name, current):
        """类型名 -> 成员补全项列表；内置类型走注册面名单，工作区 class 沿 extends 展开。"""
        builtin = BUILTIN_MEMBERS.get(type_name)
        if builtin is not None:
            return [{"label": label, "kind": kind, "detail": signature}
                    for label, (kind, signature) in sorted(builtin.items())]
        collected, visited, name, depth = {}, set(), type_name, 0
        while name and name not in visited and depth < MAX_CLASS_HIERARCHY:
            visited.add(name)
            binding = self.unique_workspace_class(name, current)
            if binding is None:
                break
            for member, bindings in binding["body_scope"].bindings.items():
                if member not in collected:  # 派生类成员优先，覆盖基类同名成员
                    collected[member] = self.class_member_item(member, bindings[0], type_name)
            name, depth = binding.get("extends"), depth + 1
        return sorted(collected.values(), key=lambda item: item["label"]) if collected else None

    def prepare_rename(self, uri, position):
        index = self.document_index(uri)
        if index is None:
            return None
        cursor = index.token_cursor(position)
        if cursor is None or not index.identifier(cursor):
            return None
        binding, free = index.resolve_token(cursor)
        if binding is None and (free is None or self.unique_workspace_binding(free) is None):
            return None  # 关键字/动态成员/内置类名/无法解析的自由标识符
        return {"range": index.token_range(cursor), "placeholder": index.tokens[cursor][0]}

    def rename(self, uri, position, new_name):
        current = self.document_index(uri)
        if current is None or not re.fullmatch(r"[\w$]+", new_name or "") or new_name[0].isdigit():
            return None
        binding, free = current.resolve(position)
        if binding is None and free is None:
            return None
        target_uri, target_index, target = uri, current, binding
        if binding is None:  # 光标在自由标识符上：改工作区唯一顶层声明
            found = self.unique_workspace_binding(free)
            if found is None:
                return None
            target_uri, target_index, target = found
        files = {target_uri: (target_index, target_index.references_of_binding(target))}
        if target["scope"] is target_index.root:
            # 顶层绑定：与 definition 的跨文件判定一致，要求工作区唯一
            found = self.unique_workspace_binding(target["token"][0])
            if found is None or found[0] != target_uri \
                    or found[2]["token"].start() != target["token"].start():
                return None
            for source_uri, symbols, text in self.workspace_sources():
                if source_uri == target_uri:
                    continue
                index = DefinitionIndex(text)
                if not index.valid:
                    if any(symbol["name"] == target["token"][0] for symbol in symbols):
                        return None
                    continue
                references = index.free_references_of_name(target["token"][0])
                if references:
                    files[source_uri] = (index, references)
        return self.build_rename(files, new_name)

    @staticmethod
    def build_rename(files, new_name):
        changes, total = {}, 0
        for file_uri, (index, cursors) in files.items():
            edits = [{"range": index.token_range(cursor), "newText": new_name} for cursor in cursors]
            edits.sort(key=lambda edit: (edit["range"]["start"]["line"], edit["range"]["start"]["character"]))
            changes[file_uri] = edits
            total += len(edits)
        if total > MAX_RENAME_EDITS:
            return None
        return {"changes": changes} if changes else None

    def check(self, text):
        with tempfile.TemporaryDirectory(prefix="cial-lsp-") as directory:
            path = pathlib.Path(directory) / "document.tjs"
            path.write_text(text, encoding="utf-8")
            try:
                run = subprocess.run([self.cial, "--check-json", str(path)], capture_output=True, timeout=5)
                report = json.loads(run.stdout.decode("utf-8"))
                if run.returncode not in (0, 1):
                    raise ValueError("checker exited with status " + str(run.returncode))
                diagnostics = []
                for diagnostic in report["diagnostics"]:
                    start = source_position(text, diagnostic["line"], diagnostic["column"])
                    end = source_position(text, diagnostic["endLine"], diagnostic["endColumn"] + 1)
                    if (end["line"], end["character"]) < (start["line"], start["character"]):
                        end = start.copy()
                    diagnostics.append({"range": {"start": start, "end": end},
                                        "severity": diagnostic["severity"], "source": "ciallang",
                                        "message": re.sub(r"^\([^@]*@\d+:\d+\)\s*", "", diagnostic["message"])})
                return diagnostics
            except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as error:
                return [{"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 0}},
                         "severity": 1, "source": "ciallang", "message": "Syntax checker unavailable: " + str(error)}]

    def publish(self, uri, text, version):
        diagnostics = self.check(text) if len(text.encode("utf-8")) <= 2_000_000 else [
            {"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 0}},
             "severity": 2, "source": "ciallang", "message": "Document exceeds the 2 MB diagnostics limit."}]
        self.send({"jsonrpc": "2.0", "method": "textDocument/publishDiagnostics",
                   "params": {"uri": uri, "version": version, "diagnostics": diagnostics}})

    def handle(self, request):
        if request.get("jsonrpc") != "2.0":
            self.error(request.get("id"), -32600, "Expected JSON-RPC 2.0")
            return None
        method = request.get("method")
        params = request.get("params", {})
        identifier = request.get("id")
        if method == "exit":
            return 0 if self.shutdown else 1
        if method == "initialize":
            if self.initialized or self.shutdown:
                self.error(identifier, -32600, "Server was already initialized")
                return None
            self.initialized = True
            folders = params.get("workspaceFolders")
            if folders is None:
                root_uri = params.get("rootUri")
                if root_uri:
                    folders = [{"uri": root_uri}]
                elif params.get("rootPath"):
                    folders = [{"uri": pathlib.Path(params["rootPath"]).resolve().as_uri()}]
            self.update_workspace_folders(folders or [])
            self.result(identifier, {"capabilities": {"positionEncoding": "utf-16", "textDocumentSync": {"openClose": True, "change": 1},
                                                     "completionProvider": {}, "hoverProvider": True,
                                                     "documentSymbolProvider": True, "workspaceSymbolProvider": True,
                                                     "definitionProvider": True,
                                                     "renameProvider": {"prepareProvider": True},
                                                     "workspace": {"workspaceFolders": {"supported": True,
                                                                                         "changeNotifications": True}}},
                                     "serverInfo": {"name": "Ciallang LSP", "version": "0.3.0"}})
        elif not self.initialized or self.shutdown:
            if "id" in request:
                self.error(identifier, -32002 if not self.initialized else -32600, "Server is not active")
        elif method == "shutdown":
            self.shutdown = True
            self.result(identifier, None)
        elif method == "workspace/didChangeWorkspaceFolders":
            event = params["event"]
            self.update_workspace_folders(event.get("added", []), event.get("removed", []))
        elif method == "workspace/symbol":
            query = params.get("query", "")
            if not isinstance(query, str):
                raise ValueError("Workspace symbol query must be a string")
            self.result(identifier, self.workspace_symbols(query))
        elif method == "textDocument/definition":
            self.result(identifier, self.definition(params["textDocument"]["uri"], params["position"]))
        elif method == "textDocument/prepareRename":
            self.result(identifier, self.prepare_rename(params["textDocument"]["uri"], params["position"]))
        elif method == "textDocument/rename":
            self.result(identifier, self.rename(params["textDocument"]["uri"], params["position"], params.get("newName")))
        elif method in ("initialized", "$/cancelRequest", "$/setTrace"):
            pass
        elif method in ("textDocument/didOpen", "textDocument/didChange"):
            document = params["textDocument"]
            uri, version = document["uri"], document["version"]
            old = self.documents.get(uri)
            if old and version <= old["version"]:
                return None
            if method.endswith("didOpen"):
                text = document["text"]
            else:
                changes = params["contentChanges"]
                if not changes or any("range" in change for change in changes):
                    return None  # 仅声明完整文本同步，不接受错误的增量更新。
                text = changes[-1]["text"]
            self.documents[uri] = {"text": text, "version": version}
            self.publish(uri, text, version)
        elif method == "textDocument/didClose":
            uri = params["textDocument"]["uri"]
            self.documents.pop(uri, None)
            self.send({"jsonrpc": "2.0", "method": "textDocument/publishDiagnostics",
                       "params": {"uri": uri, "diagnostics": []}})
        elif method in ("textDocument/completion", "textDocument/hover", "textDocument/documentSymbol"):
            document = self.documents.get(params["textDocument"]["uri"], {"text": ""})
            text = document["text"]
            if method.endswith("completion"):
                prefix = line_prefix(text, params["position"])
                match = re.search(r"[\w$]+$", prefix)
                typed = match[0] if match else ""
                items = None
                if "." in prefix:  # 成员形态才值得建索引做类型推导
                    index = self.document_index(params["textDocument"]["uri"])
                    context = index.member_context(params["position"]) if index is not None else None
                    if context is not None:
                        type_name = index.infer_receiver_type(context[0])
                        members = self.type_members(type_name, index) if type_name else None
                        if members is not None:
                            items = [item for item in members if item["label"].startswith(context[1])]
                if items is None:  # 推导不出类型/非成员位置：回落既有词法补全
                    names = set(KEYWORDS) | {symbol["name"] for symbol in declarations(text)}
                    items = [{"label": name, "kind": 14 if name in KEYWORDS else 6,
                              "detail": KEYWORDS.get(name, "document declaration")}
                             for name in sorted(names) if name.startswith(typed)]
                self.result(identifier, items)
            elif method.endswith("hover"):
                prefix = line_prefix(text, params["position"])
                line = text.split("\n")[params["position"]["line"]]
                column = len(prefix)
                match = next((match for match in WORD.finditer(line) if match.start() <= column < match.end()), None)
                help_text = KEYWORDS.get(match[0]) if match else None
                self.result(identifier, {"contents": {"kind": "plaintext", "value": help_text}} if help_text else None)
            else:
                self.result(identifier, declarations(text))
        elif "id" in request:
            self.error(identifier, -32601, "Method not found: " + str(method))
        return None

    def run(self):
        while True:
            headers = {}
            while True:
                line = self.source.readline()
                if not line:
                    return 0
                if line in (b"\r\n", b"\n"):
                    break
                key, value = line.decode("ascii").split(":", 1)
                headers[key.lower().strip()] = value.strip()
            length = int(headers.get("content-length", "-1"))
            if not 0 <= length <= 8_000_000:
                raise ValueError("Invalid Content-Length")
            body = self.source.read(length)
            if len(body) != length:
                return 1
            request = {}
            try:
                request = json.loads(body)
                if not isinstance(request, dict):
                    raise ValueError("Expected a request object")
                result = self.handle(request)
                if result is not None:
                    return result
            except json.JSONDecodeError as error:
                self.error(None, -32700, str(error))
            except (ValueError, KeyError, IndexError, TypeError) as error:
                self.error(request.get("id") if isinstance(request, dict) else None, -32602, str(error))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cial", required=True, help="Path to the built cial executable")
    args = parser.parse_args()
    return Server(args.cial).run()


if __name__ == "__main__":
    raise SystemExit(main())
