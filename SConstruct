import subprocess
import os.path
import shutil
import pathlib
import glob
import sys

# scons type=cpp_client|server build=release|debug target=all|linux|windows|macos archive=on|off copy_bin=on|off version= main=

build_type = ARGUMENTS.get("type", None)
if not build_type:
    print("Missing argument type.")
    sys.exit(1)
build = ARGUMENTS.get("build", "release")
target = ARGUMENTS.get("target", "all")
main_path = ARGUMENTS.get("main", "main.cpp")
version = ARGUMENTS.get("version", "latest")
archive = ARGUMENTS.get("archive", "off") == "on"
copy_bin = ARGUMENTS.get("copy_bin", "off") == "on"

if (not os.path.exists(f"src/{build_type}")):
    print("The build type supplied is not valid.")
    exit(1)

project_name = "Melodia"
cpp_standard = "20"
temp_dist_dir = "dist/tmp"
sfml_dir = f"src/{build_type}/external/build/SFML"
build_jobs = 4
use_system_sfml = (target == "linux")

sources = glob.glob(f"src/{build_type}/*.cpp")
if build_type == "cpp_client":
    sources.append("external/lib/RoundedRectangleShape.cpp")
sources += glob.glob("external/lib/SFC/*.cpp")
sources += glob.glob("external/lib/SFC/nanosvg++/*.cpp")

# name: path (external/lib/{path}), git (url to git repository), globs (a list of globs - all files that match any of the globs in the list must be moved to path)
external_libs = {
    "thread-pool":
        [".", "https://github.com/bshoshany/thread-pool", ["include/BS_thread_pool.hpp"]],
    "sf-svg":
        ["SFC", "https://github.com/kamirr/sf-svg", ["SFC/*"]],
    "stb":
        ["stb", "https://github.com/nothings/stb", ["stb_image_write.h", "stb_image_resize2.h", "stb_image.h"]],
    "webview":
        ["webview", "https://github.com/webview/webview", ["core/include/webview/*"]],
    "json":
        ["nlohmann", "https://github.com/nlohmann/json/tree/develop/include/nlohmann", ["include/nlohmann/*"]],
    "cpp-httplib":
        [".", "https://github.com/yhirose/cpp-httplib", ["httplib.h"]]
}

base = Environment(
    CPPPATH=["include", "external/lib"],
    CPPDEFINES=["CPPHTTPLIB_OPENSSL_SUPPORT"],
    CXXFLAGS=[f"-std=c++{cpp_standard}", "-fdiagnostics-color"],
    LIBS=[
        "libssl",
        "libcrypto",
        "libicuio",
        "libicuuc"
    ],
)

if not use_system_sfml:
    base.Append(CPPPATH=[f"{sfml_dir}/include"])

no_extras_in_build = [
    "-DBUILD_SHARED_LIBS=FALSE",
    "-DBUILD_TESTING=OFF"
]

def create_dist(target, archive_dist: bool):
    dist_path = pathlib.Path(target[0][1]).resolve()
    platform = target[0][0]
    temp_path = pathlib.Path(temp_dist_dir, platform).resolve()

    archive_ext = "gztar" if platform == "linux" else "zip"
    bin_ext = ".exe" if platform == "windows" else ""

    temp_path.mkdir(parents=True, exist_ok=True)
    (temp_path / "external/programs").mkdir(parents=True, exist_ok=True)

    shutil.copytree(
        "misc",
        temp_path / "misc",
        dirs_exist_ok=True,
    )

    if archive_dist:
        print(f"Creating archive for {platform} ({version})")

        dist_path.mkdir(parents=True, exist_ok=True)

        archive_stem = dist_path / f"Melodia-{platform}-{version}"
        archive_path = pathlib.Path(
            f"{archive_stem}.{archive_ext.replace('gztar', 'tar.gz')}"
        )

        if archive_path.exists():
            archive_path.unlink()

        shutil.make_archive(
            str(archive_stem),
            archive_ext,
            root_dir=str(temp_path),
        )

        if copy_bin:
            shutil.move(temp_path / f"Melodia{bin_ext}", str(archive_stem) + bin_ext)

    else:
        print(f"Creating destination directory for {platform} ({version})")

        dir_path = dist_path / version
        dir_path.mkdir(parents=True, exist_ok=True)

        for item in temp_path.iterdir():
            destination = dir_path / item.name

            if destination.exists() and item.name != ".music_data":
                if destination.is_dir():
                    shutil.rmtree(destination)
                else:
                    destination.unlink()

            if item.name != ".music_data" or not destination.exists():
                shutil.move(str(item), str(destination))

    if temp_path.exists():
        shutil.rmtree(temp_path)

def build_target(env, platform):
    build_dir = f"build/{platform}"

    VariantDir(build_dir, ".", duplicate=0)

    return (
        [
            platform,
            f"{out_dir}/{platform}"
        ],
        env.Program(
            target=f"{temp_dist_dir}/{platform}/{project_name + ".exe" if platform == "windows" else project_name}",
            source=[f"{build_dir}/{src}" for src in sources],
        )
    )

def ensure_sfml_repo():
    if not os.path.exists(sfml_dir):
        print("Cloning SFML...")
        subprocess.check_call([
            "git", "clone", "--depth", "1",
            "https://github.com/SFML/SFML.git",
            sfml_dir
        ])

def ensure_dependency_build(dep_name, source_dir, build_dir, cmake_args):
    cmake_args += no_extras_in_build

    lib_path = os.path.join(build_dir, "lib")

    if os.path.exists(lib_path) and any(name.startswith(f"lib{dep_name}") for name in os.listdir(lib_path)):
        print(f"{dep_name} already built in {build_dir}")
        return

    print(f"Building {dep_name} in {build_dir}...")

    os.makedirs(build_dir, exist_ok=True)

    subprocess.check_call(
        ["cmake", os.path.abspath(source_dir)] + cmake_args,
        cwd=build_dir
    )

    subprocess.check_call(
        ["cmake", "--build", ".", f"-j{build_jobs}"],
        cwd=build_dir
    )

    print(f"{dep_name} built in {build_dir}")

def ensure_deps_repo(dep_name, repo_url, branch = None):
    dep_dir = f"external/build/{dep_name}"
    if not os.path.exists(dep_dir):
        print(f"Cloning {dep_name}...")
        subprocess.check_call([
            *f"git clone --depth 1{(' --branch ' + branch) if branch else ''}".split(' '),
            repo_url,
            dep_dir
        ])

def ensure_external_lib(name):
    path, git, globs = external_libs[name]

    os.makedirs("", exist_ok=True)

def ensure_sfml_build(build_dir, cmake_args):
    lib_path = os.path.join(build_dir, "lib")

    if os.path.exists(lib_path) and any(name.startswith("libsfml") for name in os.listdir(lib_path)):
        print(f"SFML already built in {build_dir}")
        return

    print(f"Building SFML in {build_dir}...")

    os.makedirs(build_dir, exist_ok=True)

    subprocess.check_call(
        ["cmake", ".."] + cmake_args,
        cwd=build_dir
    )

    subprocess.check_call(
        ["cmake", "--build", ".", f"-j{build_jobs}"],
        cwd=build_dir
    )

    print(f"SFML built in {build_dir}")

def ensure_openssl_build(source_dir, build_dir, install_dir):
    if os.path.exists(os.path.join(install_dir, "include", "openssl")):
        print(f"OpenSSL already built in {build_dir}")
        return

    print("Building OpenSSL for Windows (MinGW cross-compile)...")
    os.makedirs(build_dir, exist_ok=True)

    subprocess.check_call([
        "perl", os.path.abspath(f"{source_dir}/Configure"),
        "mingw64",
        "no-shared",
        "no-tests",
        f"--cross-compile-prefix=x86_64-w64-mingw32-",
        f"--prefix={os.path.abspath(install_dir)}",
        f"--openssldir={os.path.abspath(install_dir)}",
    ], cwd=build_dir)

    subprocess.check_call(["make", f"-j{build_jobs}"], cwd=build_dir)
    subprocess.check_call(["make", "install_sw"], cwd=build_dir)

    print(f"OpenSSL built in {build_dir}")

if "debug" in build:
    base.Append(CXXFLAGS=[
        "-g",
        "-O0",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wfatal-errors"
    ])

    out_dir = "dist/debug"
else:
    base.Append(CXXFLAGS=["-O2"])
    out_dir = "dist/release"

if build == "debug-l1":
    base.Append(CXXFLAGS=[
        "-fsanitize=address",
        "-fsanitize=undefined",
        "-fno-omit-frame-pointer",
        "-fno-sanitize-recover=all"
    ])

    base.Append(LINKFLAGS=[
        "-fsanitize=address",
        "-fsanitize=undefined"
    ])

targets = []

if target in ("all", "linux"):
    print("Building for Linux")

    env = base.Clone()

    env.ParseConfig("pkg-config --cflags --libs sfml-graphics sfml-audio gtk+-3.0 webkit2gtk-4.1")

    prog = build_target(env, "linux")
    targets.append(prog)

if target in ("all", "windows"):
    print("Building for Windows")

    ensure_sfml_repo()

    ensure_deps_repo("libogg", "https://github.com/xiph/ogg.git")
    ensure_deps_repo("libvorbis", "https://github.com/xiph/vorbis.git")
    ensure_deps_repo("flac", "https://github.com/xiph/flac.git")
    ensure_deps_repo("openal-soft", "https://github.com/kcat/openal-soft.git")
    ensure_deps_repo("freetype", "https://github.com/freetype/freetype.git")
    ensure_deps_repo("harfbuzz", "https://github.com/harfbuzz/harfbuzz.git")
    ensure_deps_repo("openssl", "https://github.com/openssl/openssl.git")

    deps_build_dir = "external/build/build-mingw-deps"

    ensure_dependency_build("ogg", "external/build/libogg", f"{deps_build_dir}/ogg", [
        "-DCMAKE_SYSTEM_NAME=Windows",
        "-DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc",
        "-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++",
    ])

    ensure_dependency_build("vorbis", "external/build/libvorbis", f"{deps_build_dir}/vorbis", [
        "-DCMAKE_SYSTEM_NAME=Windows",
        "-DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc",
        "-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++",
        "-DBUILD_TESTING=OFF",
        f"-DOGG_INCLUDE_DIR={os.path.abspath('external/build/libogg/include')}",
        f"-DOGG_LIBRARY={os.path.abspath(deps_build_dir)}/ogg/lib/libogg.a",
    ])

    ensure_dependency_build("flac", "external/build/flac", f"{deps_build_dir}/flac", [
        "-DCMAKE_SYSTEM_NAME=Windows",
        "-DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc",
        "-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++",
        "-DINSTALL_MANPAGES=OFF",
        "-DBUILD_PROGRAMS=OFF",
        "-DBUILD_EXAMPLES=OFF",
        "-DBUILD_DOCS=OFF",
        f"-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY={os.path.abspath(deps_build_dir)}/flac/lib",
        f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={os.path.abspath(deps_build_dir)}/flac/lib",
        f"-DCMAKE_INSTALL_PREFIX={os.path.abspath(deps_build_dir)}/flac/install",
        f"-DOGG_INCLUDE_DIR={os.path.abspath('external/build/libogg/include')}",
        f"-DOGG_LIBRARY={os.path.abspath(deps_build_dir)}/ogg/lib/libogg.a",
    ])

    ensure_dependency_build("openal-soft", "external/build/openal-soft", f"{deps_build_dir}/openal-soft", [
        "-DCMAKE_SYSTEM_NAME=Windows",
        "-DBUILD_SHARED_LIBS=FALSE",
        "-DALSOFT_EXAMPLES=OFF",
        "-DALSOFT_UTILS=OFF",
        "-DALSOFT_TESTS=OFF",
        "-DALSOFT_BACKEND_PIPEWIRE=OFF",
        "-DALSOFT_BACKEND_PULSEAUDIO=OFF",
        "-DALSOFT_BACKEND_ALSA=OFF",
        "-DALSOFT_BACKEND_OSS=OFF",
        "-DALSOFT_BACKEND_SNDIO=OFF",
        "-DALSOFT_INSTALL_CONFIG=OFF"
        "-DALSOFT_REQUIRE_WINMM=ON",
        "-DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc",
        "-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++",
    ])

    ensure_dependency_build("harfbuzz", "external/build/harfbuzz", f"{deps_build_dir}/harfbuzz", [
        "-DCMAKE_SYSTEM_NAME=Windows",
        "-DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc",
        "-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++",
        "-DBUILD_SHARED_LIBS=FALSE",
    ])

    ensure_dependency_build("freetype", "external/build/freetype", f"{deps_build_dir}/freetype", [
        "-DCMAKE_SYSTEM_NAME=Windows",
        "-DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc",
        "-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++",
        "-DBUILD_SHARED_LIBS=FALSE",
    ])

    ensure_openssl_build(
        "external/build/openssl",
        f"{deps_build_dir}/openssl/build",
        f"{deps_build_dir}/openssl/install"
    )

    build_dir = f"{sfml_dir}/build-mingw"

    ensure_sfml_build(build_dir, [
        "-DCMAKE_SYSTEM_NAME=Windows",
        "-DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc",
        "-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++",
        "-DBUILD_SHARED_LIBS=FALSE",
    ])

    env = base.Clone()

    env.Replace(
        CXX="x86_64-w64-mingw32-g++",
        SHLINK="x86_64-w64-mingw32-g++",
    )

    env.Append(
        CXXFLAGS=[
            "-fpermissive"
        ],
        LINKFLAGS=[
            "-static-libgcc",
            "-static-libstdc++",
            "-static"
        ],
        LIBPATH=[
            f"{build_dir}/lib",
            f"{deps_build_dir}/ogg/lib",
            f"{deps_build_dir}/vorbis/lib",
            f"{deps_build_dir}/flac/lib",
            f"{deps_build_dir}/openal-soft",
            f"{deps_build_dir}/harfbuzz",
            f"{deps_build_dir}/freetype/lib",
            f"{deps_build_dir}/openssl/install/lib64"
        ],
        CPPPATH=[
            "include",
            f"{sfml_dir}/include",
            f"{deps_build_dir}/ogg/include",
            f"{deps_build_dir}/vorbis/include",
            f"{deps_build_dir}/flac/include",
            f"{deps_build_dir}/openal-soft/include",
            f"{deps_build_dir}/harfbuzz/include",
            f"{deps_build_dir}/freetype/include",
            f"{deps_build_dir}/openssl/install/include"
        ],
        LIBS=[
            "sfml-graphics-s",
            "sfml-audio-s",
            "sfml-window-s",
            "sfml-system-s",
            "ogg",
            "vorbis",
            "vorbisfile",
            "FLAC",
            "OpenAL32",
            "opengl32",
            "winmm",
            "gdi32",
            "harfbuzz",
            "freetype",
            "crypt32",
            "ws2_32",
            "libbcrypt",
            "libicudt"
        ],
        CPPDEFINES=["SFML_STATIC"]
    )

    prog = build_target(env, "windows")
    targets.append(prog)

if target in ("all", "macos"):
    print("Building for macOS")

    ensure_sfml_repo()

    build_dir = f"{sfml_dir}/build-macos"

    ensure_sfml_build(build_dir, [
        "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_SHARED_LIBS=FALSE",
        "-DMBEDTLS_THREADING_C=ON",
        "-DMBEDTLS_THREADING_PTHREAD=ON"
    ])

    env = base.Clone()

    env.Replace(CXX="clang++")

    env.Append(
        LIBPATH=[f"{build_dir}/lib"],
        LIBS=[
            "sfml-graphics",
            "sfml-audio",
            "sfml-window",
            "sfml-system",
        ],
        CXXFLAGS=[
            "-x", "objective-c++"  # Tells Clang to parse .cpp files as Objective-C++ (required for some MacOS specific things)
        ],
    )

    prog = build_target(env, "macos")
    targets.append(prog)

def post_build(*args, **kwargs):
    for target in targets:
        create_dist(target, archive and (not "debug" in build))

    shutil.rmtree(temp_dist_dir, ignore_errors=True)

Default(base.Command("post_build", [x[1] for x in targets], post_build))
