#ifndef LOG_H
#define LOG_H

#include <string>

// One line to stdout, guarded by a mutex so lines from different threads never mix.
void log_line(const std::string& text);

// Like log_line but only when verbose is on (--verbose). Use it for packet-level noise.
void log_debug(const std::string& text);
void log_set_verbose(bool on);

#endif
