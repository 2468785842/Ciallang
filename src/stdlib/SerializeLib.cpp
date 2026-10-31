// Copyright (c) 2026/10/26
//
// Array/Dictionary 结构化存取与 Octet pack 系实现。
// 二进制格式：tTJSBinarySerializer（MessagePack 近似，UTF-16LE 字符串，
// 头 'K','B','A','D','1','0','0',0）；文本格式：(const) %[...] / (const) [...]
// 可再解析表达式（EscapeC 转义 + 实数十六进制浮点串）。
// 文件文本编码：写出 UTF-8 + BOM、\r\n 行尾（TJS_TEXT_OUT_CRLF 同）；读取
// 自动识别 UTF-8/UTF-16LE/BE BOM（可读真实 krkr 写出的 UTF-16 文件）。

#define _CRT_SECURE_NO_WARNINGS

#include "SerializeLib.hpp"
#include "runtime/Context.hpp"

#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include "common/UTF8.hpp"
#include "types/Octet.hpp"
#include "vm/VM.hpp"

namespace cial::stdlib {
    void registerSerializeLib(Context &context) {
        context.nativeType<Array>()
            .method("load"_str, &stdlib::arrayLoad)
            .method("save"_str, &stdlib::arraySave)
            .method("loadStruct"_str, &stdlib::arrayLoadStruct)
            .method("saveStruct"_str, &stdlib::arraySaveStruct)
            .method("pack"_str, &stdlib::arrayPack)
            .method("assignStruct"_str, &stdlib::arrayAssignStruct);
        context.nativeType<Dictionary>()
            .method("load"_str, &stdlib::dictLoad)
            .method("save"_str, &stdlib::dictSave)
            .method("loadStruct"_str, &stdlib::dictLoadStruct)
            .method("saveStruct"_str, &stdlib::dictSaveStruct)
            .method("assignStruct"_str, &stdlib::dictAssignStruct);
        context.nativeType<Octet>().method("unpack"_str, &stdlib::octetUnpack);
    }

    namespace {

        constexpr u8 SER_HEADER[8] = { 'K', 'B', 'A', 'D', '1', '0', '0', 0 };

        // 二进制类型码（tjsBinarySerializer.h 全量）
        enum : u8 {
            TYPE_POSITIVE_FIX_NUM_MAX = 0x7F,
            TYPE_NEGATIVE_FIX_NUM_MIN = 0xE0,
            TYPE_NIL = 0xC0,
            TYPE_VOID = 0xC1,
            TYPE_TRUE = 0xC2,
            TYPE_FALSE = 0xC3,
            TYPE_STRING8 = 0xC4,
            TYPE_STRING16 = 0xC5,
            TYPE_STRING32 = 0xC6,
            TYPE_FLOAT = 0xCA,
            TYPE_DOUBLE = 0xCB,
            TYPE_UINT8 = 0xCC,
            TYPE_UINT16 = 0xCD,
            TYPE_UINT32 = 0xCE,
            TYPE_UINT64 = 0xCF,
            TYPE_INT8 = 0xD0,
            TYPE_INT16 = 0xD1,
            TYPE_INT32 = 0xD2,
            TYPE_INT64 = 0xD3,
            TYPE_FIX_RAW_MIN = 0xD4,
            TYPE_FIX_RAW_MAX = 0xD9,
            TYPE_RAW16 = 0xDA,
            TYPE_RAW32 = 0xDB,
            TYPE_ARRAY16 = 0xDC,
            TYPE_ARRAY32 = 0xDD,
            TYPE_MAP16 = 0xDE,
            TYPE_MAP32 = 0xDF,
            TYPE_FIX_STRING_MIN = 0xA0,
            TYPE_FIX_STRING_MAX = 0xBF,
            TYPE_FIX_ARRAY_MIN = 0x90,
            TYPE_FIX_ARRAY_MAX = 0x9F,
            TYPE_FIX_MAP_MIN = 0x80,
            TYPE_FIX_MAP_MAX = 0x8F,
        };

        [[noreturn]] void serError(const char *msg) { throw std::runtime_error(msg); }

        // ---------- UTF-8 <-> UTF-16LE（二进制格式的字符串存储态）----------

        std::string utf8ToUtf16Le(const std::string &s) {
            std::string out;
            out.reserve(s.size() * 2 + 2);
            size_t i = 0;
            while(i < s.size()) {
                const auto cp = Common::utf8Decode(s.data() + i, s.size() - i);
                i += cp.width > 0 ? static_cast<size_t>(cp.width) : 1;
                char32_t c = static_cast<char32_t>(cp.value);
                if(c <= 0xFFFF) {
                    out.push_back(static_cast<char>(c & 0xFF));
                    out.push_back(static_cast<char>((c >> 8) & 0xFF));
                } else {
                    c -= 0x10000;
                    const u32 hi = 0xD800 | (c >> 10);
                    const u32 lo = 0xDC00 | (c & 0x3FF);
                    out.push_back(static_cast<char>(hi & 0xFF));
                    out.push_back(static_cast<char>((hi >> 8) & 0xFF));
                    out.push_back(static_cast<char>(lo & 0xFF));
                    out.push_back(static_cast<char>((lo >> 8) & 0xFF));
                }
            }
            return out;
        }

        std::string utf16LeToUtf8(const u8 *data, const size_t bytes) {
            std::string out;
            out.reserve(bytes);
            for(size_t i = 0; i + 1 < bytes;) {
                char32_t u = data[i] | (static_cast<char32_t>(data[i + 1]) << 8);
                i += 2;
                if(u >= 0xD800 && u <= 0xDBFF && i + 1 < bytes) {
                    const char32_t lo = data[i] | (static_cast<char32_t>(data[i + 1]) << 8);
                    if(lo >= 0xDC00 && lo <= 0xDFFF) {
                        u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                        i += 2;
                    }
                }
                const auto enc = Common::utf8Encode(static_cast<i32>(u));
                if(enc.width > 0)
                    out.append(reinterpret_cast<const char *>(enc.data), static_cast<size_t>(enc.width));
            }
            return out;
        }

        // ---------- 文件 IO ----------

        std::string readFileBytes(const String &name) {
            std::ifstream f(name.toStdStr(), std::ios::binary);
            if(!f)
                serError("cannot open file");
            std::string content{ (std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>() };
            return content;
        }

        void writeFileBytes(const String &name, const std::string &data) {
            std::ofstream f(name.toStdStr(), std::ios::binary | std::ios::trunc);
            if(!f)
                serError("cannot open file");
            f.write(data.data(), static_cast<std::streamsize>(data.size()));
            if(!f)
                serError("cannot write file");
        }

        // 文本读取：识别 UTF-8/UTF-16LE/BE BOM 并解码为 UTF-8；无 BOM 按原样
        std::string readTextDecoded(const String &name) {
            const std::string raw = readFileBytes(name);
            if(raw.size() >= 3 && static_cast<u8>(raw[0]) == 0xEF && static_cast<u8>(raw[1]) == 0xBB &&
               static_cast<u8>(raw[2]) == 0xBF)
                return raw.substr(3);
            if(raw.size() >= 2 && static_cast<u8>(raw[0]) == 0xFF && static_cast<u8>(raw[1]) == 0xFE)
                return utf16LeToUtf8(reinterpret_cast<const u8 *>(raw.data() + 2), raw.size() - 2);
            if(raw.size() >= 2 && static_cast<u8>(raw[0]) == 0xFE && static_cast<u8>(raw[1]) == 0xFF) {
                // UTF-16BE → 翻转字节序后复用 LE 解码
                std::string sw(raw.substr(2));
                for(size_t i = 0; i + 1 < sw.size(); i += 2)
                    std::swap(sw[i], sw[i + 1]);
                return utf16LeToUtf8(reinterpret_cast<const u8 *>(sw.data()), sw.size());
            }
            return raw;
        }

        void appendUtf8(std::string &out, char32_t c) {
            const auto enc = Common::utf8Encode(static_cast<i32>(c));
            if(enc.width > 0)
                out.append(reinterpret_cast<const char *>(enc.data), static_cast<size_t>(enc.width));
        }

        String makeString(const std::string &s) { return String(s.c_str(), static_cast<u32>(s.size())); }

        std::string valueArgToString(const Value &v) {
            if(v.isString())
                return v.asString().unwrap()->toStdStr();
            if(v.isVoid())
                return {};
            Value t = v;
            if(const auto r = t.toString(); !r.isFailed() && t.isString())
                return t.asString().unwrap()->toStdStr();
            return {};
        }

        // 数值实参宽转换（TJS2 `(tjs_int)*variant` 语义：字符串/实数可转换）
        i64 coerceInt(const Value &v) {
            if(v.isVoid())
                return 0;
            if(v.isInteger())
                return v.asInteger().unwrap();
            if(v.isReal())
                return static_cast<i64>(v.asReal().unwrap().value());
            if(v.isString())
                return std::strtoll(v.asString().unwrap()->toStdStr().c_str(), nullptr, 10);
            return 0;
        }

        double coerceReal(const Value &v) {
            if(v.isVoid())
                return 0.0;
            if(v.isReal())
                return v.asReal().unwrap().value();
            if(v.isInteger())
                return static_cast<double>(v.asInteger().unwrap());
            if(v.isString())
                return std::strtod(v.asString().unwrap()->toStdStr().c_str(), nullptr);
            return 0.0;
        }

        std::string argString(const size_t argCount, const Value *args, const size_t idx) {
            if(argCount < idx + 1)
                serError("expects a file name");
            return valueArgToString(args[idx]);
        }

        bool modeHasFlag(const size_t argCount, const Value *args, const size_t idx, const char flag) {
            if(argCount < idx + 1 || args[idx].isVoid())
                return false;
            return valueArgToString(args[idx]).find(flag) != std::string::npos;
        }

        // ---------- 二进制写出 ----------

        void putBytes(std::string &out, const void *data, const size_t len) {
            out.append(static_cast<const char *>(data), len);
        }

        void putU8(std::string &out, const u8 v) { out.push_back(static_cast<char>(v)); }

        void putU16(std::string &out, const u16 v) {
            putU8(out, static_cast<u8>(v & 0xFF));
            putU8(out, static_cast<u8>((v >> 8) & 0xFF));
        }

        void putU32(std::string &out, const u32 v) {
            putU16(out, static_cast<u16>(v & 0xFFFF));
            putU16(out, static_cast<u16>((v >> 16) & 0xFFFF));
        }

        void putU64(std::string &out, const u64 v) {
            putU32(out, static_cast<u32>(v & 0xFFFFFFFFu));
            putU32(out, static_cast<u32>((v >> 32) & 0xFFFFFFFFu));
        }

        // 整数变长阶梯（tTJSBinarySerializer::PutInteger 全对齐）
        void putInteger(std::string &out, const i64 b) {
            if(b < 0) {
                if(b >= TYPE_NEGATIVE_FIX_NUM_MIN) {
                    putU8(out, static_cast<u8>(b));
                } else if(b >= -128) {
                    putU8(out, TYPE_INT8);
                    putU8(out, static_cast<u8>(b));
                } else if(b >= -32768) {
                    putU8(out, TYPE_INT16);
                    putU16(out, static_cast<u16>(static_cast<u16>(b)));
                } else if(b >= -2147483647 - 1) {
                    putU8(out, TYPE_INT32);
                    putU32(out, static_cast<u32>(static_cast<i32>(b)));
                } else {
                    putU8(out, TYPE_INT64);
                    putU64(out, static_cast<u64>(b));
                }
            } else {
                if(b <= TYPE_POSITIVE_FIX_NUM_MAX) {
                    putU8(out, static_cast<u8>(b));
                } else if(b <= 0xFF) {
                    putU8(out, TYPE_UINT8);
                    putU8(out, static_cast<u8>(b));
                } else if(b <= 0xFFFF) {
                    putU8(out, TYPE_UINT16);
                    putU16(out, static_cast<u16>(b));
                } else if(b <= 0xFFFFFFFFu) {
                    putU8(out, TYPE_UINT32);
                    putU32(out, static_cast<u32>(b));
                } else {
                    putU8(out, TYPE_UINT64);
                    putU64(out, static_cast<u64>(b));
                }
            }
        }

        void putString(std::string &out, const std::string &utf8) {
            const std::string u16bytes = utf8ToUtf16Le(utf8);
            const size_t len = u16bytes.size() / 2; // UTF-16 码元数
            if(len <= static_cast<size_t>(TYPE_FIX_STRING_MAX - TYPE_FIX_STRING_MIN)) {
                putU8(out, static_cast<u8>(TYPE_FIX_STRING_MIN + len));
            } else if(len <= 0xFF) {
                putU8(out, TYPE_STRING8);
                putU8(out, static_cast<u8>(len));
            } else if(len <= 0xFFFF) {
                putU8(out, TYPE_STRING16);
                putU16(out, static_cast<u16>(len));
            } else {
                putU8(out, TYPE_STRING32);
                putU32(out, static_cast<u32>(len));
            }
            putBytes(out, u16bytes.data(), u16bytes.size());
        }

        void putDouble(std::string &out, const double d) {
            u64 v{};
            std::memcpy(&v, &d, sizeof(v));
            putU8(out, TYPE_DOUBLE);
            putU64(out, v);
        }

        void putRaw(std::string &out, const u8 *data, const size_t len) {
            if(len <= static_cast<size_t>(TYPE_FIX_RAW_MAX - TYPE_FIX_RAW_MIN)) {
                putU8(out, static_cast<u8>(TYPE_FIX_RAW_MIN + len));
            } else if(len <= 0xFFFF) {
                putU8(out, TYPE_RAW16);
                putU16(out, static_cast<u16>(len));
            } else {
                putU8(out, TYPE_RAW32);
                putU32(out, static_cast<u32>(len));
            }
            putBytes(out, data, len);
        }

        void putStartMap(std::string &out, const size_t count) {
            if(count <= static_cast<size_t>(TYPE_FIX_MAP_MAX - TYPE_FIX_MAP_MIN)) {
                putU8(out, static_cast<u8>(TYPE_FIX_MAP_MIN + count));
            } else if(count <= 0xFFFF) {
                putU8(out, TYPE_MAP16);
                putU16(out, static_cast<u16>(count));
            } else {
                putU8(out, TYPE_MAP32);
                putU32(out, static_cast<u32>(count));
            }
        }

        void putStartArray(std::string &out, const size_t count) {
            if(count <= static_cast<size_t>(TYPE_FIX_ARRAY_MAX - TYPE_FIX_ARRAY_MIN)) {
                putU8(out, static_cast<u8>(TYPE_FIX_ARRAY_MIN + count));
            } else if(count <= 0xFFFF) {
                putU8(out, TYPE_ARRAY16);
                putU16(out, static_cast<u16>(count));
            } else {
                putU8(out, TYPE_ARRAY32);
                putU32(out, static_cast<u32>(count));
            }
        }

        // 标量 variant 写出（对象由调用方的结构化递归处理）
        void putScalar(std::string &out, const Value &v) {
            if(v.isVoid()) {
                putU8(out, TYPE_VOID);
            } else if(v.isInteger()) {
                putInteger(out, v.asInteger().unwrap());
            } else if(v.isReal()) {
                putDouble(out, v.asReal().unwrap().value());
            } else if(v.isString()) {
                putString(out, v.asString().unwrap()->toStdStr());
            } else if(v.isOctet()) {
                const Octet *o = v.asOctet().unwrap();
                putRaw(out, o->getData(), o->getSize());
            } else {
                serError("cannot serialize value");
            }
        }

        // ---------- 结构化文本：EscapeC 与表达式化 ----------

        bool isHexDigitChar(const char32_t c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        }

        // tTJSString::EscapeC 对齐：控制字符 \xNN（小写十六进制），\x 后跟十六进制
        // 字符时该字符也强制 \xNN（原版 hexflag 传播规则）
        std::string escapeC(const std::string &s) {
            std::string out;
            out.reserve(s.size());
            bool hexflag = false;
            size_t i = 0;
            while(i < s.size()) {
                const auto cp = Common::utf8Decode(s.data() + i, s.size() - i);
                const int w = cp.width > 0 ? cp.width : 1;
                const char32_t c = static_cast<char32_t>(cp.value);
                i += static_cast<size_t>(w);
                switch(c) {
                    case 0x07:
                        out += "\\a";
                        hexflag = false;
                        continue;
                    case 0x08:
                        out += "\\b";
                        hexflag = false;
                        continue;
                    case 0x0C:
                        out += "\\f";
                        hexflag = false;
                        continue;
                    case 0x0A:
                        out += "\\n";
                        hexflag = false;
                        continue;
                    case 0x0D:
                        out += "\\r";
                        hexflag = false;
                        continue;
                    case 0x09:
                        out += "\\t";
                        hexflag = false;
                        continue;
                    case 0x0B:
                        out += "\\v";
                        hexflag = false;
                        continue;
                    case '\\':
                        out += "\\\\";
                        hexflag = false;
                        continue;
                    case '\'':
                        out += "\\'";
                        hexflag = false;
                        continue;
                    case '"':
                        out += "\\\"";
                        hexflag = false;
                        continue;
                    default:
                        break;
                }
                char buf[8];
                if(hexflag && isHexDigitChar(c)) {
                    std::snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned>(c));
                    out += buf;
                    hexflag = true;
                } else if(c < 0x20) {
                    std::snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned>(c));
                    out += buf;
                    hexflag = true;
                } else {
                    appendUtf8(out, c);
                    hexflag = false;
                }
            }
            return out;
        }

        // TJSRealToHexString 对齐：[-]0x1.<13 位大写十六进制>p<无偏置指数>
        std::string realToHexString(const double d) {
            u64 bits{};
            std::memcpy(&bits, &d, sizeof(bits));
            std::string out;
            if(bits >> 63)
                out += '-';
            out += "0x1.";
            static constexpr char hex[] = "0123456789ABCDEF";
            for(int bits_ = 52; bits_ > 0;) {
                bits_ -= 4;
                out += hex[(bits >> bits_) & 0xF];
            }
            out += 'p';
            out += std::to_string(static_cast<i32>((bits >> 52) & 0x7FF) - 1023);
            return out;
        }

        // TJSVariantToExpressionString 对齐（标量；对象在结构化递归中另行处理）
        std::string expressionString(const Value &v) {
            if(v.isVoid())
                return "void";
            if(v.isInteger())
                return std::to_string(v.asInteger().unwrap());
            if(v.isReal()) {
                const double d = v.asReal().unwrap().value();
                if(std::isnan(d))
                    return "NaN";
                if(std::isinf(d))
                    return d > 0 ? "+Infinity" : "-Infinity";
                if(d == 0.0)
                    return std::signbit(d) ? "-0.0" : "+0.0";
                return realToHexString(d) + " /* " + valueArgToString(v) + " */";
            }
            if(v.isString())
                return "\"" + escapeC(v.asString().unwrap()->toStdStr()) + "\"";
            if(v.isOctet()) {
                const Octet *o = v.asOctet().unwrap();
                std::string out = "<% ";
                char buf[8];
                for(u32 i = 0; i < o->getSize(); ++i) {
                    if(i)
                        out += ' ';
                    std::snprintf(buf, sizeof(buf), "0x%02X", o->getData()[i]);
                    out += buf;
                }
                out += " %>";
                return out;
            }
            return {};
        }

        // ---------- 结构化文本/二进制写出（递归 + 环检测）----------

        using ObjStack = Vec<const Object *>;

        // Atom → 原字符串（AtomTable 内的驻留串）
        std::string atomString(const AtomTable &table, const Atom a) {
            const auto *entry = table.get(a);
            return entry && entry->str ? entry->str->toStdStr() : std::string{};
        }

        bool stackContains(const ObjStack &stack, const Object *o) {
            for(const auto *s : stack)
                if(s == o)
                    return true;
            return false;
        }

        void saveStructTextValue(std::string &out, const Value &v, ObjStack &stack, const std::string &indent,
                                 const AtomTable &table);

        void saveStructTextArray(std::string &out, const Array *arr, ObjStack &stack, const std::string &indent,
                                 const AtomTable &table) {
            stack.push_back(arr);
            out += "(const) [\r\n";
            const std::string indent2 = indent + " ";
            for(size_t i = 0; i < arr->elements.size(); ++i) {
                out += indent2;
                const Value &item = arr->elements[i];
                if(item.isObject())
                    saveStructTextValue(out, item, stack, indent2, table);
                else
                    out += expressionString(item);
                out += (i != arr->elements.size() - 1) ? ",\r\n" : "\r\n";
            }
            out += indent;
            out += "]";
            stack.pop_back();
        }

        void saveStructTextDictionary(std::string &out, const Dictionary *dic, ObjStack &stack,
                                      const std::string &indent, const AtomTable &table) {
            stack.push_back(dic);
            out += "(const) %[\r\n";
            const std::string indent2 = indent + " ";
            bool first = true;
            for(const auto &[atom, value] : dic->members) {
                if(!first)
                    out += ",\r\n";
                first = false;
                out += indent2;
                out += '"';
                out += escapeC(atomString(table, atom));
                out += "\" => ";
                if(value.isObject())
                    saveStructTextValue(out, value, stack, indent2, table);
                else
                    out += expressionString(value);
            }
            if(!first)
                out += "\r\n";
            out += indent;
            out += "]";
            stack.pop_back();
        }

        void saveStructTextValue(std::string &out, const Value &v, ObjStack &stack, const std::string &indent,
                                 const AtomTable &table) {
            auto *obj = v.asObject().unwrap();
            if(stackContains(stack, obj)) {
                out += "null /* object recursion detected */";
                return;
            }
            if(auto *arr = dynamic_cast<const Array *>(obj)) {
                saveStructTextArray(out, arr, stack, indent, table);
            } else if(auto *dic = dynamic_cast<const Dictionary *>(obj)) {
                saveStructTextDictionary(out, dic, stack, indent, table);
            } else {
                // 其他对象存为 null（原版同），对象调试串留在注释里
                std::ostringstream oss;
                oss << v;
                out += "null /* (object) \"" + escapeC(oss.str()) + "\" */";
            }
        }

        void saveStructBinaryValue(std::string &out, const Value &v, ObjStack &stack, const AtomTable &table);

        void saveStructBinaryArray(std::string &out, const Array *arr, ObjStack &stack, const AtomTable &table) {
            stack.push_back(arr);
            putStartArray(out, arr->elements.size());
            for(const auto &item : arr->elements) {
                if(item.isObject())
                    saveStructBinaryValue(out, item, stack, table);
                else
                    putScalar(out, item);
            }
            stack.pop_back();
        }

        void saveStructBinaryDictionary(std::string &out, const Dictionary *dic, ObjStack &stack,
                                        const AtomTable &table) {
            stack.push_back(dic);
            putStartMap(out, dic->members.size());
            for(const auto &[atom, value] : dic->members) {
                putString(out, atomString(table, atom));
                if(value.isObject())
                    saveStructBinaryValue(out, value, stack, table);
                else
                    putScalar(out, value);
            }
            stack.pop_back();
        }

        void saveStructBinaryValue(std::string &out, const Value &v, ObjStack &stack, const AtomTable &table) {
            auto *obj = v.asObject().unwrap();
            if(stackContains(stack, obj)) {
                putU8(out, TYPE_NIL); // 环 → null
                return;
            }
            if(auto *arr = dynamic_cast<const Array *>(obj)) {
                saveStructBinaryArray(out, arr, stack, table);
            } else if(auto *dic = dynamic_cast<const Dictionary *>(obj)) {
                saveStructBinaryDictionary(out, dic, stack, table);
            } else {
                putU8(out, TYPE_NIL); // 其他对象 → null（原版同）
            }
        }

        // ---------- 二进制读入 ----------

        struct BinReader {
            VM &vm;
            const u8 *data;
            size_t size;
            size_t pos = 0;

            void need(const size_t n) const {
                if(pos + n > size)
                    serError("binary serialize read error");
            }

            u8 r8() {
                need(1);
                return data[pos++];
            }

            u16 r16() {
                need(2);
                const u16 v = static_cast<u16>(data[pos] | (data[pos + 1] << 8));
                pos += 2;
                return v;
            }

            u32 r32() {
                need(4);
                u32 v = 0;
                for(int i = 3; i >= 0; --i)
                    v = (v << 8) | data[pos + i];
                pos += 4;
                return v;
            }

            u64 r64() {
                need(8);
                u64 v = 0;
                for(int i = 7; i >= 0; --i)
                    v = (v << 8) | data[pos + i];
                pos += 8;
                return v;
            }

            std::string readString(const size_t units) {
                need(units * 2);
                std::string s = utf16LeToUtf8(data + pos, units * 2);
                pos += units * 2;
                return s;
            }

            Value readValue(Array *rootArr, Dictionary *rootDic) {
                const u8 type = r8();
                if(type <= TYPE_POSITIVE_FIX_NUM_MAX)
                    return Value{ Integer{ type } };
                if(type >= TYPE_NEGATIVE_FIX_NUM_MIN)
                    return Value{ Integer{ static_cast<i8>(type) } };
                if(type >= TYPE_FIX_STRING_MIN && type <= TYPE_FIX_STRING_MAX)
                    return Value{ makeString(readString(type - TYPE_FIX_STRING_MIN)) };
                if(type >= TYPE_FIX_ARRAY_MIN && type <= TYPE_FIX_ARRAY_MAX)
                    return readArray(rootArr, type - TYPE_FIX_ARRAY_MIN);
                if(type >= TYPE_FIX_MAP_MIN && type <= TYPE_FIX_MAP_MAX)
                    return readDictionary(rootDic, type - TYPE_FIX_MAP_MIN);
                if(type >= TYPE_FIX_RAW_MIN && type <= TYPE_FIX_RAW_MAX)
                    return readOctet(type - TYPE_FIX_RAW_MIN);
                switch(type) {
                    case TYPE_NIL:
                    case TYPE_VOID:
                        return Value{};
                    case TYPE_TRUE:
                        return Value{ Integer{ 1 } };
                    case TYPE_FALSE:
                        return Value{ Integer{ 0 } };
                    case TYPE_STRING8: {
                        const size_t len = r8();
                        return Value{ makeString(readString(len)) };
                    }
                    case TYPE_STRING16: {
                        const size_t len = r16();
                        return Value{ makeString(readString(len)) };
                    }
                    case TYPE_STRING32: {
                        const size_t len = r32();
                        return Value{ makeString(readString(len)) };
                    }
                    case TYPE_FLOAT: {
                        const u32 t = r32();
                        const float f = std::bit_cast<float>(t);
                        return Value{ Real{ static_cast<double>(f) } };
                    }
                    case TYPE_DOUBLE: {
                        const u64 t = r64();
                        const double d = std::bit_cast<double>(t);
                        return Value{ Real{ d } };
                    }
                    case TYPE_UINT8:
                        return Value{ Integer{ r8() } };
                    case TYPE_UINT16:
                        return Value{ Integer{ r16() } };
                    case TYPE_UINT32:
                        return Value{ Integer{ r32() } };
                    case TYPE_UINT64:
                        return Value{ Integer{ static_cast<i64>(r64()) } };
                    case TYPE_INT8:
                        return Value{ Integer{ static_cast<i8>(r8()) } };
                    case TYPE_INT16:
                        return Value{ Integer{ static_cast<i16>(r16()) } };
                    case TYPE_INT32:
                        return Value{ Integer{ static_cast<i32>(r32()) } };
                    case TYPE_INT64:
                        return Value{ Integer{ static_cast<i64>(r64()) } };
                    case TYPE_RAW16: {
                        const size_t len = r16();
                        return readOctet(len);
                    }
                    case TYPE_RAW32: {
                        const size_t len = r32();
                        return readOctet(len);
                    }
                    case TYPE_ARRAY16: {
                        const size_t count = r16();
                        return readArray(rootArr, count);
                    }
                    case TYPE_ARRAY32: {
                        const size_t count = r32();
                        return readArray(rootArr, count);
                    }
                    case TYPE_MAP16: {
                        const size_t count = r16();
                        return readDictionary(rootDic, count);
                    }
                    case TYPE_MAP32: {
                        const size_t count = r32();
                        return readDictionary(rootDic, count);
                    }
                    default:
                        serError("binary serialize read error");
                }
            }

            Value readOctet(const size_t len) {
                need(len);
                Value v{ Octet(data + pos, static_cast<u32>(len)) };
                pos += len;
                return v;
            }

            Value readArray(Array *rootArr, const size_t count) {
                Array *arr = rootArr;
                Opt<RootGuard<Array>> guard;
                if(arr) {
                    rootArr = nullptr; // 根只消费一次
                    arr->elements.clear();
                } else {
                    arr = vm.create<Array>(&vm.rt());
                    guard.emplace(&vm.rt(), arr);
                }
                for(size_t i = 0; i < count; ++i)
                    arr->elements.push_back(readValue(nullptr, nullptr));
                return Value{ arr };
            }

            Value readDictionary(Dictionary *rootDic, const size_t count) {
                Dictionary *dic = rootDic;
                Opt<RootGuard<Dictionary>> guard;
                if(dic) {
                    rootDic = nullptr;
                    dic->members.clear();
                } else {
                    dic = vm.create<Dictionary>();
                    guard.emplace(&vm.rt(), dic);
                }
                for(size_t i = 0; i < count; ++i) {
                    const u8 kt = r8();
                    std::string key;
                    if(kt >= TYPE_FIX_STRING_MIN && kt <= TYPE_FIX_STRING_MAX)
                        key = readString(kt - TYPE_FIX_STRING_MIN);
                    else if(kt == TYPE_STRING8)
                        key = readString(r8());
                    else if(kt == TYPE_STRING16)
                        key = readString(r16());
                    else if(kt == TYPE_STRING32)
                        key = readString(r32());
                    else
                        serError("binary serialize read error");
                    Value value = readValue(nullptr, nullptr);
                    dic->members[vm.rt().atomTable.intern(makeString(key))] = value;
                }
                return Value{ dic };
            }
        };

        // 结构化二进制主体（saveStruct 的 'b' 形态 / loadStruct 的二进制形态）
        bool isBinaryHeader(const std::string &raw) {
            return raw.size() >= sizeof(SER_HEADER) && std::memcmp(raw.data(), SER_HEADER, sizeof(SER_HEADER)) == 0;
        }

        // ---------- 结构化文本读入（自包含递归下降，无 VM 重入）----------
        // 语法即 saveStruct 的产出：(const) %[ "k" => v, ... ] / (const) [ v, ... ]，
        // 标量：void / null / 整数 / 实数（含 0x1.XXXXpN 十六进制浮点，尾随
        // "/* ... */" 注释）/ "字符串"（EscapeC 转义）/ <% octet %>；嵌套容器递归。
        struct TextParser {
            VM &vm;
            const std::string &s;
            size_t i = 0;

            void ws() {
                while(i < s.size()) {
                    const char c = s[i];
                    if(c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                        ++i;
                    } else if(c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
                        i += 2;
                        while(i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/'))
                            ++i;
                        i = std::min(i + 2, s.size());
                    } else {
                        break;
                    }
                }
            }

            bool lit(const char *t) {
                ws();
                const size_t n = std::strlen(t);
                if(s.compare(i, n, t) == 0) {
                    i += n;
                    return true;
                }
                return false;
            }

            [[noreturn]] void fail() { serError("cannot parse struct text"); }

            std::string parseString() {
                ws();
                if(i >= s.size() || s[i] != '"')
                    fail();
                ++i;
                std::string out;
                while(true) {
                    if(i >= s.size())
                        fail();
                    const char c = s[i++];
                    if(c == '"')
                        break;
                    if(c != '\\') {
                        out += c;
                        continue;
                    }
                    if(i >= s.size())
                        fail();
                    const char e = s[i++];
                    switch(e) {
                        case 'a':
                            out += '\a';
                            break;
                        case 'b':
                            out += '\b';
                            break;
                        case 'f':
                            out += '\f';
                            break;
                        case 'n':
                            out += '\n';
                            break;
                        case 'r':
                            out += '\r';
                            break;
                        case 't':
                            out += '\t';
                            break;
                        case 'v':
                            out += '\v';
                            break;
                        case '\\':
                            out += '\\';
                            break;
                        case '\'':
                            out += '\'';
                            break;
                        case '"':
                            out += '"';
                            break;
                        case 'x': {
                            if(i + 1 < s.size() && isHexDigitChar(s[i])) {
                                char buf[3] = { s[i], s[i + 1], 0 };
                                i += 2;
                                appendUtf8(out, static_cast<char32_t>(std::strtoul(buf, nullptr, 16)));
                            } else {
                                fail();
                            }
                            break;
                        }
                        default:
                            fail();
                    }
                }
                return out;
            }

            // [-]0x1.<hex>*p<exp> 十六进制浮点（TJS2 TJSRealToHexString 形态）
            double parseHexFloat(const std::string &t) {
                size_t p = 0;
                bool neg = false;
                if(p < t.size() && t[p] == '-') {
                    neg = true;
                    ++p;
                }
                if(t.compare(p, 2, "0x") != 0)
                    return 0.0;
                p += 2;
                double mant = 0.0;
                double scale = 1.0 / 16.0;
                bool dot = false;
                while(p < t.size()) {
                    const char c = t[p];
                    if(c == '.') {
                        dot = true;
                        ++p;
                        continue;
                    }
                    if(c == 'p' || c == 'P')
                        break;
                    int d;
                    if(c >= '0' && c <= '9')
                        d = c - '0';
                    else if(c >= 'a' && c <= 'f')
                        d = c - 'a' + 10;
                    else if(c >= 'A' && c <= 'F')
                        d = c - 'A' + 10;
                    else
                        break;
                    if(dot) {
                        mant += d * scale;
                        scale /= 16.0;
                    } else {
                        mant = mant * 16.0 + d;
                    }
                    ++p;
                }
                i32 exp = 0;
                if(p < t.size() && (t[p] == 'p' || t[p] == 'P'))
                    exp = static_cast<i32>(std::strtoll(t.c_str() + p + 1, nullptr, 10));
                const double v = mant * std::pow(2.0, static_cast<double>(exp));
                return neg ? -v : v;
            }

            Value parseValue();

            Value parseArray() {
                auto *arr = vm.create<Array>(&vm.rt());
                RootGuard<Array> guard{ &vm.rt(), arr };
                ws();
                if(lit("]"))
                    return Value{ arr };
                while(true) {
                    Value v = parseValue();
                    arr->elements.push_back(v);
                    if(lit(","))
                        continue;
                    if(lit("]"))
                        break;
                    fail();
                }
                return Value{ arr };
            }

            Value parseDictionary() {
                auto *dic = vm.create<Dictionary>();
                RootGuard<Dictionary> guard{ &vm.rt(), dic };
                ws();
                if(lit("]"))
                    return Value{ dic };
                while(true) {
                    const std::string key = parseString();
                    // "=>" 与 ">=" 同义（TJS2 词法怪癖：=> 词形化为 >=）
                    if(!lit("=>") && !lit(">="))
                        fail();
                    Value v = parseValue();
                    dic->members[vm.rt().atomTable.intern(makeString(key))] = v;
                    if(lit(","))
                        continue;
                    if(lit("]"))
                        break;
                    fail();
                }
                return Value{ dic };
            }

            Value parseNumber() {
                ws();
                size_t j = i;
                while(j < s.size() &&
                      (std::isxdigit(static_cast<unsigned char>(s[j])) || s[j] == '.' || s[j] == 'p' || s[j] == 'P' ||
                       s[j] == 'x' || s[j] == 'X' ||
                       ((s[j] == '-' || s[j] == '+') && (j == i || s[j - 1] == 'p' || s[j - 1] == 'P'))))
                    ++j;
                const std::string t = s.substr(i, j - i);
                i = j;
                if(t.find('.') != std::string::npos ||
                   ((t.find("0x") != std::string::npos || t.find("0X") != std::string::npos) &&
                    (t.find('p') != std::string::npos || t.find('P') != std::string::npos)))
                    return Value{ Real{ parseHexFloat(t) } };
                if(t.compare(0, 2, "0x") == 0 || t.compare(0, 2, "0X") == 0)
                    return Value{ Integer{ static_cast<i64>(std::strtoll(t.c_str(), nullptr, 16)) } };
                if(t.find('.') != std::string::npos || t.find('e') != std::string::npos ||
                   t.find('E') != std::string::npos)
                    return Value{ Real{ std::strtod(t.c_str(), nullptr) } };
                return Value{ Integer{ static_cast<i64>(std::strtoll(t.c_str(), nullptr, 10)) } };
            }
        };

        Value TextParser::parseValue() {
            ws();
            if(i >= s.size())
                fail();
            if(s[i] == '"')
                return Value{ makeString(parseString()) };
            if(lit("(const)"))
                return parseValue();
            if(lit("%["))
                return parseDictionary();
            if(lit("["))
                return parseArray();
            if(lit("<%")) {
                std::vector<u8> bytes;
                ws();
                while(i < s.size() && s[i] != '%') {
                    while(i < s.size() && s[i] == ' ')
                        ++i;
                    if(i + 3 < s.size() && s[i] == '0' && s[i + 1] == 'x') {
                        char buf[3] = { s[i + 2], s[i + 3], 0 };
                        bytes.push_back(static_cast<u8>(std::strtoul(buf, nullptr, 16)));
                        i += 4;
                        ws();
                    } else {
                        break;
                    }
                }
                if(!lit("%>"))
                    fail();
                if(bytes.empty())
                    return Value{};
                return Value{ Octet(bytes.data(), static_cast<u32>(bytes.size())) };
            }
            if(lit("null"))
                return Value{};
            if(lit("void"))
                return Value{};
            if(std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '-' || s[i] == '+') {
                Value v = parseNumber();
                ws();
                // 实数尾随 "/* 小数 */" 注释
                if(i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
                    i += 2;
                    while(i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/'))
                        ++i;
                    i = std::min(i + 2, s.size());
                }
                return v;
            }
            fail();
        }

        // ---------- pack / unpack（tjsOctPack 对齐）----------

        enum class OctPackType {
            ascii,
            ASCII,
            bitstring,
            BITSTRING,
            charr,
            CHAR,
            dbl,
            flt,
            hex,
            HEX,
            i32t,
            U32t,
            longt,
            LONGt,
            noshort,
            NOLONG,
            pointer,
            POINTER,
            shortt,
            SHORTt,
            leshort,
            LELONG,
            uuencode,
            BRE,
            null,
            NULL_,
            fill,
            base64,
            EOT,
        };

        constexpr char OctPackChar[] = {
            'a', 'A', 'b', 'B', 'c', 'C', 'd', 'f', 'h', 'H', 'i', 'I', 'l', 'L',
            'n', 'N', 'p', 'P', 's', 'S', 'v', 'V', 'u', 'w', 'x', 'X', '@', 'm',
        };

        struct OctPackTemplate {
            OctPackType type;
            i32 length;
        };

        std::vector<OctPackTemplate> parsePackTemplate(const std::string &templ) {
            std::vector<OctPackTemplate> result;
            size_t i = 0;
            while(i < templ.size()) {
                OctPackType t = OctPackType::EOT;
                for(int k = 0; k < static_cast<int>(OctPackType::EOT); ++k) {
                    if(templ[i] == OctPackChar[k]) {
                        t = static_cast<OctPackType>(k);
                        break;
                    }
                }
                if(t == OctPackType::EOT)
                    serError("unknown pack/unpack template character");
                ++i;
                OctPackTemplate entry{ t, 1 };
                if(i < templ.size()) {
                    if(templ[i] == '*') {
                        entry.length = -1;
                        ++i;
                    } else if(templ[i] >= '0' && templ[i] <= '9') {
                        i32 num = 0;
                        while(i < templ.size() && templ[i] >= '0' && templ[i] <= '9') {
                            num = num * 10 + (templ[i] - '0');
                            ++i;
                        }
                        entry.length = num;
                    }
                }
                result.push_back(entry);
            }
            return result;
        }

        void asciiToBin(std::vector<u8> &bin, const std::string &arg, const u8 fill, i32 len) {
            if(len < 0)
                len = static_cast<i32>(arg.size());
            i32 i = 0;
            size_t p = 0;
            for(; i < len && p < arg.size(); ++i, ++p)
                bin.push_back(static_cast<u8>(arg[p]));
            for(; i < len; ++i)
                bin.push_back(fill);
        }

        // mtol: true = 高位在前；'0'/'1' 之外报错（原版同）
        void bitStringToBin(std::vector<u8> &bin, const std::string &arg, const bool mtol, i32 len) {
            if(len < 0)
                len = static_cast<i32>(arg.size());
            u8 val = 0;
            i32 pos = 0;
            size_t p = 0;
            i32 i = 0;
            if(mtol) {
                pos = 7;
                for(; i < len && p < arg.size(); ++i, ++p) {
                    if(arg[p] == '0') {
                        // val |= 0
                    } else if(arg[p] == '1') {
                        val |= static_cast<u8>(1 << pos);
                    } else {
                        serError("unknown bit string character");
                    }
                    if(pos == 0) {
                        bin.push_back(val);
                        pos = 7;
                        val = 0;
                    } else {
                        --pos;
                    }
                }
                if(pos < 7)
                    bin.push_back(val);
            } else {
                for(; i < len && p < arg.size(); ++i, ++p) {
                    if(arg[p] == '0') {
                        // val |= 0
                    } else if(arg[p] == '1') {
                        val |= static_cast<u8>(1 << pos);
                    } else {
                        serError("unknown bit string character");
                    }
                    if(pos == 7) {
                        bin.push_back(val);
                        pos = val = 0;
                    } else {
                        ++pos;
                    }
                }
                if(pos)
                    bin.push_back(val);
            }
        }

        // h: 低位 nibble 在前；H: 高位在前。原版大写十六进制只收 'A'-'E'
        // （'F' 被漏写，tjsOctPack.cpp HexToBin），按特性同步
        void hexToBin(std::vector<u8> &bin, const std::string &arg, const bool mtol, i32 len) {
            if(len < 0)
                len = static_cast<i32>(arg.size());
            u8 val = 0;
            i32 pos = 0;
            size_t p = 0;
            i32 i = 0;
            if(mtol) {
                pos = 1;
                for(; i < len && p < arg.size(); ++i, ++p) {
                    const char c = arg[p];
                    if(c >= '0' && c <= '9')
                        val |= static_cast<u8>((c - '0') << (pos * 4));
                    else if(c >= 'a' && c <= 'f')
                        val |= static_cast<u8>((c - 'a' + 10) << (pos * 4));
                    else if(c >= 'A' && c <= 'E')
                        val |= static_cast<u8>((c - 'A' + 10) << (pos * 4));
                    else
                        serError("unknown hex string character");
                    if(pos == 0) {
                        bin.push_back(val);
                        pos = 1;
                        val = 0;
                    } else {
                        --pos;
                    }
                }
                if(pos < 1)
                    bin.push_back(val);
            } else {
                for(; i < len && p < arg.size(); ++i, ++p) {
                    const char c = arg[p];
                    if(c >= '0' && c <= '9')
                        val |= static_cast<u8>((c - '0') << (pos * 4));
                    else if(c >= 'a' && c <= 'f')
                        val |= static_cast<u8>((c - 'a' + 10) << (pos * 4));
                    else if(c >= 'A' && c <= 'E')
                        val |= static_cast<u8>((c - 'A' + 10) << (pos * 4));
                    else
                        serError("unknown hex string character");
                    if(pos) {
                        bin.push_back(val);
                        pos = val = 0;
                    } else {
                        ++pos;
                    }
                }
                if(pos)
                    bin.push_back(val);
            }
        }

        template <typename TRet, typename TTmp, int NBYTE, typename TRetTmp>
        void readNumberLE(std::vector<u8> &result, const Value *args, const size_t numargs, size_t &argindex, i32 len) {
            if(len < 0)
                len = static_cast<i32>(numargs - argindex);
            if(static_cast<size_t>(len) + argindex > numargs)
                len = static_cast<i32>(numargs - argindex);
            for(i32 a = 0; a < len; ++a) {
                const TRet c = static_cast<TRet>(static_cast<i64>(coerceInt(args[argindex + a])));
                TRetTmp val{};
                std::memcpy(&val, &c, sizeof(TRet));
                for(int i = 0; i < NBYTE; ++i)
                    result.push_back(static_cast<u8>((val >> (i * 8)) & 0xFF));
            }
            argindex += static_cast<size_t>(len - 1);
        }

        template <typename TRet, typename TTmp, int NBYTE, typename TRetTmp>
        void readNumberBE(std::vector<u8> &result, const Value *args, const size_t numargs, size_t &argindex, i32 len) {
            if(len < 0)
                len = static_cast<i32>(numargs - argindex);
            if(static_cast<size_t>(len) + argindex > numargs)
                len = static_cast<i32>(numargs - argindex);
            for(i32 a = 0; a < len; ++a) {
                const TRet c = static_cast<TRet>(static_cast<i64>(coerceInt(args[argindex + a])));
                TRetTmp val{};
                std::memcpy(&val, &c, sizeof(TRet));
                for(int i = 0; i < NBYTE; ++i)
                    result.push_back(static_cast<u8>((val >> ((NBYTE - 1 - i) * 8)) & 0xFF));
            }
            argindex += static_cast<size_t>(len - 1);
        }

        // 浮点写出（double 实参；f 单精度由调用方截断）
        template <typename TRet, int NBYTE, typename TBits>
        void readRealLE(std::vector<u8> &result, const Value *args, const size_t numargs, size_t &argindex,
                        const i32 len) {
            i32 l = len;
            if(l < 0)
                l = static_cast<i32>(numargs - argindex);
            if(static_cast<size_t>(l) + argindex > numargs)
                l = static_cast<i32>(numargs - argindex);
            for(i32 a = 0; a < l; ++a) {
                const TRet c = static_cast<TRet>(coerceReal(args[argindex + a]));
                TBits val{};
                std::memcpy(&val, &c, sizeof(TRet));
                for(int i = 0; i < NBYTE; ++i)
                    result.push_back(static_cast<u8>((val >> (i * 8)) & 0xFF));
            }
            argindex += static_cast<size_t>(l - 1);
        }

        constexpr char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        void encodeBase64(const u8 *inbuf, const size_t insize, std::string &out) {
            size_t i = 0;
            const size_t insize_3 = insize >= 3 ? insize - 3 : 0;
            for(; insize >= 3 && i <= insize_3; i += 3) {
                out += B64[(inbuf[i] >> 2) & 0x3F];
                out += B64[((inbuf[i] << 4) & 0x30) | ((inbuf[i + 1] >> 4) & 0x0F)];
                out += B64[((inbuf[i + 1] << 2) & 0x3C) | ((inbuf[i + 2] >> 6) & 0x03)];
                out += B64[inbuf[i + 2] & 0x3F];
            }
            if(const size_t rem = insize - i; rem == 2 && insize >= 2) {
                out += B64[(inbuf[i] >> 2) & 0x3F];
                out += B64[((inbuf[i] << 4) & 0x30) | ((inbuf[i + 1] >> 4) & 0x0F)];
                out += B64[(inbuf[i + 1] << 2) & 0x3C];
                out += '=';
            } else if(rem == 1) {
                out += B64[(inbuf[i] >> 2) & 0x3F];
                out += B64[(inbuf[i] << 4) & 0x30];
                out += '=';
                out += '=';
            }
        }

        // 原版 decodeBase64 对齐（<4 字符直接返回空；'=' 终止）
        void decodeBase64(const std::string &in, std::vector<u8> &out) {
            const i32 len = static_cast<i32>(in.size());
            if(len < 4)
                return;
            auto b64num = [](const char c) -> i32 {
                if(c >= 'A' && c <= 'Z')
                    return c - 'A';
                if(c >= 'a' && c <= 'z')
                    return c - 'a' + 26;
                if(c >= '0' && c <= '9')
                    return c - '0' + 52;
                if(c == '+')
                    return 62;
                if(c == '/')
                    return 63;
                return 0;
            };
            i32 dptr = 0;
            const i32 len_4 = len - 4;
            while(dptr < len_4) {
                out.push_back(static_cast<u8>((b64num(in[dptr]) << 2) | (b64num(in[dptr + 1]) >> 4)));
                ++dptr;
                out.push_back(static_cast<u8>((b64num(in[dptr]) << 4) | (b64num(in[dptr + 1]) >> 2)));
                ++dptr;
                out.push_back(static_cast<u8>((b64num(in[dptr]) << 6) | b64num(in[dptr + 1])));
                dptr += 2;
            }
            out.push_back(static_cast<u8>((b64num(in[dptr]) << 2) | (b64num(in[dptr + 1]) >> 4)));
            ++dptr;
            u8 tmp = static_cast<u8>(b64num(in[dptr++]) << 4);
            if(in[dptr] != '=') {
                tmp |= static_cast<u8>(b64num(in[dptr]) >> 2);
                out.push_back(tmp);
                tmp = static_cast<u8>(b64num(in[dptr++]) << 6);
                if(in[dptr] != '=') {
                    tmp |= static_cast<u8>(b64num(in[dptr]));
                    out.push_back(tmp);
                }
            }
        }

        Value packRun(const std::vector<OctPackTemplate> &templ, const Value *args, const size_t numargs, VM &vm) {
            std::vector<u8> result;
            const size_t count = templ.size();
            size_t argindex = 0;
            for(size_t i = 0; i < count && argindex < numargs; ++argindex) {
                const OctPackType t = templ[i].type;
                const i32 len = templ[i].length;
                switch(t) {
                    case OctPackType::ascii:
                        asciiToBin(result, valueArgToString(args[argindex]), '\0', len);
                        break;
                    case OctPackType::ASCII:
                        asciiToBin(result, valueArgToString(args[argindex]), ' ', len);
                        break;
                    case OctPackType::bitstring:
                        bitStringToBin(result, valueArgToString(args[argindex]), false, len);
                        break;
                    case OctPackType::BITSTRING:
                        bitStringToBin(result, valueArgToString(args[argindex]), true, len);
                        break;
                    case OctPackType::charr:
                        readNumberLE<i8, Integer, 1, i8>(result, args, numargs, argindex, len);
                        break;
                    case OctPackType::CHAR:
                        readNumberLE<u8, Integer, 1, u8>(result, args, numargs, argindex, len);
                        break;
                    case OctPackType::dbl:
                        readRealLE<double, 8, u64>(result, args, numargs, argindex, len);
                        break;
                    case OctPackType::flt: {
                        // 单精度：double 实参截断为 float 再按 4 字节 LE 写出
                        i32 l = len;
                        if(l < 0)
                            l = static_cast<i32>(numargs - argindex);
                        if(static_cast<size_t>(l) + argindex > numargs)
                            l = static_cast<i32>(numargs - argindex);
                        for(i32 a = 0; a < l; ++a) {
                            const float f = static_cast<float>(coerceReal(args[argindex + a]));
                            const u32 bits = std::bit_cast<u32>(f);
                            for(int b = 0; b < 4; ++b)
                                result.push_back(static_cast<u8>((bits >> (b * 8)) & 0xFF));
                        }
                        argindex += static_cast<size_t>(l - 1);
                        break;
                    }
                    case OctPackType::hex:
                        hexToBin(result, valueArgToString(args[argindex]), false, len);
                        break;
                    case OctPackType::HEX:
                        hexToBin(result, valueArgToString(args[argindex]), true, len);
                        break;
                    case OctPackType::i32t:
                    case OctPackType::longt:
                        readNumberLE<i32, Integer, 4, u32>(result, args, numargs, argindex, len);
                        break;
                    case OctPackType::U32t:
                    case OctPackType::LONGt:
                        readNumberLE<u32, Integer, 4, u32>(result, args, numargs, argindex, len);
                        break;
                    case OctPackType::noshort:
                        readNumberBE<u16, Integer, 2, u16>(result, args, numargs, argindex, len);
                        break;
                    case OctPackType::NOLONG:
                        readNumberBE<u32, Integer, 4, u32>(result, args, numargs, argindex, len);
                        break;
                    case OctPackType::pointer:
                    case OctPackType::POINTER:
                        break; // 原版 TODO：不写任何字节（照常消耗实参）
                    case OctPackType::shortt:
                        readNumberLE<i16, Integer, 2, u16>(result, args, numargs, argindex, len);
                        break;
                    case OctPackType::SHORTt:
                        readNumberLE<u16, Integer, 2, u16>(result, args, numargs, argindex, len);
                        break;
                    case OctPackType::leshort:
                        readNumberLE<u16, Integer, 2, u16>(result, args, numargs, argindex, len);
                        break;
                    case OctPackType::LELONG:
                        readNumberLE<u32, Integer, 4, u32>(result, args, numargs, argindex, len);
                        break;
                    case OctPackType::uuencode:
                        serError("uuencode is not supported");
                    case OctPackType::BRE:
                        serError("BER compression is not supported");
                    case OctPackType::null:
                        for(i32 a = 0; a < len; ++a)
                            result.push_back(0);
                        --argindex; // 不消耗实参（原版同）
                        break;
                    case OctPackType::NULL_:
                        for(i32 a = 0; a < len; ++a) {
                            if(!result.empty())
                                result.pop_back();
                        }
                        --argindex;
                        break;
                    case OctPackType::fill: {
                        const size_t c = result.size();
                        for(size_t k = c; k < static_cast<size_t>(len < 0 ? c : len); ++k)
                            result.push_back(0);
                        --argindex;
                        break;
                    }
                    case OctPackType::base64:
                        decodeBase64(valueArgToString(args[argindex]), result);
                        break;
                    case OctPackType::EOT:
                        break;
                }
                if(len >= 0)
                    ++i; // '*' 时停留同一模板项吃尽剩余实参
            }
            if(result.empty())
                return Value{};
            return Value{ Octet(result.data(), static_cast<u32>(result.size())) };
        }

        // unpack 各数值形态（LE/BE + 尾部截断 + 残组补推）
        template <typename TRet, int NBYTE>
        void binToNumberLE(std::vector<TRet> &result, const u8 *&cur, const u8 *tail, i32 len) {
            if(len < 0)
                len = static_cast<i32>((tail - cur + NBYTE - 1) / NBYTE);
            if(cur + static_cast<size_t>(len) * NBYTE < tail)
                tail = cur + static_cast<size_t>(len) * NBYTE;
            TRet val{};
            u32 bytes = 0;
            for(; cur < tail; ++cur) {
                val |= static_cast<TRet>(*cur << (bytes * 8));
                if(bytes >= static_cast<u32>(NBYTE - 1)) {
                    bytes = 0;
                    result.push_back(val);
                    val = {};
                } else {
                    ++bytes;
                }
            }
            if(bytes)
                result.push_back(val);
        }

        template <typename TRet, int NBYTE>
        void binToNumberBE(std::vector<TRet> &result, const u8 *&cur, const u8 *tail, i32 len) {
            if(len < 0)
                len = static_cast<i32>((tail - cur + NBYTE - 1) / NBYTE);
            if(cur + static_cast<size_t>(len) * NBYTE < tail)
                tail = cur + static_cast<size_t>(len) * NBYTE;
            TRet val{};
            u32 bytes = NBYTE - 1;
            for(; cur < tail; ++cur) {
                val |= static_cast<TRet>(*cur << (bytes * 8));
                if(bytes == 0) {
                    bytes = NBYTE - 1;
                    result.push_back(val);
                    val = {};
                } else {
                    --bytes;
                }
            }
            if(bytes < static_cast<u32>(NBYTE - 1))
                result.push_back(val);
        }

        template <typename TRet, int NBYTE, typename TBits>
        void binToRealLE(std::vector<TRet> &result, const u8 *&cur, const u8 *tail, i32 len) {
            const size_t available = static_cast<size_t>(tail - cur);
            // 先限制字节数，再形成指针，截断输入也不会越过缓冲区末尾。
            if(len >= 0 && static_cast<size_t>(len) <= available / NBYTE)
                tail = cur + static_cast<size_t>(len) * NBYTE;
            TBits val = 0;
            u32 bytes = 0;
            for(; cur < tail; ++cur) {
                val |= static_cast<TBits>(*cur) << (bytes * 8);
                if(bytes >= static_cast<u32>(NBYTE - 1)) {
                    bytes = 0;
                    TRet r{};
                    std::memcpy(&r, &val, sizeof(TRet));
                    result.push_back(r);
                    val = 0;
                } else {
                    ++bytes;
                }
            }
            if(bytes) {
                TRet r{};
                std::memcpy(&r, &val, sizeof(TRet));
                result.push_back(r);
            }
        }

        Value unpackRun(const std::vector<OctPackTemplate> &templ, VM &vm, const Octet *oct) {
            auto *result = vm.create<Array>(&vm.rt());
            const u8 *data = oct->getData();
            const u8 *tail = data + oct->getSize();
            const u8 *cur = data;
            const size_t count = templ.size();
            size_t i = 0;
            while(i < count && cur < tail) {
                const OctPackType t = templ[i].type;
                i32 len = templ[i].length;
                switch(t) {
                    case OctPackType::ascii:
                    case OctPackType::ASCII: {
                        if(len < 0)
                            len = static_cast<i32>(tail - cur);
                        std::string out;
                        const u8 *p = cur;
                        for(i32 k = 0; k < len && p < tail; ++k, ++p)
                            if(*p != '\0')
                                appendUtf8(out, *p);
                        result->elements.push_back(Value{ makeString(out) });
                        cur += len;
                        break;
                    }
                    case OctPackType::bitstring:
                    case OctPackType::BITSTRING: {
                        if(len < 0)
                            len = static_cast<i32>(tail - cur) * 8;
                        const bool mtol = t == OctPackType::BITSTRING;
                        std::string out;
                        const u8 *p = cur;
                        i32 pos = 0;
                        if(mtol) {
                            for(; p < tail; ++p) {
                                for(int b = 0; b < 8 && pos < len; ++b, ++pos)
                                    out += (*p & (0x01 << (7 - b))) ? '1' : '0';
                                if(pos >= len)
                                    break;
                            }
                        } else {
                            for(; p < tail; ++p) {
                                for(int b = 0; b < 8 && pos < len; ++b, ++pos)
                                    out += (*p & (0x01 << b)) ? '1' : '0';
                                if(pos >= len)
                                    break;
                            }
                        }
                        result->elements.push_back(Value{ makeString(out) });
                        cur += (len + 7) / 8;
                        break;
                    }
                    case OctPackType::charr: {
                        if(len < 0)
                            len = static_cast<i32>(tail - cur);
                        std::vector<i8> vals;
                        binToNumberLE<i8, 1>(vals, cur, tail, len);
                        for(const i8 v : vals)
                            result->elements.push_back(Value{ Integer{ v } });
                        break;
                    }
                    case OctPackType::CHAR: {
                        if(len < 0)
                            len = static_cast<i32>(tail - cur);
                        std::vector<u8> vals;
                        binToNumberLE<u8, 1>(vals, cur, tail, len);
                        for(const u8 v : vals)
                            result->elements.push_back(Value{ Integer{ v } });
                        break;
                    }
                    case OctPackType::dbl: {
                        if(len < 0)
                            len = static_cast<i32>((tail - cur) / 8);
                        std::vector<double> vals;
                        binToRealLE<double, 8, u64>(vals, cur, tail, len);
                        for(const double v : vals)
                            result->elements.push_back(Value{ Real{ v } });
                        break;
                    }
                    case OctPackType::flt: {
                        if(len < 0)
                            len = static_cast<i32>((tail - cur) / 4);
                        std::vector<float> vals;
                        binToRealLE<float, 4, u32>(vals, cur, tail, len);
                        for(const float v : vals)
                            result->elements.push_back(Value{ Real{ static_cast<double>(v) } });
                        break;
                    }
                    case OctPackType::hex:
                    case OctPackType::HEX: {
                        if(len < 0)
                            len = static_cast<i32>(tail - cur) * 2;
                        const bool mtol = t == OctPackType::HEX;
                        std::string out;
                        static constexpr char hex[] = "0123456789ABCDEF";
                        const u8 *p = cur;
                        const u8 *hTail = tail;
                        if(cur + static_cast<size_t>(len) < tail)
                            hTail = cur + (len + 1) / 2;
                        i32 pos = 0;
                        if(mtol) {
                            pos = 1;
                            for(i32 k = 0; k < len && p < hTail; ++k) {
                                const char ch = hex[(*p >> (pos * 4)) & 0xF];
                                out += ch;
                                if(pos == 0) {
                                    pos = 1;
                                    ++p;
                                } else {
                                    --pos;
                                }
                            }
                        } else {
                            for(i32 k = 0; k < len && p < hTail; ++k) {
                                const char ch = hex[(*p >> (pos * 4)) & 0xF];
                                out += ch;
                                if(pos) {
                                    pos = 0;
                                    ++p;
                                } else {
                                    ++pos;
                                }
                            }
                        }
                        result->elements.push_back(Value{ makeString(out) });
                        cur += (len + 1) / 2;
                        break;
                    }
                    case OctPackType::i32t:
                    case OctPackType::longt: {
                        if(len < 0)
                            len = static_cast<i32>((tail - cur) / 4);
                        std::vector<i32> vals;
                        binToNumberLE<i32, 4>(vals, cur, tail, len);
                        for(const i32 v : vals)
                            result->elements.push_back(Value{ Integer{ v } });
                        break;
                    }
                    case OctPackType::U32t:
                    case OctPackType::LONGt: {
                        if(len < 0)
                            len = static_cast<i32>((tail - cur) / 4);
                        std::vector<u32> vals;
                        binToNumberLE<u32, 4>(vals, cur, tail, len);
                        for(const u32 v : vals)
                            result->elements.push_back(Value{ Integer{ static_cast<i64>(v) } });
                        break;
                    }
                    case OctPackType::noshort: {
                        if(len < 0)
                            len = static_cast<i32>((tail - cur) / 2);
                        std::vector<u16> vals;
                        binToNumberBE<u16, 2>(vals, cur, tail, len);
                        for(const u16 v : vals)
                            result->elements.push_back(Value{ Integer{ v } });
                        break;
                    }
                    case OctPackType::NOLONG: {
                        if(len < 0)
                            len = static_cast<i32>((tail - cur) / 4);
                        std::vector<u32> vals;
                        binToNumberBE<u32, 4>(vals, cur, tail, len);
                        for(const u32 v : vals)
                            result->elements.push_back(Value{ Integer{ static_cast<i64>(v) } });
                        break;
                    }
                    case OctPackType::pointer:
                        serError("unpack 'p' is not supported");
                    case OctPackType::POINTER:
                        serError("unpack 'P' is not supported");
                    case OctPackType::shortt: {
                        if(len < 0)
                            len = static_cast<i32>((tail - cur) / 2);
                        std::vector<i16> vals;
                        binToNumberLE<i16, 2>(vals, cur, tail, len);
                        for(const i16 v : vals)
                            result->elements.push_back(Value{ Integer{ v } });
                        break;
                    }
                    case OctPackType::SHORTt:
                    case OctPackType::leshort: {
                        if(len < 0)
                            len = static_cast<i32>((tail - cur) / 2);
                        std::vector<u16> vals;
                        binToNumberLE<u16, 2>(vals, cur, tail, len);
                        for(const u16 v : vals)
                            result->elements.push_back(Value{ Integer{ v } });
                        break;
                    }
                    case OctPackType::LELONG: {
                        if(len < 0)
                            len = static_cast<i32>((tail - cur) / 4);
                        std::vector<u32> vals;
                        binToNumberLE<u32, 4>(vals, cur, tail, len);
                        for(const u32 v : vals)
                            result->elements.push_back(Value{ Integer{ static_cast<i64>(v) } });
                        break;
                    }
                    case OctPackType::uuencode:
                        serError("uuencode is not supported");
                    case OctPackType::BRE:
                        serError("BER compression is not supported");
                    case OctPackType::null: {
                        if(len < 0)
                            len = static_cast<i32>(tail - cur);
                        cur += len;
                        break;
                    }
                    case OctPackType::NULL_: {
                        if(len < 0)
                            len = static_cast<i32>(cur - data);
                        for(i32 k = 0; k < len; ++k) {
                            if(data != cur)
                                --cur;
                            else
                                break;
                        }
                        break;
                    }
                    case OctPackType::fill: {
                        if(len < 0)
                            len = static_cast<i32>(tail - cur);
                        cur = data + len;
                        break;
                    }
                    case OctPackType::base64: {
                        std::string out;
                        encodeBase64(cur, static_cast<size_t>(tail - cur), out);
                        result->elements.push_back(Value{ makeString(out) });
                        cur = tail;
                        break;
                    }
                    case OctPackType::EOT:
                        break;
                }
                ++i;
            }
            return Value{ result };
        }

        // ---------- assignStruct（结构化深拷贝，环 → null）----------

        void assignStructIntoDict(VM &vm, Dictionary *dst, const Dictionary *src, std::vector<const Object *> &stack);

        void assignStructIntoArray(VM &vm, Array *dst, const Array *src, std::vector<const Object *> &stack) {
            stack.push_back(src);
            dst->elements.clear();
            for(const auto &item : src->elements) {
                if(item.isObject()) {
                    auto *obj = item.asObject().unwrap();
                    if(stackContains(stack, obj)) {
                        dst->elements.push_back(Value{}); // 环 → null
                        continue;
                    }
                    if(auto *sarr = dynamic_cast<const Array *>(obj)) {
                        auto *narr = vm.create<Array>(&vm.rt());
                        dst->elements.push_back(Value{ narr });
                        assignStructIntoArray(vm, narr, sarr, stack);
                    } else if(auto *sdic = dynamic_cast<const Dictionary *>(obj)) {
                        auto *ndic = vm.create<Dictionary>();
                        dst->elements.push_back(Value{ ndic });
                        assignStructIntoDict(vm, ndic, sdic, stack);
                    } else {
                        dst->elements.push_back(item); // 其他对象按引用复制（原版同）
                    }
                } else {
                    dst->elements.push_back(item);
                }
            }
            stack.pop_back();
        }

        void assignStructIntoDict(VM &vm, Dictionary *dst, const Dictionary *src, std::vector<const Object *> &stack) {
            stack.push_back(src);
            dst->members.clear();
            for(const auto &[atom, value] : src->members) {
                if(value.isObject()) {
                    auto *obj = value.asObject().unwrap();
                    if(stackContains(stack, obj)) {
                        dst->members[atom] = Value{};
                        continue;
                    }
                    if(auto *sarr = dynamic_cast<const Array *>(obj)) {
                        auto *narr = vm.create<Array>(&vm.rt());
                        dst->members[atom] = Value{ narr };
                        assignStructIntoArray(vm, narr, sarr, stack);
                    } else if(auto *sdic = dynamic_cast<const Dictionary *>(obj)) {
                        auto *ndic = vm.create<Dictionary>();
                        dst->members[atom] = Value{ ndic };
                        assignStructIntoDict(vm, ndic, sdic, stack);
                    } else {
                        dst->members[atom] = value;
                    }
                } else {
                    dst->members[atom] = value;
                }
            }
            stack.pop_back();
        }

    } // namespace

    // ==================== Array ====================

    Value arrayLoad(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = dynamic_cast<Array *>(thisObj.asObject().unwrap());
        if(!arr)
            serError("Array.load expects an array");
        const std::string content = readTextDecoded(makeString(argString(argCount, args, 0)));

        // 逐行切分（\r\n|\r|\n；末尾残段非空才算一行）
        arr->elements.clear();
        size_t sp = 0;
        size_t p = 0;
        while(p < content.size()) {
            if(content[p] == '\r' || content[p] == '\n') {
                arr->elements.push_back(Value{ String(content.c_str() + sp, static_cast<u32>(p - sp)) });
                const char c = content[p++];
                if(c == '\r' && p < content.size() && content[p] == '\n')
                    ++p;
                sp = p;
                continue;
            }
            ++p;
        }
        if(const size_t l = content.size() - sp)
            arr->elements.push_back(Value{ String(content.c_str() + sp, static_cast<u32>(l)) });
        return thisObj;
    }

    Value arraySave(VM *, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = dynamic_cast<Array *>(thisObj.asObject().unwrap());
        if(!arr)
            serError("Array.save expects an array");
        std::string out;
        for(const auto &item : arr->elements) {
            if(item.isString() || item.isInteger() || item.isReal())
                out += valueArgToString(item);
            out += "\r\n";
        }
        writeFileBytes(makeString(argString(argCount, args, 0)), out);
        return thisObj;
    }

    Value arrayLoadStruct(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = dynamic_cast<Array *>(thisObj.asObject().unwrap());
        if(!arr)
            serError("Array.loadStruct expects an array");
        const std::string raw = readFileBytes(makeString(argString(argCount, args, 0)));
        if(!isBinaryHeader(raw))
            serError("Array.loadStruct expects a binary struct file");
        BinReader reader{ *vm, reinterpret_cast<const u8 *>(raw.data()) + sizeof(SER_HEADER),
                          raw.size() - sizeof(SER_HEADER) };
        reader.readValue(arr, nullptr);
        return thisObj;
    }

    Value arraySaveStruct(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = dynamic_cast<Array *>(thisObj.asObject().unwrap());
        if(!arr)
            serError("Array.saveStruct expects an array");
        const String name = makeString(argString(argCount, args, 0));
        if(modeHasFlag(argCount, args, 1, 'b')) {
            std::string out{ reinterpret_cast<const char *>(SER_HEADER), sizeof(SER_HEADER) };
            std::vector<const Object *> stack{ arr };
            saveStructBinaryArray(out, arr, stack, vm->rt().atomTable);
            writeFileBytes(name, out);
        } else {
            std::string out;
            std::vector<const Object *> stack{ arr };
            saveStructTextArray(out, arr, stack, {}, vm->rt().atomTable);
            writeFileBytes(name, out);
        }
        return thisObj;
    }

    Value arrayPack(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = dynamic_cast<Array *>(thisObj.asObject().unwrap());
        if(!arr)
            serError("Array.pack expects an array");
        if(argCount < 1 || !args[0].isString())
            serError("Array.pack expects a template string");
        const auto templ = parsePackTemplate(args[0].asString().unwrap()->toStdStr());
        return packRun(templ, arr->elements.data(), arr->elements.size(), *vm);
    }

    Value arrayAssignStruct(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        auto *arr = dynamic_cast<Array *>(thisObj.asObject().unwrap());
        if(!arr)
            serError("Array.assignStruct expects an array");
        if(argCount < 1 || !args[0].isObject())
            serError("Array.assignStruct expects an array or dictionary");
        auto *src = dynamic_cast<const Array *>(args[0].asObject().unwrap());
        if(!src)
            serError("Array.assignStruct expects an array");
        std::vector<const Object *> stack;
        assignStructIntoArray(*vm, arr, src, stack);
        return thisObj;
    }

    // ==================== Dictionary ====================

    void dictLoad(VM *, const Value &, size_t, Value *) {
        // 原版 Dictionary.load 为 TODO 空实现（tjsDictionary.cpp），按特性同步
    }

    void dictSave(VM *, const Value &, size_t, Value *) {
        // 原版 Dictionary.save 为 TODO 空实现（tjsDictionary.cpp），按特性同步
    }

    Value dictLoadStruct(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        auto *dic = dynamic_cast<Dictionary *>(thisObj.asObject().unwrap());
        if(!dic)
            serError("Dictionary.loadStruct expects a dictionary");
        dic->members.clear(); // 原版在读文件前即清空自身
        const std::string raw = readFileBytes(makeString(argString(argCount, args, 0)));
        if(isBinaryHeader(raw)) {
            BinReader reader{ *vm, reinterpret_cast<const u8 *>(raw.data()) + sizeof(SER_HEADER),
                              raw.size() - sizeof(SER_HEADER) };
            reader.readValue(nullptr, dic);
            return thisObj;
        }
        // 非二进制回落文本结构化：解析结果作为返回值（不写入自身——原版同款怪癖）。
        // 专用解析器而非表达式求值：evalExpr 与外层脚本执行共用 VMState 不可重入
        const std::string text = readTextDecoded(makeString(argString(argCount, args, 0)));
        TextParser parser{ *vm, text };
        return parser.parseValue();
    }

    Value dictSaveStruct(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        auto *dic = dynamic_cast<Dictionary *>(thisObj.asObject().unwrap());
        if(!dic)
            serError("Dictionary.saveStruct expects a dictionary");
        const String name = makeString(argString(argCount, args, 0));
        if(modeHasFlag(argCount, args, 1, 'b')) {
            std::string out{ reinterpret_cast<const char *>(SER_HEADER), sizeof(SER_HEADER) };
            std::vector<const Object *> stack{ dic };
            saveStructBinaryDictionary(out, dic, stack, vm->rt().atomTable);
            writeFileBytes(name, out);
        } else {
            std::string out;
            std::vector<const Object *> stack{ dic };
            saveStructTextDictionary(out, dic, stack, {}, vm->rt().atomTable);
            writeFileBytes(name, out);
        }
        return thisObj;
    }

    Value dictAssignStruct(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        auto *dic = dynamic_cast<Dictionary *>(thisObj.asObject().unwrap());
        if(!dic)
            serError("Dictionary.assignStruct expects a dictionary");
        if(argCount < 1 || !args[0].isObject())
            serError("Dictionary.assignStruct expects an array or dictionary");
        auto *src = dynamic_cast<const Dictionary *>(args[0].asObject().unwrap());
        if(!src)
            serError("Dictionary.assignStruct expects a dictionary");
        std::vector<const Object *> stack;
        assignStructIntoDict(*vm, dic, src, stack);
        return thisObj;
    }

    // ==================== Octet ====================

    Value octetUnpack(VM *vm, const Value &thisObj, const size_t argCount, Value *args) {
        if(!thisObj.isOctet())
            serError("octet.unpack expects an octet");
        if(argCount < 1 || !args[0].isString())
            serError("octet.unpack expects a template string");
        const auto templ = parsePackTemplate(args[0].asString().unwrap()->toStdStr());
        return unpackRun(templ, *vm, thisObj.asOctet().unwrap());
    }

} // namespace cial::stdlib
