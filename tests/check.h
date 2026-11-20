#ifndef CHECK_H
#define CHECK_H

#include <cstdio>

// Tiny test helper: CHECK prints the failing line and counts it, main returns test_result().
static int check_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
            check_failures++;                                                \
        }                                                                    \
    } while (0)

static int test_result(const char* name) {
    if (check_failures == 0) std::printf("PASS %s\n", name);
    else std::printf("%d failure(s) in %s\n", check_failures, name);
    return check_failures == 0 ? 0 : 1;
}

#endif
