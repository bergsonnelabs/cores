/**
 * syscalls.c — Minimal newlib syscall stubs for bare-metal builds.
 *
 * Providing these prevents the linker from pulling in nosys.specs stubs,
 * which emit "will always fail" warnings. These implementations simply
 * return appropriate error codes — syscalls are never used in bare-metal code.
 */

#include <sys/stat.h>
#include <errno.h>

int _close(int fd)                             { (void)fd;  errno = ENOSYS; return -1; }
int _lseek(int fd, int ptr, int dir)           { (void)fd;  (void)ptr; (void)dir; errno = ENOSYS; return -1; }
int _read(int fd, char *ptr, int len)          { (void)fd;  (void)ptr; (void)len; errno = ENOSYS; return -1; }
int _write(int fd, char *ptr, int len)         { (void)fd;  (void)ptr; return len; }
int _fstat(int fd, struct stat *st)            { (void)fd;  st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)                            { (void)fd;  return 1; }
int _getpid(void)                              { return 1; }
int _kill(int pid, int sig)                    { (void)pid; (void)sig; errno = EINVAL; return -1; }
