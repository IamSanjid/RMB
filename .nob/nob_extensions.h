#ifndef NOB_EXTENSIONS_H_
#define NOB_EXTENSIONS_H_

#include "nob.h"

#ifdef _WIN32
#define realpath(N, R) _fullpath((R), (N), MAX_PATH)
#define PIPE_TYPE HANDLE
#define EXIT_CODE_TYPE DWORD
#else
#define PIPE_TYPE int
#define EXIT_CODE_TYPE int
#endif

#ifndef NOB_REBUILD_URSELF_EX
#  if defined(_WIN32) && defined(_MSC_VER)
#      define NOB_REBUILD_URSELF_EX(binary_path, source_path) "cl.exe", nob_temp_sprintf("/Fe:%s", binary_path), source_path
#  else
#      define NOB_REBUILD_URSELF_EX(binary_path, source_path) NOB_REBUILD_URSELF(binary_path, source_path)
#  endif
#endif

#define nob_da_deep_free(da)                            \
    do {                                                \
        for (size_t i = 0; i < (da).count; i++) {       \
            NOB_FREE((void*)(da).items[i]);             \
        }                                               \
        nob_da_free(da);                                \
    } while(0);

#define nob_log_linebreak() fprintf(stderr, "\n");

bool nob_fps_contains_path(Nob_File_Paths* fps, const char* path);

Nob_String_View nob_sv_chop_by_delim_get_last(Nob_String_View* sv, char delim);
bool nob_sv_startswith_cstr(Nob_String_View sv, const char* cstr);
bool nob_sv_endswith_cstr(Nob_String_View sv, const char* cstr);
bool nob_sv_contains_cstr(Nob_String_View sv, const char* cstr);
Nob_String_View nob_sv_chop_left_by_n(Nob_String_View sv, size_t n);
Nob_String_View nob_sv_chop_right_by_n(Nob_String_View sv, size_t n);

bool nob_cmd_run_sync_with_output(Nob_Cmd cmd, Nob_String_Builder* o_sb,
                                  EXIT_CODE_TYPE* exit_code);
bool nob_cmd_run_sync_silently(Nob_Cmd cmd, EXIT_CODE_TYPE* exit_code);

bool nob_mkdir_recursively_if_not_exists(const char *path);
bool nob_mk_file_parentdir_if_not_exists(const char *path);

bool nob_find_files_with_extensions(const char* search_dir, const char** extensions_arr, size_t arr_cnt, Nob_File_Paths* outfiles);
const char* nob_find_path_recursively(const char* search_dir, const char* search_path);
bool nob_find_paths_recursively(const char* dir, const char** paths, size_t paths_cnt, Nob_File_Paths* outfiles);
const char* nob_filename_from_path(const char* path);

const char* nob_relative_path_from(const char* from_path, const char* to_path);
const char* nob_fullpath(const char* path);

#ifdef NOB_EXTENSIONS_IMPLEMENTATION

bool nob_fps_contains_path(Nob_File_Paths* fps, const char* path) {
    for (size_t i = 0; i < fps->count; i++)
        if (strcmp(fps->items[i], path) == 0) return true;
    return false;
}

Nob_String_View nob_sv_chop_by_delim_get_last(Nob_String_View* sv, char delim) {
    do {
        Nob_String_View chopped = nob_sv_chop_by_delim(sv, delim);
        if (sv->count <= 0) {
            return chopped;
        }
    } while (1);
}

bool nob_sv_startswith_cstr(Nob_String_View sv, const char* cstr) {
    size_t len = strlen(cstr);
    if (sv.count < len) return false;

    return memcmp(sv.data, cstr, len) == 0;
}

bool nob_sv_startswith(Nob_String_View sv_a, Nob_String_View sv_b) {
    if (sv_a.count < sv_b.count) return false;

    return memcmp(sv_a.data, sv_b.data, sv_b.count) == 0;
}

bool nob_sv_endswith_cstr(Nob_String_View sv, const char* cstr) {
    size_t len = strlen(cstr);
    if (sv.count < len) return false;

    return memcmp(sv.data + (sv.count - len), cstr, len) == 0;
}

bool nob_sv_contains_cstr(Nob_String_View sv, const char* cstr) {
    size_t len = strlen(cstr);
    if (sv.count < len) return false;

    size_t i = 0;
    while (i < sv.count && (sv.count - i - 1) > len) {
        if (memcmp(sv.data + i, cstr, len) == 0 || memcmp(sv.data + (sv.count - i), cstr, len) == 0) {
            return true;
        }
        i++;
    }

    return false;
}

Nob_String_View nob_sv_chop_left_by_n(Nob_String_View sv, size_t n) {
    return nob_sv_from_parts(sv.data + n, sv.count - n);
}

Nob_String_View nob_sv_chop_right_by_n(Nob_String_View sv, size_t n) {
    return nob_sv_from_parts(sv.data, sv.count - n);
}

bool _nob_read_from_pipe(PIPE_TYPE pipe_fds[2], Nob_String_Builder* o_sb) {
    char loc_buf[BUFSIZ];

#ifdef _WIN32
    DWORD dwRead;
    BOOL bSuccess = FALSE;

    do {
        bSuccess = ReadFile(pipe_fds[0], loc_buf, sizeof(loc_buf), &dwRead, NULL);
        if (!bSuccess) {
            return false;
        }
        nob_sb_append_buf(o_sb, loc_buf, dwRead);
    } while (dwRead != 0);
#else
    close(pipe_fds[1]);
    size_t rb = 0;
    do {
        rb = read(pipe_fds[0], loc_buf, sizeof(loc_buf));
        if (rb < 0) {
            return false;
        }
        nob_sb_append_buf(o_sb, loc_buf, rb);
    } while (rb != 0);
#endif

    // the end line (/r/n or /n) sequence(s) is not needed..
    if (o_sb->items[o_sb->count - 1] == '\n') {
        o_sb->count--;
        if (o_sb->items[o_sb->count - 1] == '\r')
            o_sb->count--;
    }

    return true;
}

bool nob_cmd_run_sync_with_output(Nob_Cmd cmd, Nob_String_Builder* o_sb, EXIT_CODE_TYPE* exit_code) {
    if (cmd.count < 1) {
        nob_log(NOB_ERROR, "Could not run empty command");
        if (exit_code) {
            *exit_code = 1;
        }
        return false;
    }

    Nob_String_Builder sb = {0};
    nob_cmd_render(cmd, &sb);
    nob_sb_append_null(&sb);
    nob_log(NOB_INFO, "CMD: %s", sb.items);
    nob_sb_free(sb);
    memset(&sb, 0, sizeof(sb));

    PIPE_TYPE pipe_fds[2];
    EXIT_CODE_TYPE result = 0;
    
#ifdef _WIN32
    // Reference: https://learn.microsoft.com/en-us/windows/win32/ProcThread/creating-a-child-process-with-redirected-input-and-output
    SECURITY_ATTRIBUTES saAttr;
    ZeroMemory(&saAttr, sizeof(saAttr));
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    //               TO_CHILD      FOR_PARENT
    if (!CreatePipe(&pipe_fds[0], &pipe_fds[1], &saAttr, 0)) {
        nob_log(NOB_ERROR, "Could not create pipe for child process: %lu", GetLastError());
        nob_return_defer(1);
    }

    if (!SetHandleInformation(pipe_fds[0], HANDLE_FLAG_INHERIT, 0)) {
        nob_log(NOB_ERROR, "Could not set pipe handle information for child process: %lu", GetLastError());
        nob_return_defer(1);
    }

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bSuccess = FALSE;

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));

    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = pipe_fds[1];
    siStartInfo.hStdOutput = pipe_fds[1];
    siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    // TODO: use a more reliable rendering of the command instead of cmd_render
    // cmd_render is for logging primarily
    nob_cmd_render(cmd, &sb);
    nob_sb_append_null(&sb);
    bSuccess =
        CreateProcessA(NULL, sb.items, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
    nob_sb_free(sb);

    if (!bSuccess) {
        nob_log(NOB_ERROR, "Could not create child process: %lu", GetLastError());
        nob_return_defer(1);
    }

    // wait then read?
    DWORD wait_result = WaitForSingleObject(piProcInfo.hProcess, INFINITE);

    if (wait_result == WAIT_FAILED) {
        nob_log(NOB_ERROR, "could not wait on child process: %lu", GetLastError());
        nob_return_defer(1);
    }

    DWORD exit_status;
    if (!GetExitCodeProcess(piProcInfo.hProcess, &exit_status)) {
        nob_log(NOB_ERROR, "could not get process exit code: %lu", GetLastError());
        nob_return_defer(1);
    }

    if (exit_status != 0) {
        nob_log(NOB_ERROR, "command exited with exit code %lu", exit_status);
    }
    nob_return_defer(exit_status);
#else
    if (pipe(pipe_fds) < 0) {
        nob_log(NOB_ERROR, "Could not create pipe for child process: %s", strerror(errno));
        nob_return_defer(1);
    }

    pid_t cpid = fork();
    if (cpid < 0) {
        nob_log(NOB_ERROR, "Could not fork child process: %s", strerror(errno));
        nob_return_defer(1);
    }

    if (cpid == 0) {
        // NOTE: This leaks a bit of memory in the child process.
        // But do we actually care? It's a one off leak anyway...
        Nob_Cmd cmd_null = {0};
        nob_da_append_many(&cmd_null, cmd.items, cmd.count);
        nob_cmd_append(&cmd_null, NULL);

        // redirect all the STDOUT, STDERR outputs of the child 
        // process to the parent.
        dup2(pipe_fds[1], STDOUT_FILENO);
        dup2(pipe_fds[1], STDERR_FILENO);
        close(pipe_fds[0]);
        close(pipe_fds[1]);

        if (execvp(cmd.items[0], (char * const*) cmd_null.items) < 0) {
            nob_log(NOB_ERROR, "Could not exec child process: %s", strerror(errno));
            exit(1);
        }
        NOB_ASSERT(0 && "unreachable");
    }

    // wait then read?
    for (;;) {
        int wstatus = 0;
        if (waitpid(cpid, &wstatus, 0) < 0) {
            nob_log(NOB_ERROR, "could not wait on command (pid %d): %s", cpid, strerror(errno));
            nob_return_defer(1);
        }

        if (WIFEXITED(wstatus)) {
            int exit_status = WEXITSTATUS(wstatus);
            if (exit_status != 0) {
                nob_log(NOB_ERROR, "command exited with exit code %d", exit_status);
            }
            result = exit_status;
            break;
        }

        if (WIFSIGNALED(wstatus)) {
            nob_log(NOB_ERROR, "command process was terminated by %s", strsignal(WTERMSIG(wstatus)));
            return false;
        }
    }
#endif
    if (!_nob_read_from_pipe(pipe_fds, o_sb)) {
        nob_log(NOB_ERROR, "couldn't read the out of the command %s", strerror(errno));
        nob_return_defer(1);
    }

defer:
#ifdef _WIN32
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    CloseHandle(pipe_fds[1]);
#endif
    if (exit_code) {
        *exit_code = result;
    }
    return result == 0;
}

bool nob_cmd_run_sync_silently(Nob_Cmd cmd, EXIT_CODE_TYPE* exit_code) {
    if (cmd.count < 1) {
        nob_log(NOB_ERROR, "Could not run empty command");
        return false;
    }

    EXIT_CODE_TYPE result = 0;
#ifdef _WIN32
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bSuccess = FALSE;

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));

    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = INVALID_HANDLE_VALUE;
    siStartInfo.hStdOutput = INVALID_HANDLE_VALUE;
    siStartInfo.hStdInput = INVALID_HANDLE_VALUE;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    // TODO: use a more reliable rendering of the command instead of cmd_render
    // cmd_render is for logging primarily
    Nob_String_Builder sb = {0};
    nob_cmd_render(cmd, &sb);
    nob_sb_append_null(&sb);
    bSuccess =
        CreateProcessA(NULL, sb.items, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
    nob_sb_free(sb);

    if (!bSuccess) {
        nob_log(NOB_ERROR, "Could not create child process: %lu", GetLastError());
        nob_return_defer(1);
    }

    DWORD wait_result = WaitForSingleObject(piProcInfo.hProcess, INFINITE);

    if (wait_result == WAIT_FAILED) {
        nob_return_defer(1);
    }

    DWORD exit_status;
    if (!GetExitCodeProcess(piProcInfo.hProcess, &exit_status)) {
        nob_return_defer(1);
    }

    nob_return_defer(exit_status);
#else
    pid_t cpid = fork();
    if (cpid < 0) {
        nob_log(NOB_ERROR, "Could not fork child process: %s", strerror(errno));
        nob_return_defer(1);
    }

    if (cpid == 0) {
        // NOTE: This leaks a bit of memory in the child process.
        // But do we actually care? It's a one off leak anyway...
        Nob_Cmd cmd_null = {0};
        nob_da_append_many(&cmd_null, cmd.items, cmd.count);
        nob_cmd_append(&cmd_null, NULL);

        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        if (execvp(cmd.items[0], (char * const*) cmd_null.items) < 0) {
            nob_log(NOB_ERROR, "Could not exec child process: %s", strerror(errno));
            exit(1);
        }
        NOB_ASSERT(0 && "unreachable");
    }

    // silently wait and return result accordingly...
    for (;;) {
        int wstatus = 0;
        if (waitpid(cpid, &wstatus, 0) < 0) {
            nob_return_defer(1);
        }

        if (WIFEXITED(wstatus)) {
            int exit_status = WEXITSTATUS(wstatus);
            nob_return_defer(exit_status);
        }

        if (WIFSIGNALED(wstatus)) {
            nob_return_defer(1);
        }
    }
#endif

defer:
#ifdef _WIN32
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
#endif
    if (exit_code) {
        *exit_code = result;
    }
    // the caller might be still interested in the output
    // even if the child exitted with a non-zero code.
    return result == 0;
}

bool nob_mkdir_silently_if_not_exists(const char *path)
{
#ifdef _WIN32
    int result = mkdir(path);
#else
    int result = mkdir(path, 0755);
#endif
    if (result < 0) {
        if (errno == EEXIST) {
            return true;
        }
        return false;
    }

    return true;
}

bool _nob_mkdir_recursively_if_not_exists(const char *path, bool stop_at_end) {
    bool result = true;

    Nob_String_View path_sv = nob_sv_from_cstr(path);
    Nob_String_Builder path_sb = {0};
    do {
        char* path_sep = "/";
        Nob_String_View portion_sv = nob_sv_chop_by_delim(&path_sv, path_sep[0]);
        if (nob_sv_eq(path_sv, portion_sv)) {
            path_sep = "\\";
            portion_sv = nob_sv_chop_by_delim(&path_sv, path_sep[0]);
            if (nob_sv_eq(path_sv, portion_sv)) {
                path_sep[0] = '\0'; // going to add two'\0' shouldn't matter...
                if (stop_at_end) {
                    nob_return_defer(true);
                }
            }
        }

        nob_sb_append_buf(&path_sb, portion_sv.data, portion_sv.count);
        nob_sb_append_buf(&path_sb, path_sep, 1);
        nob_sb_append_null(&path_sb);

        if (!nob_mkdir_silently_if_not_exists(path_sb.items)) {
            nob_log(NOB_ERROR, "Couldn't create directory: %s", path_sb.items);
            nob_return_defer(false);
        }

        if (path_sb.count > 0) {
            path_sb.count--;
        }
    } while(path_sv.count > 0);

defer:
    nob_sb_free(path_sb);
    return result;
}

bool nob_mkdir_recursively_if_not_exists(const char *path) {
    if (nob_file_exists(path)) {
        return true;
    }
    return _nob_mkdir_recursively_if_not_exists(path, false);
}

bool nob_mk_file_parentdir_if_not_exists(const char *path) {
    if (nob_file_exists(path)) {
        return true;
    }
    return _nob_mkdir_recursively_if_not_exists(path, true);
}

// RES_VALID_TILL_TMP_RESET
bool nob_find_files_with_extensions(const char* search_dir, const char** extensions_arr, size_t arr_cnt, Nob_File_Paths* outfiles) {
    bool result = true;
    Nob_File_Paths files = {0};
    if (!nob_read_entire_dir(search_dir, &files)) nob_return_defer(0);

    for (size_t i = 0; i < files.count; i++) {
        const char* file = files.items[i];
        if (strcmp(file, ".") == 0) continue;
        if (strcmp(file, "..") == 0) continue;

        const char* path = nob_temp_sprintf("%s/%s", search_dir, file);
        if (nob_get_file_type(path) == NOB_FILE_REGULAR) {
            Nob_String_View sv = nob_sv_from_cstr(path);
            for (size_t j = 0; j < arr_cnt; j++) {
                if (nob_sv_endswith_cstr(sv, extensions_arr[j])) {
                    nob_da_append(outfiles, path);
                    break;
                }
            }
        }
    }

defer:
    nob_da_free(files);
    return result;
}

// RES_VALID_TILL_TMP_RESET
const char* nob_find_path_recursively(const char* search_dir, const char* search_path) {
    char* result = NULL;
    const char* default_path = nob_temp_sprintf("%s/%s", search_dir, search_path);
    if (nob_file_exists(default_path)) {
        nob_return_defer((char*)default_path);
    }

    Nob_File_Paths files = {0};
    if (!nob_read_entire_dir(search_dir, &files)) nob_return_defer(NULL);

    for (size_t i = 0; i < files.count; i++) {
        const char* file = files.items[i];
        if (strcmp(file, ".") == 0) continue;
        if (strcmp(file, "..") == 0) continue;

        char* path = nob_temp_sprintf("%s/%s", search_dir, file);
        Nob_String_View path_sv = nob_sv_from_cstr(path);

        if (nob_sv_endswith_cstr(path_sv, search_path)) {
            nob_return_defer(path);
        }
        
        Nob_File_Type file_type = nob_get_file_type(path);
        if (file_type == NOB_FILE_DIRECTORY) {
            result = (char*)nob_find_path_recursively(path, search_path);
            if (result) {
                nob_return_defer(result);
            }
        } else if (file_type == NOB_FILE_SYMLINK) {
            char* tmp_path = realpath(path, NULL);
            if (!tmp_path) continue;
            path = nob_temp_strdup(tmp_path);
            free(tmp_path);

            result = (char*)nob_find_path_recursively(path, search_path);
            if (result) {
                nob_return_defer(result);
            }
        }
    }
defer:
    return result;
}

// RES_VALID_TILL_TMP_RESET
bool nob_find_paths_recursively(const char* dir, const char** paths, size_t paths_cnt, Nob_File_Paths* outfiles) {
    bool result = false;
    for (size_t i = 0; i < paths_cnt; i++) {
        const char* path = nob_find_path_recursively(dir, paths[i]);
        if (!path) {
            nob_log(NOB_WARNING, "Couldn't find %s in %s", paths[i], dir);
            continue;
        }
        result = true;
        nob_da_append(outfiles, path);
    }

    return result;
}

// RES_VALID_TILL_TMP_RESET
const char* nob_filename_from_path(const char* path) {
    Nob_String_View path_sv = nob_sv_from_cstr(path);
    size_t i = 0;
    size_t j = 0;

    while (i < path_sv.count) {
        char c = path_sv.data[path_sv.count - 1 - i];
        bool is_sep = c =='/';
#ifdef _WIN32
        is_sep |= c == '\\';
#endif
        if (is_sep) {
            if (i - j > 0) {
                break;
            }
            j++;
        }
        i++;
    }

    Nob_String_View filename_with_ext = nob_sv_from_parts(path_sv.data + (path_sv.count - i), i - j);
    Nob_String_View filename = nob_sv_chop_by_delim(&filename_with_ext, '.');
    return nob_temp_sv_to_cstr(filename);
}

// RES_VALID_TILL_TMP_RESET
const char* nob_relative_path_from(const char* from_path, const char* to_path) {
    char* result = NULL;
    Nob_String_Builder from_sb = {0};
    Nob_String_Builder to_sb = {0};

    char* tmp_from_path = realpath(from_path, NULL);
    char* tmp_to_path = realpath(to_path, NULL);

    if (tmp_from_path == NULL || tmp_to_path == NULL) {
        nob_log(NOB_ERROR, "Couldn't find full path of %s or %s", from_path, to_path);
        nob_return_defer(NULL);
    }
    nob_log(NOB_INFO, "REAL_From_Path: %s", tmp_from_path);
    nob_log(NOB_INFO, "REAL_To_Path: %s", tmp_to_path);
    nob_sb_append_cstr(&from_sb, tmp_from_path);
    nob_sb_append_cstr(&to_sb, tmp_to_path);

    Nob_String_View from_sv = nob_sv_from_parts(from_sb.items, from_sb.count);
    Nob_String_View to_sv = nob_sv_from_parts(to_sb.items, to_sb.count);

    // the `from_path` has to be inside `to_path` otherwise we would've to search for whole file system..
    if (!nob_sv_startswith(to_sv, from_sv)) {
        nob_log(NOB_ERROR, "Cannot find relative path from %s to %s.", from_path, to_path);
        nob_return_defer(NULL);
    }

    result = (char*)nob_temp_sv_to_cstr(nob_sv_chop_left_by_n(to_sv, from_sv.count));

defer:
    if (tmp_from_path) {
        free(tmp_from_path);
    }
    if (tmp_to_path) {
        free(tmp_to_path);
    }
    nob_sb_free(from_sb);
    nob_sb_free(to_sb);
    return result;
}


const char* nob_fullpath(const char* path) {
    char* rp = realpath(path, NULL);
    if (!rp) {
        return NULL;
    }

#ifdef _WIN32
    // Windows windows.... _fullpath doesn't care whether or not the provided path is valid...
    if (!nob_file_exists(path)) {
        free(rp);
        return NULL;
    }
#endif

    char* result = nob_temp_strdup(rp);
    free(rp);
    return result;
}

#endif //NOB_EXTENSIONS_IMPLEMENTATION

#endif //NOB_EXTENSIONS_H_
