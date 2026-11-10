/*
 * Copyright (c) 2024/5/29 下午5:45
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

#include "Session.hpp"

#include <filesystem>

#include "stdlib/ExceptionLib.hpp"
#include "stdlib/Print.hpp"
#include "stdlib/ScriptsLib.hpp"
#include "stdlib/StandardLibrary.hpp"
#include "vm/VM.hpp"

namespace cial::cli {
    void setupSession(Context &context, VM &vm, vm::VMState &state, const std::filesystem::path &scriptPath) {
        stdlib::registerStandardLibrary(context);
        // print/println 是类型擦除的 NativeFunction，签名无法自动推导；按注册事实显式登记
        context.registryGlobalFunc("print"_str, &stdlib::S_PrintFunction);
        context.recordApiGlobal(ApiMeta{ .kind = ApiKind::Global,
                                         .name = "print",
                                         .paramTypes = { "any" },
                                         .ret = "void",
                                         .doc = "输出值的字符串化结果（不换行）" });
        context.registryGlobalFunc("println"_str, &stdlib::S_PrintlnFunction);
        context.recordApiGlobal(ApiMeta{ .kind = ApiKind::Global,
                                         .name = "println",
                                         .paramTypes = { "any" },
                                         .ret = "void",
                                         .doc = "输出值的字符串化结果并换行" });
        vm.eval(R"(class __DebugShim {
    function message(s) { println(s); }
}
var Debug = new __DebugShim();
)"_str);
        stdlib::initExceptionClass(vm);
        // 重绑定 Scripts：嵌套执行所需的 VMState + 存储基目录（相对路径
        // 相对当前执行脚本所在目录，krkr2 TVPSearchPlacedPath 的 CLI 语义）
        stdlib::registerScriptsLib(context, &state,
                                   scriptPath.empty() ? std::filesystem::path{} : scriptPath.parent_path());
    }
} // namespace cial::cli
