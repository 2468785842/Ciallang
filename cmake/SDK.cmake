include(CMakePackageConfigHelpers)

# 包名由依赖发现过程记录；导出的目标名不能唯一确定 Config 包名。
configure_package_config_file("${CMAKE_CURRENT_LIST_DIR}/CiallangConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CiallangConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Ciallang")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/CiallangConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY ExactVersion)

install(TARGETS Ciallang EXPORT CiallangTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT SDK
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT SDK
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT SDK)
# 公共入口（Tier 1）为 cial/Engine.hpp、cial/Storage.hpp、Binding.hpp 与生成的
# cial/Version.hpp、cial/Capability.hpp；其余传递依赖的内部头随 SDK 分发以便编译，
# 不代表 VM/IR 的内部布局获得 ABI 或源码兼容承诺（分层表见 doc/embedding.md）。
# cli/format/dbg 与 tjsbc/decompile 为工具层模块（工具 exe 与共享库的源码形态），
# 不进入 SDK 公共头，也不在公共闭包内。
install(DIRECTORY "${PROJECT_SOURCE_DIR}/src/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ciallang" COMPONENT SDK
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "cli" EXCLUDE PATTERN "format" EXCLUDE
    PATTERN "dbg" EXCLUDE PATTERN "tjsbc" EXCLUDE PATTERN "decompile" EXCLUDE)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/cial/Version.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/cial/Capability.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ciallang/cial" COMPONENT SDK)
install(EXPORT CiallangTargets NAMESPACE Ciallang::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Ciallang" COMPONENT SDK)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/CiallangConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CiallangConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Ciallang" COMPONENT SDK)
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/ciallang" COMPONENT SDK)
# 系统声明（构建期由 cial POST_BUILD 经 --api-decl-dir 按符号拆分生成，
# doc/declarations.md §3）随 SDK 安装：宿主把该目录配置进编辑器即可获得接口
# 文档与跳转，不需要翻查 C++ 注册源码。无 CLI 的配置没有产物，只安装手写声明。
if(TARGET cial)
    install(DIRECTORY "$<TARGET_FILE_DIR:cial>/declarations/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/ciallang/declarations" COMPONENT SDK)
else()
    install(FILES "${PROJECT_SOURCE_DIR}/tools/lsp/Exception.d.tjs"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/ciallang/declarations" COMPONENT SDK)
endif()
install(FILES "${PROJECT_SOURCE_DIR}/tests/installed/CMakeLists.txt"
    "${PROJECT_SOURCE_DIR}/tests/installed/main.cpp"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/ciallang/examples/host" COMPONENT SDK)
