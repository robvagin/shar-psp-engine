#ifndef STUB_PSPIO_H
#define STUB_PSPIO_H
#include <fcntl.h>
#include <unistd.h>
typedef int SceUID;
#define PSP_O_RDONLY O_RDONLY
static inline int sceIoOpen(const char *p, int f, int m) { return open(p, f, m); }
static inline int sceIoRead(int f, void *p, int n) { return (int)read(f, p, n); }
static inline int sceIoClose(int f) { return close(f); }
#endif
