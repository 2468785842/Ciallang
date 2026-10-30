#pragma once

namespace cial {
    class Context;
}

namespace cial::stdlib {
    // 注册内建方法、属性和类对象；print/Debug/Exception 由宿主配置。
    void registerStandardLibrary(Context &context);
} // namespace cial::stdlib
