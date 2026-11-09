// Windows feasibility shim (bench only): the POSIX calls libreflexworld uses that MinGW lacks (a real port: a small
// platform.h with these, plus <cmath> / <vector> / <algorithm> includes where the sources rely on transitive ones).
#pragma once
#if defined(_WIN32)
#include <vector>
#include <algorithm>
#include <cmath>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <io.h>
#include <direct.h>
#define mkdir(p, m) _mkdir(p)
// not atomic like POSIX (seek + read); a real port would use ReadFile / WriteFile with an OVERLAPPED offset
static inline long long pread(int fd, void* buf, size_t n, long long off) { if (_lseeki64(fd, off, SEEK_SET) < 0) return -1; return _read(fd, buf, (unsigned)n); }
static inline long long pwrite(int fd, const void* buf, size_t n, long long off) { if (_lseeki64(fd, off, SEEK_SET) < 0) return -1; return _write(fd, buf, (unsigned)n); }
#endif
