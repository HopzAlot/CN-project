#include "common/log.h"

#include <atomic>
#include <iostream>
#include <mutex>

static std::mutex out_mu;
static std::atomic<bool> verbose(false);

void log_line(const std::string& text) {
    std::lock_guard<std::mutex> lock(out_mu);
    std::cout << text << std::endl;
}

void log_debug(const std::string& text) {
    if (verbose) log_line("[debug] " + text);
}

void log_set_verbose(bool on) {
    verbose = on;
}
