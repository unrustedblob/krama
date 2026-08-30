// Fatal path handling for program vs system errors

#ifndef FUNC_FATAL_H
#define FUNC_FATAL_H

// Possible paths for the `fatal` function to take on encountering an error
enum FatalPath {
        EXIT,
        ABORT,
};

// Prints a message and exits the program based on `action`. Best called indirectly using the
// `FATAL` macro
[[noreturn]] void fatal(enum FatalPath action, const char *msg, ...);

// Macro to wrap the call to `fatal`
#define FATAL(cond, fatal_path, ...)                                                             \
        do {                                                                                     \
                if ((cond)) {                                                                    \
                        fprintf(stderr, "%s::%s::%d :\n Condition ( %s )\n", __FILE__, __func__, \
                                __LINE__, #cond);                                                \
                        fatal(fatal_path, __VA_ARGS__);                                          \
                }                                                                                \
        } while (false)

#endif // FUNC_FATAL_H
