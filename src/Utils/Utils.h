#pragma once

#include <string>
#include <vector>

#if _DEBUG
#define DEBUG_OUT(...) fprintf(stdout, __VA_ARGS__)
#else
#define DEBUG_OUT(...)                                                                             \
    do {                                                                                           \
    } while (0);
#endif

#if _DEBUG
#define DEBUG_OUT_COND(cond, ...)                                                                  \
    if (cond) {                                                                                    \
        fprintf(stdout, __VA_ARGS__);                                                              \
    }
#else
#define DEBUG_OUT_COND(cond, ...)                                                                  \
    do {                                                                                           \
    } while (0);
#endif

namespace Utils {
std::string to_lower(std::string str);
std::vector<std::string> split_str(std::string str, const std::string& delimiter);
template <typename T>
inline int sign(T val) {
    return (T(0) < val) - (val < T(0));
}
} // namespace Utils