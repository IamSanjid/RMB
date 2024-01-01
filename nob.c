//#define _NOB_TEST

#define NOB_IMPLEMENTATION
#include ".nob/nob.h"

#define NOB_EXTENSIONS_IMPLEMENTATION
#include ".nob/nob_extensions.h"

#define NOB_CONFIG_PATH ".nob/nob_config.h"

#if defined(NOB_CONFIGURED) || defined(_NOB_TEST)
#include NOB_CONFIG_PATH

static const char* source_exts[] = {".c", ".cpp", ".cxx"};
static const char* obj_exts[] = {".o", ".obj", ".objc"};

// TODO: Add a way to store outputs(object files, shared/dynamic library files)?
typedef struct {
    const char* name;
    const void* config;
    int (*acquire)(const void* this);
    int (*build)(const void* this);
} External;
int default_external_acquire(const void* this) {
    (void)this;
    return 1;
}
int default_external_build(const void* this) {
    (void)this;
    return 1;
}

typedef struct {
    const char* link;
    const char* branch; // Optional
    const char* tag;    // Optional
} ExternalGitConfig;

int default_git_external_acquire(const void* this);
int external_imgui_build(const void* this_ptr);
int external_glfw_build(const void* this);

// We can introduce dependencies on each other but who cares right now lol...
static const External externals[] = {
    {
        .name = "glfw",
        &(ExternalGitConfig){
            .link = "https://github.com/glfw/glfw.git",
            .branch = NULL,
            .tag = "3.3.9",
        },
        &default_git_external_acquire,
        &external_glfw_build,
    },
    {
        .name = "imgui",
        &(ExternalGitConfig){
            .link = "https://github.com/ocornut/imgui.git",
            .branch = NULL,
            .tag = "v1.90",
        },
        &default_git_external_acquire,
        &external_imgui_build,
    },
    {
        .name = "concurrentqueue",
        &(ExternalGitConfig){
            .link = "https://github.com/cameron314/concurrentqueue.git",
            .branch = NULL,
            .tag = "v1.0.4",
        },
        &default_git_external_acquire,
        &default_external_build,
    }};

int default_git_external_acquire(const void* this_ptr) {
    External* this = (External*)this_ptr;
    int result = 1;
    ExternalGitConfig* config = (ExternalGitConfig*)this->config;

    const char* dir = this->name;

    const char* proj_dir = nob_temp_sprintf(EXTERNALS_DIR "/%s", dir);
    const char* proj_git_dir = nob_temp_sprintf("%s/.git", proj_dir);

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, git_exec);

    // TODO: Find a more solid way to detect if the externel dir is a git proj.
    // Use `git` client itself to detect it... ```git rev-parse --git-dir```
    if (nob_file_exists(proj_git_dir)) {
        nob_cmd_append(&cmd, "--git-dir");
        nob_cmd_append(&cmd, proj_git_dir);
        nob_cmd_append(&cmd, "--work-tree");
        nob_cmd_append(&cmd, proj_dir);
        nob_cmd_append(&cmd, "pull");
        if (config->branch)
            nob_cmd_append(&cmd, "origin", "branch", config->branch);
        else if (config->tag)
            nob_cmd_append(&cmd, "origin", "tag", config->tag);
        if (!nob_cmd_run_sync(cmd)) {
            nob_log(NOB_ERROR,
                    "Couldn't pull the project from '%s', remove the %s directory and try again",
                    config->link, proj_dir);
            nob_return_defer(0);
        }
        nob_return_defer(1);
    }
    nob_cmd_append(&cmd, "clone");
    nob_cmd_append(&cmd, config->link);
    nob_cmd_append(&cmd, "--depth=1"); // TODO: make it configurable?
    if (config->branch)
        nob_cmd_append(&cmd, "--branch", config->branch, "--single-branch");
    else if (config->tag)
        nob_cmd_append(&cmd, "--branch", config->tag, "--single-branch");
    nob_cmd_append(&cmd, proj_dir);

    if (!nob_cmd_run_sync(cmd))
        nob_return_defer(0);

defer:
    nob_cmd_free(cmd);
    return result;
}

// Valid till next nob_temp_reset.
const char* default_external_dir(const void* this_ptr) {
    External* this = (External*)this_ptr;

    const char* proj_dir = nob_temp_sprintf(EXTERNALS_DIR "/%s", this->name);
    return proj_dir;
}

typedef struct {
    const char** search_dirs;
    size_t search_dirs_len;
    const char** search_files;
    size_t search_files_len;
} SearchFilesInDirsArgs;

int search_files_in_dirs(SearchFilesInDirsArgs args, int* found_dir_idx) {
    int result = 0;
    *found_dir_idx = -1;
    for (size_t i = 0; i < args.search_dirs_len; i++) {
        size_t j = 0;
        for (; j < args.search_files_len; j++) {
            const char* search_file_path =
                nob_temp_sprintf("%s/%s", args.search_dirs[i], args.search_files[j]);
            int file_exists = nob_file_exists(search_file_path);
            if (file_exists < 0)
                nob_return_defer(file_exists);
            if (!file_exists)
                break;
        }
        if (j == args.search_files_len) {
            *found_dir_idx = i;
            nob_return_defer(1);
        }
    }

defer:
    return result;
}

int search_any_file_in_dirs(SearchFilesInDirsArgs args, int* found_dir_idx, int* found_file_idx) {
    int result = 0;
    *found_dir_idx = -1;
    for (size_t i = 0; i < args.search_dirs_len; i++) {
        size_t j = 0;
        for (; j < args.search_files_len; j++) {
            const char* search_file_path =
                nob_temp_sprintf("%s/%s", args.search_dirs[i], args.search_files[j]);
            int file_exists = nob_file_exists(search_file_path);
            if (file_exists < 0)
                nob_return_defer(file_exists);
            if (file_exists) {
                *found_dir_idx = i;
                *found_file_idx = j;
                nob_return_defer(1);
            }
        }
    }

defer:
    return result;
}

#ifndef _WIN32
// Uses Temp Buffer of NOB the caller must be responsible for freeing the strings...
bool unix_find_glfw_required_x11_paths(char** include_dir, char** libs_dir) {
    // reference: https://github.com/Kitware/CMake/blob/master/Modules/FindX11.cmake
    static const char* x11_inc_search_dirs[] = {
        "/usr/pkg/xorg/include",        "/usr/X11R6/include",
        "/usr/X11R7/include",           "/usr/include/X11",
        "/usr/openwin/include",         "/usr/openwin/share/include",
        "/opt/graphics/OpenGL/include", "/opt/X11/include"};
    static const char* x11_lib_search_dirs[] = {"/usr/pkg/xorg/lib", "/usr/X11R6/lib",
                                                "/usr/X11R7/lib", "/usr/openwin/lib",
                                                "/opt/X11/lib"};
    static const char* x11_required_header_files[] = {"X11/X.h",
                                                      "X11/extensions/Xrandr.h",
                                                      "X11/extensions/Xinerama.h",
                                                      "X11/extensions/XKB.h",
                                                      "X11/Xcursor/Xcursor.h",
                                                      "X11/extensions/XInput.h",
                                                      "X11/extensions/shape.h"};
    static const char* x11_lib_files[] = {"libX11.so", "libX11.a", "libX11"};

    bool result = true;
    *include_dir = NULL;
    *libs_dir = NULL;
    char* x11_lib = NULL;

    int x11_include_dir_idx = 0;
    int x11_lib_dir_idx = 0;
    int x11_lib_file_idx = 0;
    if (search_files_in_dirs((SearchFilesInDirsArgs){include_dirs.items, include_dirs.count,
                                                     x11_required_header_files,
                                                     NOB_ARRAY_LEN(x11_required_header_files)},
                             &x11_include_dir_idx))
        *include_dir = nob_temp_strdup(include_dirs.items[x11_include_dir_idx]);
    else if (search_files_in_dirs((SearchFilesInDirsArgs){x11_inc_search_dirs,
                                                          NOB_ARRAY_LEN(x11_inc_search_dirs),
                                                          x11_required_header_files,
                                                          NOB_ARRAY_LEN(x11_required_header_files)},
                                  &x11_include_dir_idx))
        *include_dir = nob_temp_strdup(x11_inc_search_dirs[x11_include_dir_idx]);
    else {
        nob_log(NOB_ERROR, "Couldn't find any X11 include dirs.");
        nob_return_defer(false);
    }

    if (search_any_file_in_dirs((SearchFilesInDirsArgs){library_dirs.items, library_dirs.count,
                                                        x11_lib_files,
                                                        NOB_ARRAY_LEN(x11_lib_files)},
                                &x11_lib_dir_idx, &x11_lib_file_idx)) {
        *libs_dir = nob_temp_strdup(library_dirs.items[x11_lib_dir_idx]);
        x11_lib = nob_temp_strdup(x11_lib_files[x11_lib_file_idx]);
    }
    else if (search_any_file_in_dirs(
                 (SearchFilesInDirsArgs){x11_lib_search_dirs, NOB_ARRAY_LEN(x11_lib_search_dirs),
                                         x11_lib_files, NOB_ARRAY_LEN(x11_lib_files)},
                 &x11_lib_dir_idx, &x11_lib_file_idx)) {
        *libs_dir = nob_temp_strdup(x11_lib_search_dirs[x11_lib_dir_idx]);
        x11_lib = nob_temp_strdup(x11_lib_files[x11_lib_file_idx]);
    }
    else {
        nob_log(NOB_ERROR, "Couldn't find any X11 libraries.");
        nob_return_defer(false);
    }

    nob_log(NOB_INFO,
            "Found X11 Package:\n    include_path=%s\n    library_path=%s\n    libfile=%s\n",
            *include_dir, *libs_dir, x11_lib);
defer:
    return result;
}
#endif

typedef struct {
    const char* input_dir;
    const char* build_dir;
    const char* input_flag;
    const char* output_flag;
    size_t max_process_count;
#ifdef _WIN32
    const char* pdb_file;
    bool multiple_pdb_writer;
#endif
} CompileObjsOptions;

void default_platform_specific_compile_options(Nob_Cmd* cmd, CompileObjsOptions* options) {
    options->max_process_count = MAX_PROCESS;
    options->input_flag = "-c";
    options->output_flag = "-o";

#ifdef _WIN32

    options->output_flag = "-Fo";

#ifdef BUILD_DEBUG
    nob_cmd_append(cmd, "-Od", "-EHsc", "-Zi", "-Ob0", "-MDd", "-RTC1", "-D_DEBUG=1");
#else
    nob_cmd_append(cmd, "-O2", "-Ob2", "-MD", "-DNDEBUG");
#endif
    // such bloat....
    nob_cmd_append(cmd, "-WX-", "-D_UNICODE", "-DUNICODE", "-DWIN32", "-D_WINDOWS", "-Gm-", "-GS",
                   "-fp:precise", "-Zc:wchar_t", "-Zc:forScope", "-Zc:inline");
    if (options->multiple_pdb_writer) {
        nob_cmd_append(cmd, "-MP");
        nob_cmd_append(cmd, "-FS");
    }
    if (options->pdb_file) {
        nob_cmd_append(cmd,
                       nob_temp_sprintf("-Fd\"%s/%s\"", options->build_dir, options->pdb_file));
    }
    else {
        nob_cmd_append(cmd, nob_temp_sprintf("-Fd\"%s/" DEFAULT_PDB_FILE "\"", options->build_dir));
    }

#else // _WIN32
#if defined(__APPLE__) || defined(__MACH__)

#else // MACOS

#endif // UNIX/LINUX

#ifdef BUILD_DEBUG
    nob_cmd_append(cmd, "-O0", "-ggdb", "-D_DEBUG=1");
#else
    nob_cmd_append(cmd, "-O2", "-s", "-DNDEBUG");
#endif

#endif // END_OF_PLATFORM_SPECIFIC
}

bool compile_obj_files(Nob_Cmd* default_cmd, const CompileObjsOptions* options,
                       const char** sources, size_t sources_cnt, Nob_File_Paths* object_files) {
    bool result = true;
    const size_t saved_cmd_cnt = default_cmd->count;

    Nob_Procs procs = {0};

    const char* full_input_dir = nob_fullpath(options->input_dir);
    NOB_ASSERT(full_input_dir != NULL && "The input dir shouldn't be NULL");
    const char* full_build_dir = nob_fullpath(options->build_dir);
    NOB_ASSERT(full_build_dir != NULL && "The build dir shouldn't be NULL");

    full_build_dir = nob_temp_sprintf("%s/" BUILD_OBJS_DIR, full_build_dir);

    if (!nob_mkdir_recursively_if_not_exists(full_build_dir)) {
        nob_return_defer(false);
    }

    size_t idx = 0;
    do {
        for (size_t p = 0; p < options->max_process_count; p++, idx++) {
            if (idx >= sources_cnt)
                break;
            default_cmd->count = saved_cmd_cnt;

            const char* input_file_path = sources[idx];

            const char* input_path = nob_fullpath(input_file_path);
            if (input_path == NULL) {
                input_path = nob_temp_sprintf("%s/%s", full_input_dir, input_file_path);
            }
            const char* output_path =
                nob_temp_sprintf("%s/%s.obj", full_build_dir, nob_filename_from_path(input_path));

            nob_da_append(object_files, output_path);

            if (nob_needs_rebuild(output_path, &input_path, 1)) {
                nob_cmd_append(default_cmd, options->input_flag, input_path);
#ifdef _WIN32
                nob_cmd_append(default_cmd,
                               nob_temp_sprintf("%s%s", options->output_flag, output_path));
#else
                nob_cmd_append(default_cmd, options->output_flag, output_path);
#endif
                Nob_Proc proc = nob_cmd_run_async(*default_cmd);
                nob_da_append(&procs, proc);
            }
        }
        if (!nob_procs_wait(procs))
            nob_return_defer(false);
        procs.count = 0;
    } while (idx < sources_cnt);

defer:
    nob_da_free(procs);
    default_cmd->count = saved_cmd_cnt;
    return result;
}

int external_glfw_build(const void* this_ptr) {
    External* this = (External*)this_ptr;

    static const char* default_srcs[] = {"src/context.c", "src/init.c",   "src/input.c",
                                         "src/monitor.c", "src/vulkan.c", "src/window.c"};

    ExternalGitConfig* config = (ExternalGitConfig*)this->config;
    nob_log_linebreak();
    nob_log(NOB_INFO, "Building glfw %s...", config->tag);

    int result = 1;

    const char* dir = this->name;

    Nob_Cmd cmd = {0};
    Nob_File_Paths object_files = {0};

    const char* proj_dir = nob_temp_sprintf(EXTERNALS_DIR "/%s", dir);
    const char* build_path = BUILD__PATH(EXTERNALS_DIR "/glfw");

    if (!nob_mkdir_recursively_if_not_exists(build_path)) {
        nob_return_defer(0);
    }

    nob_cmd_append(&cmd, c_compiler_exec);
    CompileObjsOptions compile_objs_options = {.input_dir = proj_dir,
                                               .build_dir = build_path,
#ifdef _WIN32
                                               .pdb_file = "glfw.pdb",
                                               .multiple_pdb_writer = true
#endif
    };
    default_platform_specific_compile_options(&cmd, &compile_objs_options);

    nob_cmd_append(&cmd, "-D_CRT_SECURE_NO_WARNINGS", "-Gd", "-TC", "/errorReport:prompt", "-W3",
                   "/external:W3");
    nob_cmd_append(&cmd, nob_temp_sprintf("-I%s/include", proj_dir));
    nob_cmd_append(&cmd, nob_temp_sprintf("-I%s/src", proj_dir));
#ifdef _WIN32
    static const char* platform_specific_sources[] = {
        "src/win32_init.c",  "src/win32_joystick.c", "src/win32_monitor.c",
        "src/win32_time.c",  "src/win32_thread.c",   "src/win32_window.c",
        "src/wgl_context.c", "src/egl_context.c",    "src/osmesa_context.c"};

    nob_cmd_append(&cmd, "-D_GLFW_WIN32");
#else
#if defined(__APPLE__) || defined(__MACH__)
    nob_log(NOB_INFO, "TODO: for macos");
    nob_return_defer(1);
#else
    static const char* platform_specific_sources[] = {
        "src/x11_init.c",       "src/x11_monitor.c",   "src/x11_window.c",  "src/xkb_unicode.c",
        "src/posix_time.c",     "src/posix_thread.c",  "src/glx_context.c", "src/egl_context.c",
        "src/osmesa_context.c", "src/linux_joystick.c"};
    char* x11_include_path = NULL;
    char* x11_library_path = NULL;
    if (!unix_find_glfw_required_x11_paths(&x11_include_path, &x11_library_path))
        nob_return_defer(0);
    bool is_non_implicit_x11_path = !nob_fps_contains_path(&include_dirs, x11_include_path) ||
                                    !nob_fps_contains_path(&library_dirs, x11_library_path);

    if (!is_non_implicit_x11_path) {
        nob_cmd_append(&cmd, nob_temp_sprintf("-I%s", x11_include_path));
        nob_cmd_append(&cmd, nob_temp_sprintf("-L%s", x11_library_path));
    }
    nob_cmd_append(&cmd, "-D_GLFW_X11", "-fPIC", "-Wall", "-std=gnu99");
#endif
#endif

    if (!compile_obj_files(&cmd, &compile_objs_options, default_srcs, NOB_ARRAY_LEN(default_srcs),
                           &object_files)) {
        nob_return_defer(0);
    }

    if (!compile_obj_files(&cmd, &compile_objs_options, platform_specific_sources,
                           NOB_ARRAY_LEN(platform_specific_sources), &object_files)) {
        nob_return_defer(0);
    }

    cmd.count = 0;
    // link to shared lib...

#ifdef _WIN32
    const char* libglfw3_path = nob_temp_sprintf("%s/glfw3.lib", build_path);
    if (nob_needs_rebuild(libglfw3_path, object_files.items, object_files.count)) {
        nob_cmd_append(&cmd, "lib", nob_temp_sprintf("/OUT:%s", libglfw3_path), "/MACHINE:X64",
                       "/machine:x64");
        for (size_t i = 0; i < object_files.count; ++i) {
            nob_cmd_append(&cmd, object_files.items[i]);
        }
        if (!nob_cmd_run_sync(cmd))
            nob_return_defer(false);
    }

#else
#if defined(__APPLE__) || defined(__MACH__)
    nob_log(NOB_INFO, "TODO: for macos");
    nob_return_defer(1);
#else
    const char* libglfw3_path = nob_temp_sprintf("%s/libglfw3.a", build_path);
    if (nob_needs_rebuild(libglfw3_path, object_files.items, object_files.count)) {
        nob_cmd_append(&cmd, "ar", "qc", libglfw3_path);
        for (size_t i = 0; i < object_files.count; ++i) {
            nob_cmd_append(&cmd, object_files.items[i]);
        }
        if (!nob_cmd_run_sync(cmd))
            nob_return_defer(false);
        cmd.count = 0;

        nob_cmd_append(&cmd, "ranlib", libglfw3_path);
        if (!nob_cmd_run_sync(cmd))
            nob_return_defer(false);
    }

#endif
#endif

    nob_log(NOB_INFO, "GLFW has been built successfully!");

defer:
    nob_cmd_free(cmd);
    nob_da_free(object_files);
    return result;
}

int external_imgui_build(const void* this_ptr) {
    static const char* imgui_backend_files[] = {
        "backends/imgui_impl_glfw.cpp",
        "backends/imgui_impl_opengl3.cpp",
    };
    External* this = (External*)this_ptr;

    ExternalGitConfig* config = (ExternalGitConfig*)this->config;
    nob_log_linebreak();
    nob_log(NOB_INFO, "Building imgui %s...", config->tag);

    int result = 1;

    const char* dir = this->name;

    Nob_Cmd cmd = {0};
    Nob_File_Paths object_files = {0};
    Nob_File_Paths srcs = {0};

    const char* proj_dir = nob_temp_sprintf(EXTERNALS_DIR "/%s", dir);
    const char* build_path = nob_temp_sprintf(BUILD__PATH(EXTERNALS_DIR "/%s"), dir);

    if (!nob_mkdir_recursively_if_not_exists(build_path)) {
        nob_return_defer(0);
    }

    nob_cmd_append(&cmd, cpp_compiler_exec);
    CompileObjsOptions compile_objs_options = {.input_dir = proj_dir,
                                               .build_dir = build_path,
#ifdef _WIN32
                                               .pdb_file = "imgui.pdb",
                                               .multiple_pdb_writer = true
#endif
    };
    default_platform_specific_compile_options(&cmd, &compile_objs_options);

    if (!nob_find_files_with_extensions(proj_dir, source_exts, NOB_ARRAY_LEN(source_exts), &srcs)) {
        nob_return_defer(0);
    }

    if (!nob_find_paths_recursively(proj_dir, imgui_backend_files,
                                    NOB_ARRAY_LEN(imgui_backend_files), &srcs)) {
        nob_return_defer(0);
    }

    for (size_t i = 0; i < NOB_ARRAY_LEN(externals); i++) {
        if (strcmp(externals[i].name, "glfw") == 0) {
            const char* dir = default_external_dir(&externals[i]);
            nob_cmd_append(&cmd, nob_temp_sprintf("-I%s/include", dir));
            break;
        }
    }
    nob_cmd_append(&cmd, nob_temp_sprintf("-I%s/backends", proj_dir));
    nob_cmd_append(&cmd, nob_temp_sprintf("-I%s", proj_dir));

#ifdef _WIN32
    nob_cmd_append(&cmd, "-W4", "-std:c++20");
#else // _WIN32
#if defined(__APPLE__) || defined(__MACH__)
    nob_log(NOB_INFO, "TODO: for macos");
    nob_return_defer(1);
#else // MACOS

#endif // LINUX
    nob_cmd_append(&cmd, "-Wall", "-std=c++20");
#endif // END_OF_PLATFORM_SEPCIFIC

    if (!compile_obj_files(&cmd, &compile_objs_options, srcs.items, srcs.count, &object_files)) {
        nob_return_defer(0);
    }
    nob_log(NOB_INFO, "ImGui Object Files has been built successfully!");

defer:
    nob_cmd_free(cmd);
    nob_da_free(object_files);
    nob_da_free(srcs);
    return result;
}

#define TEMPORARY_NOB_OUT ".nob/nob.tmp"
#ifdef _WIN32

int populate_lib_inc_dirs() {
    nob_log(NOB_INFO, "TODO: for macos");
    exit(1);
}

#else
#if defined(__APPLE__) || defined(__MACH__)

int populate_lib_inc_dirs() {
    nob_log(NOB_INFO, "TODO: for macos");
    exit(1);
}

#else

int populate_lib_inc_dirs() {
    int result = 1;

    Nob_Cmd cmd = {0};
    Nob_String_Builder sb = {0};

    nob_log(NOB_INFO, "Extracting system library and include path from c compiler...");
    nob_cmd_append(&cmd, c_compiler_exec, "-v", "-o", TEMPORARY_NOB_OUT, __FILE__);
    if (!nob_cmd_run_sync_with_output(cmd, &sb, NULL) || sb.count <= 1)
        nob_return_defer(0);
    nob_sb_append_null(&sb);

    bool searching_for_include_dirs = false;
    Nob_String_View output = nob_sv_from_parts(sb.items, sb.count);
    do {
        Nob_String_View line = nob_sv_chop_by_delim(&output, '\n');
        if (line.count > 0) {
            if (nob_sv_startswith_cstr(line, "LIBRARY_PATH=")) {
                Nob_String_View args = nob_sv_chop_by_delim_get_last(&line, '=');
                do {
                    char* lib_dir =
                        realpath(nob_temp_sv_to_cstr(nob_sv_chop_by_delim(&args, ':')), NULL);
                    if (lib_dir) {
                        if (!nob_fps_contains_path(&library_dirs, lib_dir)) {
                            nob_da_append(&library_dirs, lib_dir);
                        }
                        else
                            free(lib_dir);
                    }
                } while (args.count > 0);
                continue;
            }
            if (searching_for_include_dirs) {
                if (nob_sv_startswith_cstr(line, "End of search list")) {
                    searching_for_include_dirs = false;
                    continue;
                }
                char* include_dir = realpath(nob_temp_sv_to_cstr(nob_sv_trim(line)), NULL);
                if (include_dir) {
                    if (!nob_fps_contains_path(&include_dirs, include_dir)) {
                        nob_da_append(&include_dirs, include_dir);
                    }
                    else
                        free(include_dir);
                }
                continue;
            }
            if (nob_sv_startswith_cstr(line, "#include <...> search"))
                searching_for_include_dirs = true;
        }

    } while (output.count > 0);
    if (searching_for_include_dirs) {
        nob_log(NOB_ERROR, "Couldn't parse the system default include dirs");
        nob_return_defer(0);
    }
defer:
    if (nob_file_exists(TEMPORARY_NOB_OUT))
        remove(TEMPORARY_NOB_OUT);
    nob_cmd_free(cmd);
    nob_sb_free(sb);
    return result;
}
#endif
#endif

int check_prerequisites() {
    // Using everything which are available to use.
    Nob_File_Paths required_bins = {0};
    nob_cmd_append(&required_bins, git_exec, "--version");

#ifdef _WIN32
    nob_cmd_append(&required_bins, "link", "", cpp_compiler_exec, "", "lib", "");
#else
#if defined(__APPLE__) || defined(__MACH__)
#else
    nob_cmd_append(&required_bins, "cc", "--version", "ar", "--version", "ranlib", "--version");
#endif
#endif

    int result = 1;

    if (!nob_mkdir_if_not_exists("./build"))
        nob_return_defer(0);

    Nob_Cmd cmd = {0};
    for (size_t i = 0; i < required_bins.count; i += 2) {
        cmd.count = 0;
        const char* bin = required_bins.items[i];
        nob_cmd_append(&cmd, bin, required_bins.items[i + 1]);

        EXIT_CODE_TYPE exit_code = 0;
        if (nob_cmd_run_sync_silently(cmd, &exit_code) ||
        // EXIT_CODE when the command was not found...
#ifdef _WIN32
            exit_code != 9009
#else
            exit_code != 127
#endif
        ) {
            nob_log(NOB_INFO, "%s [OK]", bin);
        }
        else {
            nob_log(NOB_ERROR,
                    "Couldn't find %s, make sure %s is in your PATH environment variable.", bin,
                    bin);
            nob_return_defer(0);
        }
    }
#ifdef _WIN32
#else
#if defined(__APPLE__) || defined(__MACH__)
#else
    nob_return_defer(populate_lib_inc_dirs());
#endif
#endif
defer:
    nob_cmd_free(cmd);
    nob_da_free(required_bins);
    nob_temp_reset();
    return result;
}

int check_externels() {
    int result = 1;

    nob_log(NOB_INFO, "Checking external projects");
    if (!nob_mkdir_if_not_exists(EXTERNALS_DIR))
        return 0;

    for (size_t i = 0, n = NOB_ARRAY_LEN(externals); i < n; i++) {
        if (!externals[i].acquire(&externals[i]))
            nob_return_defer(0);
        if (!externals[i].build(&externals[i]))
            nob_return_defer(0);
    }

    nob_temp_reset();
defer:
    return result;
}

int build_main() {
    static const char* source_dir = "src";
    static const char* build_path = BUILD__PATH("RMB");
    static const char* source_dirs[] = {"src", "src/views", "src/Utils"};
    char* libglfw3_path = "";
    char* imgui_objs_path = "";
    const char* main_path = nob_temp_sprintf("%s/" MAIN BUILD_OUT_SUFFIX, build_path);

    if (!nob_mkdir_recursively_if_not_exists(build_path)) {
        return 0;
    }

    int result = 1;

    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    Nob_File_Paths object_files = {0};

    Nob_File_Paths srcs = {0};
    Nob_File_Paths includes = {0};

    for (size_t i = 0; i < NOB_ARRAY_LEN(source_dirs); i++) {
        const char* src_dir = source_dirs[i];
        if (!nob_find_files_with_extensions(src_dir, source_exts, NOB_ARRAY_LEN(source_exts),
                                            &srcs)) {
            nob_return_defer(0);
        }
        nob_da_append(&includes, src_dir);
    }

    for (size_t i = 0; i < NOB_ARRAY_LEN(externals); i++) {
        const char* dir = default_external_dir(&externals[i]);
        nob_da_append(&includes, nob_temp_strdup(dir));
        nob_da_append(&includes, nob_temp_sprintf("%s/include", dir));
        nob_da_append(&includes, nob_temp_sprintf("%s/backends", dir));
        if (strcmp(externals[i].name, "glfw") == 0) {
            libglfw3_path = nob_temp_sprintf(BUILD__PATH("%s"), dir);
        }
        else if (strcmp(externals[i].name, "imgui") == 0) {
            imgui_objs_path = nob_temp_sprintf(BUILD__PATH("%s"), dir);
        }
    }
    nob_cmd_append(&cmd, cpp_compiler_exec);

    CompileObjsOptions compile_objs_options = {.input_dir = source_dir,
                                               .build_dir = build_path,
#ifdef _WIN32
                                               .pdb_file = NULL,
                                               .multiple_pdb_writer = true
#endif
    };
    default_platform_specific_compile_options(&cmd, &compile_objs_options);
#ifdef _WIN32

#define PLATFORM_SPECIFIC_DIR "src/win"
    nob_cmd_append(&cmd, "-W4", "-std:c++20");

#if !defined(BUILD_DEBUG)
    nob_cmd_append(&cmd, "-GL", "-Oi");
#endif

    libglfw3_path = nob_temp_sprintf("%s/glfw3.lib", libglfw3_path);

#else // _WIN32
#if defined(__APPLE__) || defined(__MACH__)

#define PLATFORM_SPECIFIC_DIR "src/macos"

#else // MACOS

#define PLATFORM_SPECIFIC_DIR "src/linux"
    nob_cmd_append(&cmd, "-Wall", "-std=c++20");

    libglfw3_path = nob_temp_sprintf("%s/libglfw3.a", libglfw3_path);

#endif // UNIX/LINUX
#endif // END_OF_PLATFORM_SPECIFIC

    if (!nob_find_files_with_extensions(PLATFORM_SPECIFIC_DIR, source_exts,
                                        NOB_ARRAY_LEN(source_exts), &srcs)) {
        nob_return_defer(0);
    }
    nob_da_append(&includes, PLATFORM_SPECIFIC_DIR);

    for (size_t i = 0; i < includes.count; i++) {
        nob_cmd_append(&cmd, nob_temp_sprintf("-I%s", includes.items[i]));
    }

    if (!compile_obj_files(&cmd, &compile_objs_options, srcs.items, srcs.count, &object_files)) {
        nob_return_defer(0);
    }

    // GET ImGui object files... May be we should add that step here?
    imgui_objs_path = nob_temp_sprintf("%s/" BUILD_OBJS_DIR, imgui_objs_path);
    if (!nob_find_files_with_extensions(imgui_objs_path, obj_exts, NOB_ARRAY_LEN(obj_exts),
                                        &object_files)) {
        nob_return_defer(0);
    }

    cmd.count = 0;
    // LINK LIBS
#ifdef _WIN32
    if (nob_needs_rebuild(main_path, object_files.items, object_files.count)) {
        const char* main_lib = nob_temp_sprintf("%s/" MAIN ".lib", build_path);
        nob_cmd_append(&cmd, "link", nob_temp_sprintf("/OUT:%s", main_path), "/MACHINE:X64");
        nob_cmd_append(&cmd, "/MANIFEST", "/manifest:embed");
        nob_cmd_append(&cmd, "/DYNAMICBASE", "/NXCOMPAT", nob_temp_sprintf("/IMPLIB:%s", main_lib));
#ifdef BUILD_DEBUG
        const char* main_end_pdb = nob_temp_sprintf("%s/" MAIN ".pdb", build_path);
        const char* main_ilk = nob_temp_sprintf("%s/" MAIN ".ilk", build_path);
        nob_cmd_append(&cmd, "/DEBUG");
        nob_cmd_append(&cmd, nob_temp_sprintf("\"/PDB:%s\"", main_end_pdb));
        nob_cmd_append(&cmd, "/INCREMENTAL", nob_temp_sprintf("/ILK:%s", main_ilk));
#else
        const char* main_iobj = nob_temp_sprintf("%s/" MAIN ".iobj", build_path);
        nob_cmd_append(&cmd, "/INCREMENTAL:NO", "/SUBSYSTEM:WINDOWS", "/OPT:REF", "/OPT:ICF",
                       "/LTCG:incremental", "/ENTRY:mainCRTStartup");
        nob_cmd_append(&cmd, nob_temp_sprintf("/LTCGOUT:%s", main_iobj));
#endif
        nob_cmd_append(&cmd, "opengl32.lib", "user32.lib", "gdi32.lib", "shell32.lib");
        nob_cmd_append(&cmd, libglfw3_path);

        for (size_t i = 0; i < object_files.count; ++i) {
            nob_cmd_append(&cmd, object_files.items[i]);
        }
        if (!nob_cmd_run_sync(cmd))
            nob_return_defer(false);
    }

#else // _WIN32
#if defined(__APPLE__) || defined(__MACH__)

#else // MACOS

    if (nob_needs_rebuild(main_path, object_files.items, object_files.count)) {
        nob_cmd_append(&cmd, cpp_compiler_exec, "-o", main_path);

        for (size_t i = 0; i < object_files.count; ++i) {
            nob_cmd_append(&cmd, object_files.items[i]);
        }
        nob_cmd_append(&cmd, libglfw3_path, "-lGL", "-lX11", "-lXi", "-lXfixes", "-lXtst", "-lm",
                       "-lpthread", "-ldl");
        if (!nob_cmd_run_sync(cmd))
            nob_return_defer(false);
    }

#endif // LINUX
#endif
    nob_log(NOB_INFO, "Built '" MAIN "' successfully: %s", main_path);

defer:
    nob_cmd_free(cmd);
    nob_da_free(srcs);
    nob_da_free(includes);
    nob_da_free(object_files);
    nob_da_free(procs);
    nob_temp_reset();
    return result;
}

void cleanup() {
    nob_log(NOB_INFO, "Finished '" MAIN "' building step, cleaning up and quitting.");
#ifndef _WIN32
#if defined(__APPLE__) || defined(__MACH__)
#else
    if (include_dirs.count > 0)
        nob_da_deep_free(include_dirs);
    if (library_dirs.count > 0)
        nob_da_deep_free(library_dirs);
#endif
#endif
}

int main(int argc, char** argv) {
    const char* program = nob_shift_args(&argc, &argv);
    (void)program;

    int result = 0;
    nob_log(NOB_INFO, "--- STAGE 2 ---");

    if (!check_prerequisites())
        nob_return_defer(1);
    if (!check_externels())
        nob_return_defer(1);

    // nob_return_defer(0);
    nob_log(NOB_INFO, "BUILDING '" MAIN "'");
    if (!build_main())
        nob_return_defer(1);

defer:
    cleanup();
    return result;
}

#else

void generate_default_config(Nob_String_Builder* sb) {
    nob_log(NOB_ERROR, "generate_default_config is not implemented yet...");
    exit(0);
}

int main(int argc, char** argv) {
// Windows is not fun at all just like macOS...
#ifdef _WIN32
    if (!nob_file_exists(argv[0])) {
        argv[0] = nob_temp_sprintf("%s.exe", argv[0]);
    }
#endif
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char* program = nob_shift_args(&argc, &argv);
    (void)program;
    nob_log(NOB_INFO, "--- STAGE 1 ---");

    int config_exists = nob_file_exists(NOB_CONFIG_PATH);
    if (config_exists < 0)
        return 1;
    if (config_exists == 0) {
        nob_log(NOB_INFO, "Generating %s", NOB_CONFIG_PATH);
        Nob_String_Builder content = {0};
        generate_default_config(&content);
        if (!nob_write_entire_file(NOB_CONFIG_PATH, content.items, content.count))
            return 1;
    }
    else {
        nob_log(NOB_INFO, "file `%s` already exists", NOB_CONFIG_PATH);
    }

    Nob_Cmd cmd = {0};
    const char* configured_binary = "./nob.configured";
    const char* deps[] = {__FILE__, NOB_CONFIG_PATH};
    int needs_rebuild = nob_needs_rebuild(configured_binary, deps, NOB_ARRAY_LEN(deps));
    if (needs_rebuild < 0)
        return 1;
    if (needs_rebuild) {
        nob_cmd_append(&cmd, NOB_REBUILD_URSELF_EX(configured_binary, "nob.c"), "-DNOB_CONFIGURED");
        if (!nob_cmd_run_sync(cmd))
            return 1;
    }
    else {
        nob_log(NOB_INFO, "executable `%s` is up to date", configured_binary);
    }

    cmd.count = 0;
    nob_cmd_append(&cmd, configured_binary);
    nob_da_append_many(&cmd, argv, argc);
    if (!nob_cmd_run_sync(cmd))
        return 1;

    return 0;
}

#endif // NOB_CONFIGURED
