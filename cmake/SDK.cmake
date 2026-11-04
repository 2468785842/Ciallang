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
# 公共入口为 cial/Engine.hpp 与 Binding.hpp；传递依赖的内部头随 SDK 分发，
# 不代表 VM/IR 的内部布局获得 ABI 或源码兼容承诺。
# cli/format/dbg 为工具层模块（工具 exe 与共享库的源码形态），不进入 SDK 公共头。
install(DIRECTORY "${PROJECT_SOURCE_DIR}/src/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ciallang" COMPONENT SDK
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "cli" EXCLUDE PATTERN "format" EXCLUDE
    PATTERN "dbg" EXCLUDE)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/cial/Version.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ciallang/cial" COMPONENT SDK)
install(EXPORT CiallangTargets NAMESPACE Ciallang::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Ciallang" COMPONENT SDK)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/CiallangConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CiallangConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Ciallang" COMPONENT SDK)
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/ciallang" COMPONENT SDK)
install(FILES "${PROJECT_SOURCE_DIR}/tests/installed/CMakeLists.txt"
    "${PROJECT_SOURCE_DIR}/tests/installed/main.cpp"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/ciallang/examples/host" COMPONENT SDK)
