#ifndef _NOB_CONFIG_H_
#define _NOB_CONFIG_H_

#define MAIN "RMB"
// #define BUILD_DEBUG
#define MAX_PROCESS 5
#define EXTERNALS_DIR "externals"
#ifdef BUILD_DEBUG
#define BUILD_CONF "Debug"
#else
#define BUILD_CONF "Release"
#endif
#define BUILD_DIR "build"
#if defined(__aarch64__) || defined(__arm64__)
#define BUILD_ARCH "arm64"
#else
#define BUILD_ARCH "x64"
#endif
#define BUILD_OBJS_DIR "objs"

// May be we can change it programatically,
// may be download `git` temporarily if it doesn't exists?
static char* git_exec = "git";

// NOTE: We could introduce TARGET_#PLATFORM# macros, but the project only support these three, so
// these are enough
#ifdef _WIN32
#define TARGET_NAME "windows"
#define DEFAULT_PDB_FILE "vc143.pdb"
#define BUILD_OUT_SUFFIX ".exe"

static char* c_compiler_exec = "cl.exe";
static char* cpp_compiler_exec = "cl.exe";
#else

#define CC "cc" // used to determine system default include and library dirs

struct Nob_File_Paths;
static Nob_File_Paths include_dirs = {0};
static Nob_File_Paths library_dirs = {0};
#if defined(__APPLE__) || defined(__MACH__)

#ifndef LLVM_TOOLCHAIN
// Change this to the llvm toolchain directory, or define `LLVM_TOOLCHAIN`
#if defined(__aarch64__) || defined(__arm64__)
#define LLVM_TOOLCHAIN "/opt/homebrew/opt/llvm"
#else
#define LLVM_TOOLCHAIN "/usr/local/opt/llvm"
#endif // AARCH64/ARM64
#endif // LLVM_TOOLCHAIN

#define TARGET_NAME "macos"
static char* cpp_compiler_exec = LLVM_TOOLCHAIN"/bin/clang++";
static char* obj_cpp_compiler_exec = LLVM_TOOLCHAIN"/bin/clang++";
static char* c_compiler_exec = "clang"; // Using Apple's Clang for C/Objective-C

#ifdef BUILD_DEBUG
static char* cpp_flags[] = {};
static char* ld_flags[] = {};
#else
static char* cpp_flags[] = {};
static char* ld_flags[] = {"-Wl,-S"};
#endif // BUILD_DEBUG

static char* os_version_str = NULL;
static char* os_sysroot_dir = NULL;
#else
#define TARGET_NAME "linux"
static char* cpp_compiler_exec = "g++";
static char* c_compiler_exec = "cc";
#endif

#define BUILD_OUT_SUFFIX ""

#endif

#define BUILD__PATH(path) BUILD_DIR "/" path "/" BUILD_CONF "/" TARGET_NAME "-" BUILD_ARCH

#endif //_NOB_CONFIG_H_