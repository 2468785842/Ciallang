#include "Binding.hpp"
#include "cial/Engine.hpp"
#include "decl/DeclModel.hpp"

struct Counter {
    int value{};
    int add(int n) { return value += n; }
};

int main() {
    using namespace cial;
    cial::Engine engine;
    static_assert(cial::capabilities::hasDeclarationMetadata());
    engine.context()
        .module("Host"_str)
        .class_<Counter>("Counter"_str)
        .constructor<>()
        .method("add"_str, &Counter::add,
                api::signature({ { .name = "n", .type = "Integer" } }, "Integer", "增加并返回计数"));
    const auto declaration = decl::generateDeclarations(decl::declarationsFromCatalog(engine.context().apiCatalog()));
    if(declaration.find("function add(n: Integer): Integer;") == std::string::npos ||
       declaration.find("增加并返回计数") == std::string::npos)
        return 3;
    cial::Common::Result result;
    auto value = engine.execute("(new Host.Counter()).add(42) + \"😀\".length - 2", result, "host://smoke", true);
    if(result.isFailed() || value->asInteger().unwrap() != 42)
        return 1;
    cial::Common::Result invalid;
    auto unused = engine.execute("var = ;", invalid, "host://invalid");
    if(!invalid.isFailed() || invalid.messages().empty() || invalid.messages()[0].source() != "host://invalid")
        return 2;
    // 能力查询（doc/roadmap.md §3.1）：constexpr 查询与宏一致，别名过渡保留。
    if(cial::capabilities::hasJit() != (CIAL_CAP_JIT != 0))
        return 4;
    return CIAL_VERSION_STRING[0] != '\0' && CIAL_SDK_API_VERSION == 1 &&
            cial::SDK_API_VERSION == CIAL_SDK_API_VERSION &&
            cial::capabilities::capabilitySchemaVersion() == CIAL_CAPABILITY_SCHEMA_VERSION
        ? 0
        : 3;
}
