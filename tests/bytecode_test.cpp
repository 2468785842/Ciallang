#include <catch2/catch_all.hpp>

#include <limits>

#include "types/Function.hpp"
#include "vm/Bytecode.hpp"
#include "vm/VMState.hpp"

using namespace cial;
using cial::inter::Register;

TEST_CASE("bytecode LEB128 preserves full-width integers and cursor boundaries", "[bytecode]") {
    for(const u64 value :
        { u64{ 0 }, u64{ 1 }, u64{ 127 }, u64{ 128 }, u64{ 16384 }, u64{ 1 } << 63, std::numeric_limits<u64>::max() }) {
        Vec<u8> bytes{ 0x42 };
        const auto count = vm::encodeUleb128(value, bytes);
        bytes.push_back(0x43);
        u64 pc = 1;
        REQUIRE(vm::decodeUleb128(bytes.data(), pc, count) == value);
        REQUIRE(pc == count + 1);
        REQUIRE(bytes[pc] == 0x43);
    }

    for(const i64 value : { std::numeric_limits<i64>::min(), i64{ -129 }, i64{ -1 }, i64{ 0 }, i64{ 1 }, i64{ 128 },
                            std::numeric_limits<i64>::max() }) {
        Vec<u8> bytes;
        vm::encodeSleb128(value, bytes);
        u64 pc = 0;
        REQUIRE(vm::decodeSleb128(bytes.data(), pc, bytes.size()) == value);
        REQUIRE(pc == bytes.size());
    }
}

TEST_CASE("bytecode LEB128 rejects overflow and truncation without advancing the cursor", "[bytecode]") {
    SECTION("a terminating tenth byte has at most one payload bit") {
        for(unsigned last = 2; last < 128; ++last) {
            Vec<u8> bytes{ 0x42 };
            bytes.insert(bytes.end(), 9, 0x80);
            bytes.push_back(static_cast<u8>(last));
            u64 pc = 1;
            REQUIRE_THROWS_AS(vm::decodeUleb128(bytes.data(), pc, 10), std::runtime_error);
            REQUIRE(pc == 1);
            REQUIRE_THROWS_AS(vm::decodeSleb128(bytes.data(), pc, 10), std::runtime_error);
            REQUIRE(pc == 1);
        }
    }

    SECTION("unterminated values respect the caller's byte limit") {
        const Vec<u8> bytes(10, 0x80);
        for(const size_t limit : { size_t{ 0 }, size_t{ 1 }, size_t{ 9 }, size_t{ 10 } }) {
            u64 pc = 0;
            REQUIRE_THROWS_AS(vm::decodeUleb128(bytes.data(), pc, limit), std::runtime_error);
            REQUIRE(pc == 0);
        }
    }
}

TEST_CASE("bytecode PopN encodes its count and removes only temporary argument slots", "[bytecode]") {
    SECTION("the count uses ZigZag LEB128") {
        vm::Chunk chunk;
        chunk.emit<inter::PopN>(Integer{ 128 });
        const auto encoded = vm::Bytecode::compile(chunk.getInstVec(), {});
        REQUIRE(encoded.code == Vec<u8>{ static_cast<u8>(vm::VmOpCode::PopN), 0x80, 0x02 });
        REQUIRE(encoded.tacStartPos == Vec<u32>{ 0, 3 });
    }

    SECTION("dispatch pops the newest value and preserves the frame window") {
        Runtime runtime;
        Context context{ runtime };
        vm::VMState state{ context };
        Vec<size_t> poolSizes;
        Integer remaining{};
        auto probe = runtime.create<NativeFunction>([&](VM *, const Value &, size_t) {
            poolSizes.push_back(context.regPool().used());
            if(poolSizes.size() == 1)
                remaining = state.curFrame()->getArgs(1)[0].asInteger().unwrap();
        });
        context.registryGlobalFunc("probe"_str, probe.get());
        const Atom probeName = runtime.atomTable.intern("probe"_str);
        vm::Chunk chunk;
        chunk.setRegCount(4);
        chunk.emit<inter::GGlobal>(probeName, Register{ 3 });
        chunk.emit<inter::LoadImm>(Register{ 0 }, Integer{ 11 });
        chunk.emit<inter::LoadImm>(Register{ 1 }, Integer{ 22 });
        chunk.emit<inter::Push>(Register{ 0 });
        chunk.emit<inter::Push>(Register{ 1 });
        chunk.emit<inter::PopN>(Integer{ 1 });
        chunk.emit<inter::Call>(Register{ 2 }, Register{ 3 }, Integer{ 0 });
        chunk.emit<inter::PopN>(Integer{ 1 });
        chunk.emit<inter::Call>(Register{ 2 }, Register{ 3 }, Integer{ 0 });
        chunk.emit<inter::Ret>(Register{ 2 });
        auto rooted = std::move(chunk).toHeap(runtime);
        state.allocCallFrame(rooted.get());
        const auto frameSize = context.regPool().used();
        state.run();
        REQUIRE(remaining == 11);
        REQUIRE(poolSizes == Vec<size_t>{ frameSize + 1, frameSize });
    }
}
