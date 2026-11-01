#include "Binding.hpp"
#include "cial/Engine.hpp"

struct Counter {
    int value{};
    int add(int n) { return value += n; }
};

int main() {
    using namespace cial;
    cial::Engine engine;
    engine.context().module("Host"_str).class_<Counter>("Counter"_str).constructor<>().method("add"_str, &Counter::add);
    cial::Common::Result result;
    auto value = engine.execute("(new Host.Counter()).add(42) + \"😀\".length - 2", result, "host://smoke", true);
    if(result.isFailed() || value->asInteger().unwrap() != 42)
        return 1;
    cial::Common::Result invalid;
    auto unused = engine.execute("var = ;", invalid, "host://invalid");
    if(!invalid.isFailed() || invalid.messages().empty() || invalid.messages()[0].source() != "host://invalid")
        return 2;
    return CIAL_VERSION_STRING[0] != '\0' && cial::SDK_API_VERSION == 1 ? 0 : 3;
}
