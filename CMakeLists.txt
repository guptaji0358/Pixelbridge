cmake_minimum_required(VERSION 3.21)
project(QRStudio VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------- layout ----
# The native source tree lives in ONE directory, named here and nowhere else.
# To rename it (e.g. gui -> app) rename the folder and change this default,
# or configure with -DQRS_SOURCE_DIR=<name>.
set(QRS_SOURCE_DIR "gui" CACHE STRING "Folder holding include/, src/ and main/")
set(QRS_ROOT    "${CMAKE_CURRENT_SOURCE_DIR}/${QRS_SOURCE_DIR}")
set(QRS_INCLUDE "${QRS_ROOT}/include")
set(QRS_SRC     "${QRS_ROOT}/src")
set(QRS_MAIN    "${QRS_ROOT}/main")
if(NOT IS_DIRECTORY "${QRS_ROOT}")
    message(FATAL_ERROR "QRS_SOURCE_DIR='${QRS_SOURCE_DIR}' does not exist under ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(QRS_BUILD_TESTS "Build the C++ test suite" ON)
option(QRS_BUILD_GUI   "Build the desktop application" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Concurrent Test)

# ------------------------------------------------- C: quirc (third party) ----
add_library(quirc STATIC
    third_party/quirc/quirc.c
    third_party/quirc/decode.c
    third_party/quirc/identify.c
    third_party/quirc/version_db.c)
target_include_directories(quirc PUBLIC third_party/quirc)
# Default of 254 regions is too low for photos and multi-code sheets.
target_compile_definitions(quirc PUBLIC QUIRC_MAX_REGIONS=65534)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(quirc PRIVATE -w)
endif()

# ------------------------------------------------------ C: decoding core ----
add_library(qrs_decode STATIC
    ${QRS_SRC}/core/qrs_decode.c
    ${QRS_SRC}/core/qrs_preprocess.c)
target_include_directories(qrs_decode PUBLIC ${QRS_INCLUDE} PRIVATE ${QRS_SRC}/core)
target_link_libraries(qrs_decode PRIVATE quirc)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(qrs_decode PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------- Rust: QR encoder ----
# Built as a DLL with a plain C ABI so the Rust toolchain's C runtime never
# has to match the C++ toolchain's.
find_program(CARGO_EXECUTABLE cargo REQUIRED)
get_filename_component(_cargo_dir "${CARGO_EXECUTABLE}" DIRECTORY)
set(QRS_RUST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rust/qrs_encode")
set(QRS_RUST_TARGET_DIR "${CMAKE_BINARY_DIR}/rust")
set(QRS_RUST_DLL    "${QRS_RUST_TARGET_DIR}/release/qrs_encode.dll")
set(QRS_RUST_IMPLIB "${QRS_RUST_TARGET_DIR}/release/libqrs_encode.dll.a")

# Rust's GNU target links with `gcc`; pin it to the gcc that ships next to
# cargo (MSYS2) so PATH order cannot substitute a mismatched MinGW runtime.
set(_cargo_env "CARGO_TARGET_DIR=${QRS_RUST_TARGET_DIR}")
if(EXISTS "${_cargo_dir}/gcc.exe")
    list(APPEND _cargo_env "CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER=${_cargo_dir}/gcc.exe")
endif()

file(GLOB QRS_RUST_SOURCES CONFIGURE_DEPENDS "${QRS_RUST_DIR}/src/*.rs")
add_custom_command(
    OUTPUT "${QRS_RUST_DLL}" "${QRS_RUST_IMPLIB}"
    COMMAND ${CMAKE_COMMAND} -E env ${_cargo_env}
            "${CARGO_EXECUTABLE}" build --release --offline --manifest-path "${QRS_RUST_DIR}/Cargo.toml"
    DEPENDS ${QRS_RUST_SOURCES} "${QRS_RUST_DIR}/Cargo.toml"
    COMMENT "Building Rust encoder (qrs_encode.dll)"
    VERBATIM)
add_custom_target(qrs_encode_build DEPENDS "${QRS_RUST_DLL}")

add_library(qrs_encode SHARED IMPORTED GLOBAL)
set_target_properties(qrs_encode PROPERTIES
    IMPORTED_LOCATION "${QRS_RUST_DLL}"
    IMPORTED_IMPLIB   "${QRS_RUST_IMPLIB}")
add_dependencies(qrs_encode qrs_encode_build)

# --------------------------------------------------- C++: core services ----
add_library(qrs_core STATIC
    ${QRS_SRC}/core/Generator.cpp
    ${QRS_SRC}/core/Scanner.cpp
    ${QRS_SRC}/core/Content.cpp
    ${QRS_SRC}/core/Files.cpp)
target_include_directories(qrs_core PUBLIC ${QRS_INCLUDE})
target_link_libraries(qrs_core PUBLIC Qt6::Core Qt6::Gui PRIVATE qrs_decode qrs_encode)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(qrs_core PRIVATE -Wall -Wextra)
endif()

# ------------------------------------------------------------ executables ----
# qrs_copy_runtime_dlls() / qrs_deploy_qt_plugins() put the DLLs each program
# needs (Rust encoder, Qt, MinGW runtime, Qt plugins) next to it after every build.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(RuntimeDlls)

add_executable(qrs_cli ${QRS_MAIN}/qrs_cli.cpp)
target_link_libraries(qrs_cli PRIVATE qrs_core)
qrs_copy_runtime_dlls(qrs_cli)

if(QRS_BUILD_GUI AND EXISTS "${QRS_ROOT}/gui.cmake")
    include("${QRS_ROOT}/gui.cmake")
endif()

if(QRS_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests/cpp)
endif()
