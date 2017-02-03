/*
 * interpose.c
 * smarguet
 * 7/10/01
 *
 * interpose on syscalls.
 *
 * hacked up for uthreads by pdemoreu. hacked up again by pgriess.
 * hacked up for linux's way of doing dlsym by scannell.
 * spring cleaning and slightly improved open provided by eric.
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <dirent.h>
#ifdef __linux__
#include <libintl.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>

#include "uthread.h"
#include "uthread_sched.h"

#ifdef __APPLE__
#define LIBC_LOCATION "/usr/lib/libSystem.B.dylib"
#elif __linux__
#define LIBC_LOCATION "/lib/x86_64-linux-gnu/libc.so.6"
#endif

#define INTERPOSE_NEED_DLOPEN
int     usr_getdents(int fd, struct dirent *buf, const size_t count);
int     usr_access(const char *file, int mode);
int     usr_chdir(const char *file);
int     usr_fchdir(int fd);
int     usr_close(int fd);
int     usr_open(const char *pathname, int flags, ...);
pid_t   usr_fork(void);
off_t   usr_lseek(int fd, off_t offset, int whence);
ssize_t usr_write(int fd, const void *buf, size_t count);
ssize_t usr_read(int fd, void* buf, const size_t count);
void    usr_perror(const char *s);

/* need to define some pragmas to get these functions called. */
#ifndef __APPLE__
#pragma weak read=usr_read
#pragma weak write=usr_write
#pragma weak open=usr_open
#pragma weak close=usr_close
#pragma weak lseek=usr_lseek
#pragma weak chdir=usr_chdir
#pragma weak fchdir=usr_fchdir
#pragma weak access=usr_access
#pragma weak getdents=usr_getdents
#pragma weak fork=usr_fork
#pragma weak perror=usr_perror

#pragma weak __read=usr_read
#pragma weak __write=usr_write
#pragma weak __open=usr_open
#pragma weak __close=usr_close
#pragma weak __lseek=usr_lseek
#pragma weak __chdir=usr_chdir
#pragma weak __fchdir=usr_fchdir
#pragma weak __access=usr_access
#pragma weak __getdents=usr_getdents
#pragma weak __fork=usr_fork
#pragma weak __perror=usr_perror
#endif

void
usr_perror(const char *s)
{
    /* fprintf will reschedule by calling write */
    fprintf(stderr, "%s: %s", s, strerror(errno));
}

int
usr_access(const char *file, int mode)
{
    typedef	int (*access_func)(const char *, int);
    access_func	real_access;
    int ret;
    void* libhandle;

    uthread_yield();

    libhandle = dlopen(LIBC_LOCATION, RTLD_LAZY);
    real_access = (access_func)dlsym(libhandle, "access");

    ret = real_access(file, mode);

    dlclose(libhandle);

    return ret;
}

int 
usr_chdir(const char *file)
{
    typedef	int	(*chdir_func)(const char *file);
    chdir_func	real_chdir;
    int ret;
    void* libhandle;

    uthread_yield();

    libhandle = dlopen(LIBC_LOCATION, RTLD_LAZY);
    real_chdir = (chdir_func)dlsym(libhandle, "chdir");

    ret = real_chdir(file); 

    dlclose(libhandle);

    return ret;
}

int 
usr_fchdir(int fd)
{
    typedef	int	(*fchdir_func)(int fd);
    fchdir_func	real_fchdir;
    int ret;
    void* libhandle;

    uthread_yield();

    libhandle = dlopen(LIBC_LOCATION, RTLD_LAZY);
    real_fchdir = (fchdir_func)dlsym(libhandle, "chdir");

    ret = real_fchdir(fd); 

    dlclose(libhandle);

    return ret; 
}

pid_t
usr_fork(void)
{
    typedef pid_t (*fork_func)(void);
    fork_func	real_fork;
    int			child;

    void* libhandle = dlopen(LIBC_LOCATION, RTLD_LAZY);
    real_fork = (fork_func)dlsym(libhandle, "fork");

    /* the child process inherits a copy of all of the uthreads.
     * This may not be desirable.
     */
    child = real_fork();

    dlclose(libhandle);

    return child;
}

ssize_t
usr_write(int fd, const void *buf, size_t count)
{
    typedef	int	(*write_func)(int fd, const void *buf, size_t count);
    write_func real_write;
    int ret;
    void* libhandle;

    uthread_yield();

    libhandle = dlopen(LIBC_LOCATION, RTLD_LAZY);
    real_write = (write_func)dlsym(libhandle, "write");

    ret = real_write(fd,buf,count);

    dlclose(libhandle);

    return ret;
}

off_t
usr_lseek(int fd, off_t offset, int whence)
{
    typedef int	(*lseek_func)(int fd, off_t offset, int whence);
    lseek_func	real_lseek;
    int ret;
    void* libhandle;

    uthread_yield();

    libhandle = dlopen(LIBC_LOCATION, RTLD_LAZY);
    real_lseek = (lseek_func)dlsym(libhandle, "lseek");

    ret = real_lseek(fd, offset, whence);

    dlclose(libhandle);

    return ret;
}

int
usr_getdents(int fd, struct dirent *buf, size_t count)
{
    typedef int	(*getdents_func)(int fd, struct dirent *buf, size_t count);
    getdents_func	real_getdents;
    int ret;
    void* libhandle;

    uthread_yield();

    libhandle = dlopen(LIBC_LOCATION, RTLD_LAZY);
    real_getdents = (getdents_func)dlsym(libhandle, "getdents");

    ret = real_getdents(fd, buf, count);

    dlclose(libhandle);

    return ret; 
}

int
usr_close(int fd)
{
    typedef int	(*close_func)(int fd);
    close_func	real_close;
    int ret;
    void* libhandle;

    uthread_yield();

    libhandle = dlopen(LIBC_LOCATION, RTLD_LAZY);
    real_close = (close_func)dlsym(libhandle, "close");

    ret = real_close(fd);

    dlclose(libhandle);

    return ret;
}


int
usr_open(const char *pathname, int flags, ...)
{
    typedef int	(*open_func)(const char *pathname, int flags, ...);
    open_func	real_open;
    int ret;
    void* libhandle;

    uthread_yield();

    libhandle = dlopen(LIBC_LOCATION, RTLD_LAZY);
    real_open = (open_func)dlsym(libhandle, "open");

    if (flags & O_CREAT) {
        /* mode needs to be supplied if O_CREAT is
         * part of the flags -- otherwise, it is
         * ignored.
         */
        va_list ap;
        va_start(ap, flags);
#ifdef __APPLE__

        int mode = va_arg(ap, int);
        va_end(ap);
        ret = real_open(pathname, flags, mode);

#else

        mode_t mode = va_arg(ap, mode_t);
        va_end(ap);
        ret = real_open(pathname, flags, mode);

#endif

    } else {
        ret = real_open(pathname, flags);
    }

    dlclose(libhandle);

    return ret;
}

ssize_t
usr_read(const int fd, void* buf, const size_t count)
{
    typedef int	(*read_func)(const int fd, void *buf, const size_t count);
    read_func	real_read;
    int ret;
    void* libhandle;

    uthread_yield();

    libhandle = dlopen(LIBC_LOCATION, RTLD_LAZY);
    real_read = (read_func)dlsym(libhandle, "read");

    ret = real_read(fd, buf, count);    

    dlclose(libhandle);

    return ret;
}

