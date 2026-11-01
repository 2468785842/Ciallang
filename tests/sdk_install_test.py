"""安装、移动前缀，再从安装示例构建宿主；不使用源码树头文件或 Python 配置依赖。"""
import argparse
from pathlib import Path
import shutil
import os
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--cmake", default="cmake")
    args = parser.parse_args()
    build = args.build.resolve()
    cache = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        key, value = line.split("=", 1)
        cache[key.split(":", 1)[0]] = value
    config = cache.get("CMAKE_BUILD_TYPE") or "Release"

    def run(*command):
        subprocess.run(command, check=True)

    # Windows 编译器的后台进程可能暂时持有构建目录；清理错误不覆盖实际验收结果。
    with tempfile.TemporaryDirectory(prefix="cial sdk 空格 ", ignore_cleanup_errors=True) as directory:
        root = Path(directory)
        prefix = root / "original"
        run(args.cmake, "--install", str(build), "--config", config,
            "--component", "SDK", "--prefix", str(prefix))
        moved = root / "relocated"
        prefix.rename(moved)
        source = root / "host"
        shutil.copytree(moved / "share/ciallang/examples/host", source)
        command = [args.cmake, "-S", str(source), "-B", str(root / "build"),
                   "-G", cache["CMAKE_GENERATOR"], f"-DCMAKE_BUILD_TYPE={config}"]
        dependencies = cache.get("CMAKE_PREFIX_PATH", "")
        installed = cache.get("VCPKG_INSTALLED_DIR") or cache.get("_VCPKG_INSTALLED_DIR")
        triplet = cache.get("VCPKG_TARGET_TRIPLET")
        if installed and triplet:
            dependencies = str(Path(installed) / triplet) + ";" + dependencies
        command.append(f"-DCMAKE_PREFIX_PATH={moved};{dependencies}")
        for key in ("CMAKE_CXX_COMPILER", "CMAKE_MAKE_PROGRAM", "CMAKE_TOOLCHAIN_FILE",
                    "CMAKE_MSVC_RUNTIME_LIBRARY", "CMAKE_OSX_DEPLOYMENT_TARGET",
                    "CMAKE_OSX_ARCHITECTURES", "VCPKG_TARGET_TRIPLET", "VCPKG_INSTALLED_DIR"):
            if cache.get(key):
                command.append(f"-D{key}={cache[key]}")
        command.append("-DVCPKG_MANIFEST_INSTALL=OFF")
        run(*command)
        run(args.cmake, "--build", str(root / "build"), "--config", config, "--parallel", "2")
        ctest = str(Path(shutil.which(args.cmake) or args.cmake).with_name("ctest" + (".exe" if os.name == "nt" else "")))
        run(ctest, "--test-dir", str(root / "build"), "-C", config, "--output-on-failure")


if __name__ == "__main__":
    main()
