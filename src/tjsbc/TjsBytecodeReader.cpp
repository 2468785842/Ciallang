// Copyright (c) 2026/10/26
//

#include "TjsBytecodeReader.hpp"

#include <cstring>
#include <fstream>

#include "common/UTF8.hpp"

namespace cial::tjsbc {

    namespace {

        // 文件魔数：'T','J','S','2','1','0','0',0（与 context 记录的 "TJS2" tag 同串前缀）
        constexpr u8 FILE_TAG[8] = {'T', 'J', 'S', '2', '1', '0', '0', 0};
        constexpr u8 DATA_TAG[4] = {'D', 'A', 'T', 'A'};
        constexpr u8 OBJ_TAG[4] = {'O', 'B', 'J', 'S'};
        constexpr u8 CODE_TAG[4] = {'T', 'J', 'S', '2'};

        [[noreturn]] void bcError(const char *msg) { throw std::runtime_error(msg); }

        struct ByteCursor {
            const u8 *data;
            size_t size;
            size_t pos = 0;

            void need(const size_t n) const {
                if(pos + n > size)
                    bcError("tjs bytecode: unexpected end of file");
            }

            u8 u8At() {
                need(1);
                return data[pos++];
            }

            u16 u16At() {
                need(2);
                const u16 v = static_cast<u16>(data[pos] | (data[pos + 1] << 8));
                pos += 2;
                return v;
            }

            i32 i32At() {
                need(4);
                i32 v = 0;
                for(int k = 3; k >= 0; --k)
                    v = (v << 8) | data[pos + k];
                pos += 4;
                return v;
            }

            i64 i64At() {
                need(8);
                i64 v = 0;
                for(int k = 7; k >= 0; --k)
                    v = (v << 8) | data[pos + k];
                pos += 8;
                return v;
            }

            void expect(const u8 *tag, const size_t n, const char *what) {
                need(n);
                if(std::memcmp(data + pos, tag, n) != 0)
                    bcError(what);
                pos += n;
            }

            void skip(const size_t n) {
                need(n);
                pos += n;
            }

            // i16 码字数组（count 个，奇数个补 1 个 i16 零对齐）
            Vec<i16> readI16Array(const i32 count) {
                Vec<i16> out(static_cast<size_t>(count));
                for(i32 k = 0; k < count; ++k)
                    out[static_cast<size_t>(k)] = static_cast<i16>(u16At());
                if(count & 1)
                    skip(2);
                return out;
            }
        };

        std::string utf16LeToUtf8(const u8 *data, const size_t bytes) {
            std::string out;
            out.reserve(bytes);
            for(size_t k = 0; k + 1 < bytes;) {
                char32_t u = data[k] | (static_cast<char32_t>(data[k + 1]) << 8);
                k += 2;
                if(u >= 0xD800 && u <= 0xDBFF && k + 1 < bytes) {
                    const char32_t lo = data[k] | (static_cast<char32_t>(data[k + 1]) << 8);
                    if(lo >= 0xDC00 && lo <= 0xDFFF) {
                        u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                        k += 2;
                    }
                }
                const auto enc = Common::utf8Encode(static_cast<i32>(u));
                if(enc.width > 0)
                    out.append(reinterpret_cast<const char *>(enc.data), static_cast<size_t>(enc.width));
            }
            return out;
        }

        // 常量池 7 段（tjsByteCodeLoader::ReadDataArea 对齐：各段 4 字节对齐补零，
        // 字符串 len 为 UTF-16 码元数、奇数补 2B）
        TjsConstPool readDataArea(ByteCursor &c, const i32 dataSize) {
            const size_t end = c.pos + static_cast<size_t>(dataSize);
            TjsConstPool pool{};

            // 1. BYTE（1B × count，补零到 4 对齐）
            i32 count = c.i32At();
            if(count > 0) {
                pool.bytes.resize(static_cast<size_t>(count));
                for(i32 k = 0; k < count; ++k)
                    pool.bytes[static_cast<size_t>(k)] = static_cast<i8>(c.u8At());
                c.skip(((static_cast<size_t>(count) + 3) & ~static_cast<size_t>(3))
                       - static_cast<size_t>(count));
            }
            // 2. SHORT
            count = c.i32At();
            if(count > 0) {
                pool.shorts.resize(static_cast<size_t>(count));
                for(i32 k = 0; k < count; ++k)
                    pool.shorts[static_cast<size_t>(k)] = static_cast<i16>(c.u16At());
                if(count & 1)
                    c.skip(2);
            }
            // 3. INTEGER
            count = c.i32At();
            if(count > 0) {
                pool.ints.resize(static_cast<size_t>(count));
                for(i32 k = 0; k < count; ++k)
                    pool.ints[static_cast<size_t>(k)] = c.i32At();
            }
            // 4. LONG
            count = c.i32At();
            if(count > 0) {
                pool.longs.resize(static_cast<size_t>(count));
                for(i32 k = 0; k < count; ++k)
                    pool.longs[static_cast<size_t>(k)] = c.i64At();
            }
            // 5. REAL
            count = c.i32At();
            if(count > 0) {
                pool.reals.resize(static_cast<size_t>(count));
                for(i32 k = 0; k < count; ++k) {
                    const i64 bits = c.i64At();
                    std::memcpy(&pool.reals[static_cast<size_t>(k)], &bits, sizeof(double));
                }
            }
            // 6. STRING（[len i32][UTF-16LE 码元 × len]，len 奇数补 2B）
            count = c.i32At();
            if(count > 0) {
                pool.strings.resize(static_cast<size_t>(count));
                for(i32 k = 0; k < count; ++k) {
                    const i32 len = c.i32At();
                    if(len < 0)
                        bcError("tjs bytecode: negative string length");
                    const size_t bytes = static_cast<size_t>(len) * 2;
                    c.need(bytes);
                    const std::string utf8 = utf16LeToUtf8(c.data + c.pos, bytes);
                    pool.strings[static_cast<size_t>(k)] =
                        String(utf8.c_str(), static_cast<u32>(utf8.size()));
                    c.skip(bytes);
                    if(len & 1)
                        c.skip(2);
                }
            }
            // 7. OCTET（[len i32][bytes × len]，补零到 4 对齐）
            count = c.i32At();
            if(count > 0) {
                pool.octets.resize(static_cast<size_t>(count));
                for(i32 k = 0; k < count; ++k) {
                    const i32 len = c.i32At();
                    if(len < 0)
                        bcError("tjs bytecode: negative octet length");
                    c.need(static_cast<size_t>(len));
                    pool.octets[static_cast<size_t>(k)].assign(c.data + c.pos,
                                                               c.data + c.pos + len);
                    c.skip(((static_cast<size_t>(len) + 3) & ~static_cast<size_t>(3))
                           - static_cast<size_t>(len));
                }
            }

            if(c.pos > end)
                bcError("tjs bytecode: data area overrun");
            c.pos = end; // 数据区按 datasize 边界收口（对残余填充不敏感）
            return pool;
        }

        TjsContext readContext(ByteCursor &c) {
            c.expect(CODE_TAG, 4, "tjs bytecode: context tag mismatch");
            c.skip(4); // ctxsize（格式自描述；解析以字段序列为准）

            TjsContext ctx{};
            ctx.parent = c.i32At();
            ctx.name = c.i32At();
            ctx.type = static_cast<TjsContextType>(c.i32At());
            ctx.maxVariableCount = c.i32At();
            ctx.variableReserveCount = c.i32At();
            ctx.maxFrameCount = c.i32At();
            ctx.funcDeclArgCount = c.i32At();
            ctx.funcDeclUnnamedArgArrayBase = c.i32At();
            ctx.funcDeclCollapseBase = c.i32At();
            ctx.propSetter = c.i32At();
            ctx.propGetter = c.i32At();
            ctx.superClassGetter = c.i32At();

            // 源位置表（仅 outputdebug 产物非 0；先全部 CodePos 再全部 SourcePos）
            const i32 srcPosCount = c.i32At();
            if(srcPosCount > 0) {
                ctx.codePos.resize(static_cast<size_t>(srcPosCount));
                for(i32 k = 0; k < srcPosCount; ++k)
                    ctx.codePos[static_cast<size_t>(k)] = c.i32At();
                ctx.sourcePos.resize(static_cast<size_t>(srcPosCount));
                for(i32 k = 0; k < srcPosCount; ++k)
                    ctx.sourcePos[static_cast<size_t>(k)] = c.i32At();
            }

            const i32 codeCount = c.i32At();
            if(codeCount < 0)
                bcError("tjs bytecode: negative code count");
            ctx.code = c.readI16Array(codeCount);

            const i32 dataCount = c.i32At();
            if(dataCount < 0)
                bcError("tjs bytecode: negative data count");
            ctx.data.resize(static_cast<size_t>(dataCount));
            for(i32 k = 0; k < dataCount; ++k) {
                const i16 type = static_cast<i16>(c.u16At());
                const i16 index = static_cast<i16>(c.u16At());
                ctx.data[static_cast<size_t>(k)] = TjsDataSlot{type, index};
            }

            const i32 scCount = c.i32At();
            if(scCount > 0) {
                ctx.superClassGetterPointers.resize(static_cast<size_t>(scCount));
                for(i32 k = 0; k < scCount; ++k)
                    ctx.superClassGetterPointers[static_cast<size_t>(k)] = c.i32At();
            }

            const i32 propCount = c.i32At();
            if(propCount > 0) {
                ctx.propMembers.resize(static_cast<size_t>(propCount));
                for(i32 k = 0; k < propCount; ++k) {
                    const i32 name = c.i32At();
                    const i32 objIdx = c.i32At();
                    ctx.propMembers[static_cast<size_t>(k)] = {name, objIdx};
                }
            }
            return ctx;
        }

    } // namespace

    TjsBytecodeFile readTjsBytecodeBytes(const Vec<u8> &bytes) {
        ByteCursor c{bytes.data(), bytes.size(), 0};
        TjsBytecodeFile file{};

        c.expect(FILE_TAG, 8, "tjs bytecode: bad magic");
        file.fileSize = c.i32At();
        if(static_cast<size_t>(file.fileSize) != bytes.size())
            bcError("tjs bytecode: file size mismatch");

        c.expect(DATA_TAG, 4, "tjs bytecode: DATA tag mismatch");
        file.dataSize = c.i32At();
        if(file.dataSize < 4)
            bcError("tjs bytecode: bad data size");
        // datasize 含 DATA tag(4) + 自身 size 字段(4)：数据区 = datasize - 8
        file.pool = readDataArea(c, file.dataSize - 8);

        c.expect(OBJ_TAG, 4, "tjs bytecode: OBJS tag mismatch");
        file.objsSize = c.i32At();
        file.topLevel = c.i32At();
        const i32 count = c.i32At();
        if(count < 0)
            bcError("tjs bytecode: negative context count");
        file.contexts.resize(static_cast<size_t>(count));
        for(i32 k = 0; k < count; ++k)
            file.contexts[static_cast<size_t>(k)] = readContext(c);

        // objsize 自描述校验（容错：不强制收口，越界已由各 read 的 need 兜底）
        return file;
    }

    TjsBytecodeFile readTjsBytecodeFile(const std::string &path) {
        std::ifstream f(path, std::ios::binary);
        if(!f)
            throw std::runtime_error("tjs bytecode: cannot open " + path);
        Vec<u8> bytes{(std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()};
        return readTjsBytecodeBytes(bytes);
    }

} // namespace cial::tjsbc
