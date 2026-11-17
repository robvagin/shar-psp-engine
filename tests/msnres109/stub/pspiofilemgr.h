// Заглушка для хост-теста msnres.c на Маке: sceIo* через POSIX
#include <fcntl.h>
#include <unistd.h>
typedef int SceUID;
#define PSP_O_RDONLY O_RDONLY
#define PSP_SEEK_SET SEEK_SET
#define PSP_SEEK_END SEEK_END
static inline SceUID sceIoOpen(const char *p, int fl, int m) { (void)m; return open(p, fl); }
static inline int sceIoClose(SceUID f) { return close(f); }
static inline int sceIoRead(SceUID f, void *b, int n) { return (int)read(f, b, (size_t)n); }
static inline int sceIoLseek32(SceUID f, int o, int w) { return (int)lseek(f, o, w); }
