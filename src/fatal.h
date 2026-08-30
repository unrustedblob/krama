// Fatal path handling for program vs system errors

#ifndef FUNC_FATAL_H
#define FUNC_FATAL_H

enum FatalPath {
        EXIT,
        ABORT,
};

[[noreturn]] void fatal(enum FatalPath action, const char *msg, ...);

#define FATAL(cond, fatal_path, ...)                                                             \
        do {                                                                                     \
                if ((cond)) {                                                                    \
                        fprintf(stderr, "%s::%s::%d :\n Condition ( %s )\n", __FILE__, __func__, \
                                __LINE__, #cond);                                                \
                        fatal(fatal_path, __VA_ARGS__);                                          \
                }                                                                                \
        } while (false)

#endif // FUNC_FATAL_H
