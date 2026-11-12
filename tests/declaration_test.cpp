//
// 声明文件 (.d.tjs) 模式回归（doc/declarations.md）：解析、生成、目录转换、
// 往返幂等与执行侧拒绝。验收对应路线图批次 2 的 .d.tjs 声明模式。
//

#include <catch2/catch_all.hpp>

#include "cli/Compilation.hpp"
#include "common/SourceFile.hpp"
#include "decl/DeclModel.hpp"
#include "runtime/Context.hpp"
#include "stdlib/NativeRegister.hpp"
#include "stdlib/StandardLibrary.hpp"
#include "vm/VM.hpp"

using namespace cial;

namespace {
    // 解析环境：声明解析需要 Context 提供的 PreProcessor
    struct DeclEnv {
        Runtime runtime;
        Context context{ runtime };
    };

    decl::Module parseText(const std::string &text, Common::Result &result) {
        DeclEnv env;
        Common::SourceFile source;
        source.setSourceName("virtual://decl/test.d.tjs");
        Common::Result load;
        REQUIRE(source.load(load, text));
        return decl::parseDeclarations(source, result, env.context.pp());
    }

    std::string firstErrorCode(const std::string &text) {
        Common::Result result;
        (void)parseText(text, result);
        REQUIRE(result.isFailed());
        for(const auto &message : result.messages())
            if(message.isError())
                return message.code();
        return {};
    }
} // namespace

TEST_CASE("声明类型：联合、命名空间限定和数组类型往返", "[decl]") {
    Common::Result diagnostics;
    const auto module =
        parseText("function split(pattern: String | RegExp, resources: Host.Resource[]): String[];\n", diagnostics);
    REQUIRE_FALSE(diagnostics.isFailed());
    REQUIRE(module.symbols.front().params[0].type == "String | RegExp");
    REQUIRE(module.symbols.front().params[1].type == "Host.Resource[]");
    REQUIRE(module.symbols.front().ret == "String[]");
    const auto generated = decl::generateDeclarations(module);
    Common::Result again;
    REQUIRE(decl::generateDeclarations(parseText(generated, again)) == generated);
    REQUIRE_FALSE(again.isFailed());
    CHECK(firstErrorCode("function broken(value: String |): void;") == "decl.invalid-type");
    CHECK(firstErrorCode("function broken(value: String[): void;") == "decl.invalid-type");
}

TEST_CASE("声明解析：函数参数、类型规范化与默认值", "[decl]") {
    Common::Result result;
    const auto module =
        parseText("/** 简要文档 */\n"
                  "function fill(value: any, start: int = 0, end: Real = -1.5, tag: String = \"x\"): void;\n"
                  "function vague(a, b: any);\n"
                  "function rest(args: any*);\n"
                  "function all(*);\n",
                  result);
    REQUIRE_FALSE(result.isFailed());
    REQUIRE(module.symbols.size() == 4);

    const auto &fill = module.symbols[0];
    REQUIRE(fill.kind == decl::SymbolKind::Function);
    REQUIRE(fill.doc == "简要文档");
    REQUIRE(fill.ret == "void");
    REQUIRE(fill.params.size() == 4);
    REQUIRE(fill.params[0].name == "value");
    REQUIRE(fill.params[0].type == "any");
    REQUIRE_FALSE(fill.params[0].optional);
    REQUIRE(fill.params[1].type == "Integer"); // int 关键字规范化为模型名
    REQUIRE(fill.params[1].optional);
    REQUIRE(fill.params[1].defaultText == "0");
    REQUIRE(fill.params[2].type == "Real");
    REQUIRE(fill.params[2].defaultText == "-1.5");
    REQUIRE(fill.params[3].type == "String");
    REQUIRE(fill.params[3].defaultText == "\"x\"");

    const auto &vague = module.symbols[1];
    REQUIRE(vague.params.size() == 2);
    REQUIRE(vague.params[0].type.empty()); // 无注解 = 类型未知
    REQUIRE(vague.params[1].type == "any");

    const auto &rest = module.symbols[2];
    REQUIRE(rest.params.size() == 1);
    REQUIRE(rest.params[0].collapse);
    REQUIRE(rest.params[0].name == "args");
    REQUIRE(rest.params[0].type == "any");

    const auto &all = module.symbols[3];
    REQUIRE(all.params.size() == 1);
    REQUIRE(all.params[0].collapse);
    REQUIRE(all.params[0].name.empty());
}

TEST_CASE("声明解析：属性、变量与访问器", "[decl]") {
    Common::Result result;
    const auto module = parseText("property shorthand: Integer;\n"
                                  "property full { getter: String; setter(value: String); }\n"
                                  "property ro { getter; }\n"
                                  "var flag: Boolean;\n"
                                  "const limit: Integer;\n",
                                  result);
    REQUIRE_FALSE(result.isFailed());
    REQUIRE(module.symbols.size() == 5);

    const auto &shorthand = module.symbols[0];
    REQUIRE(shorthand.kind == decl::SymbolKind::Property);
    REQUIRE(shorthand.ret == "Integer");
    // 简写形式不声称读写性
    REQUIRE_FALSE(shorthand.hasGetter);
    REQUIRE_FALSE(shorthand.hasSetter);

    const auto &full = module.symbols[1];
    REQUIRE(full.hasGetter);
    REQUIRE(full.hasSetter);
    REQUIRE(full.ret == "String");

    const auto &ro = module.symbols[2];
    REQUIRE(ro.hasGetter);
    REQUIRE_FALSE(ro.hasSetter);
    REQUIRE(ro.ret.empty());

    REQUIRE(module.symbols[3].kind == decl::SymbolKind::Variable);
    REQUIRE_FALSE(module.symbols[3].constant);
    REQUIRE(module.symbols[3].ret == "Boolean");
    REQUIRE(module.symbols[4].constant);
}

TEST_CASE("声明解析：类、继承、静态与嵌套命名空间", "[decl]") {
    Common::Result result;
    const auto module = parseText("class Array extends Object {\n"
                                  "    function push(*);\n"
                                  "    static function create(): Array;\n"
                                  "    function Array(size: Integer = 0);\n"
                                  "    class Inner {\n"
                                  "        function ping(): void;\n"
                                  "    }\n"
                                  "}\n"
                                  "namespace Host {\n"
                                  "    /** 工具函数 */\n"
                                  "    function util(): void;\n"
                                  "    namespace Sub {\n"
                                  "        function deep(): void;\n"
                                  "    }\n"
                                  "}\n",
                                  result);
    REQUIRE_FALSE(result.isFailed());
    REQUIRE(module.symbols.size() == 2);

    const auto &array = module.symbols[0];
    REQUIRE(array.kind == decl::SymbolKind::Class);
    REQUIRE(array.extends == std::vector<std::string>{ "Object" });
    REQUIRE(array.members.size() == 4);
    REQUIRE(array.members[0].params.back().collapse);
    REQUIRE(array.members[1].kind == decl::SymbolKind::StaticFunction);
    REQUIRE(array.members[1].ret == "Array");
    // 构造器按 TJS2 惯例与类同名，按普通成员函数保留
    REQUIRE(array.members[2].kind == decl::SymbolKind::Function);
    REQUIRE(array.members[2].name == "Array");
    REQUIRE(array.members[2].params.front().optional);
    REQUIRE(array.members[3].kind == decl::SymbolKind::Class);
    REQUIRE(array.members[3].name == "Inner");
    REQUIRE(array.members[3].members.size() == 1);

    const auto &host = module.symbols[1];
    REQUIRE(host.kind == decl::SymbolKind::Namespace);
    REQUIRE(host.members.size() == 2);
    REQUIRE(host.members[0].doc == "工具函数");
    REQUIRE(host.members[1].kind == decl::SymbolKind::Namespace);
    REQUIRE(host.members[1].name == "Sub");
    REQUIRE(host.members[1].members.size() == 1);
}

TEST_CASE("声明诊断：逐类错误使用 decl.* 码", "[decl]") {
    CHECK(firstErrorCode("var v: Integer = 1;") == "decl.initializer");
    CHECK(firstErrorCode("const c: Integer = 1;") == "decl.initializer");
    CHECK(firstErrorCode("function f(a = (1));") == "decl.invalid-default");
    CHECK(firstErrorCode("function f(a: Integer = name);") == "decl.invalid-default");
    CHECK(firstErrorCode("function f(a: );") == "decl.invalid-type");
    CHECK(firstErrorCode("var v: 5;") == "decl.invalid-type");
    CHECK(firstErrorCode("class A { namespace B {} }") == "decl.invalid-nesting");
    CHECK(firstErrorCode("static function f();") == "decl.static-outside-class");
    CHECK(firstErrorCode("class A { static var v: Integer; }") == "decl.expected-token");
    CHECK(firstErrorCode("if (x) {}") == "decl.not-a-declaration");
    CHECK(firstErrorCode("foo;") == "decl.not-a-declaration");
    CHECK(firstErrorCode("class A { function f();") == "decl.expected-token");
    CHECK(firstErrorCode("property p { getter: Integer; setter(v: Integer); getter: Integer; }") ==
          "decl.duplicate-handler");
}

TEST_CASE("生成器：目录转换确定性输出", "[decl]") {
    ApiCatalog catalog;
    const auto stringKey = TypeTraits<String>::key();
    const auto arrayKey = TypeTraits<Array>::key();
    catalog.typeNames[stringKey] = "String";
    catalog.typeMembers[stringKey] = {
        ApiMeta{
            .kind = ApiKind::Method, .name = "charAt", .paramTypes = { "Integer" }, .ret = "String", .doc = "取字符" },
        ApiMeta{ .kind = ApiKind::Method, .name = "push", .variadic = true, .ret = "any" },
        ApiMeta{ .kind = ApiKind::Property, .name = "length", .ret = "any" },
    };
    catalog.typeNames[arrayKey] = "Array";
    catalog.typeMembers[arrayKey] = {
        ApiMeta{ .kind = ApiKind::Static, .name = "create", .paramTypes = { "any" }, .ret = "Array" },
    };
    catalog.globals = {
        ApiMeta{ .kind = ApiKind::Namespace, .name = "Host" },
        ApiMeta{ .kind = ApiKind::Global, .name = "util", .owner = "Host", .ret = "void" },
    };

    const auto module = decl::declarationsFromCatalog(catalog);
    // 未被 globals 认领的类按名排序前插；无名参数生成 argN 占位；
    // 变长条目转未命名收拢；登记属性转简写（读写性未知）
    REQUIRE(decl::generateDeclarations(module) ==
            "class Array {\n"
            "  /** @returns Array。 */\n"
            "  static function create(arg0: any): Array;\n"
            "}\n"
            "class String {\n"
            "  /**\n"
            "   * 取字符\n"
            "   * @returns String。\n"
            "   */\n"
            "  function charAt(arg0: Integer): String;\n"
            "  /** @returns any。 */\n"
            "  function push(*): any;\n"
            "  property length: any;\n"
            "}\n"
            "namespace Host {\n"
            "  /** @returns void。 */\n"
            "  function util(): void;\n"
            "}\n");
}

TEST_CASE("往返：标准库声明生成后可解析且逐字节幂等", "[decl]") {
    DeclEnv env;
    stdlib::registerStandardLibrary(env.context);
    const auto module = decl::declarationsFromCatalog(env.context.apiCatalog());
    const auto text = decl::generateDeclarations(module);
    REQUIRE_FALSE(text.empty());

    Common::Result load;
    Common::Result parseResult;
    Common::SourceFile source;
    source.setSourceName("virtual://stdlib.d.tjs");
    REQUIRE(source.load(load, text));
    const auto reparsed = decl::parseDeclarations(source, parseResult, env.context.pp());
    if(parseResult.isFailed()) {
        for(const auto &message : parseResult.messages())
            FAIL(message.message());
    }
    REQUIRE(decl::generateDeclarations(reparsed) == text);
}

TEST_CASE("执行侧拒绝：compile 与 VM::execute 拒绝 .d.tjs", "[decl]") {
    // 身份判定只看后缀，大小写不敏感，不访问文件系统
    REQUIRE(Common::isDeclarationSourceName("stdlib.d.tjs"));
    REQUIRE(Common::isDeclarationSourceName("STDLIB.D.TJS"));
    REQUIRE(Common::isDeclarationSourceName("archive://scripts/host.d.tjs"));
    REQUIRE_FALSE(Common::isDeclarationSourceName("script.tjs"));
    REQUIRE_FALSE(Common::isDeclarationSourceName("libdat.tjs"));
    REQUIRE_FALSE(Common::isDeclarationSourceName("notes.d.tjs.txt"));

    Runtime runtime;
    Context context{ runtime };
    vm::VMState state{ context };
    VM vm{ &state };

    Common::SourceFile source;
    source.setSourceName("host.d.tjs");
    Common::Result load;
    REQUIRE(source.load(load, "function f(): void;\n"));

    Common::Result compileResult;
    const auto compilation = cli::compile(compileResult, context, source);
    REQUIRE(compileResult.isFailed());
    REQUIRE_FALSE(compilation.chunk.has_value());
    REQUIRE(compileResult.messages().front().code() == "decl.not-executable");

    Common::Result executeResult;
    (void)vm.execute(source, executeResult);
    REQUIRE(executeResult.isFailed());
    REQUIRE(executeResult.messages().front().code() == "decl.not-executable");
}
