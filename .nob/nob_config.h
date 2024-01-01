#ifndef _NOB_CONFIG_H_
#define _NOB_CONFIG_H_

#define MAIN "RMB"
//#define BUILD_DEBUG
#define MAX_PROCESS 5
#define EXTERNALS_DIR "externals"
#ifdef BUILD_DEBUG
#define BUILD_CONF "Debug"
#else
#define BUILD_CONF "Release"
#endif
#define BUILD_DIR "build"
#define BUILD_ARCH "x64"
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
#if defined(__APPLE__) || defined(__MACH__)
#define TARGET_NAME "macos"
static char* cpp_compiler_exec = "clang";
#else
#define TARGET_NAME "linux"
static char* cpp_compiler_exec = "g++";

struct Nob_File_Paths;
static Nob_File_Paths include_dirs = {0};
static Nob_File_Paths library_dirs = {0};
#endif
static char* c_compiler_exec = "cc";
#define BUILD_OUT_SUFFIX = ""

#endif

#define BUILD__PATH(path) BUILD_DIR "/" path "/" BUILD_CONF "/" TARGET_NAME "-" BUILD_ARCH

#endif //_NOB_CONFIG_H_
