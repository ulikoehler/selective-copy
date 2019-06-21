#define _GNU_SOURCE
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>

#define ASSIGN_DLSYM_OR_DIE(name)			\
        libc_##name = (libc_##name##_##t)(intptr_t)dlsym(RTLD_NEXT, #name);			\
        if (!libc_##name)                       \
        {                                       \
                const char *dlerror_str = dlerror();                          \
                fprintf(stderr, "libopenlog init error for %s: %s\n", #name,\
                        dlerror_str ? dlerror_str : "(null)");                \
                _exit(1);                       \
        }

typedef int (*libc_open_t)(const char*, int, ...);
#ifdef HAVE_OPEN64
typedef int (*libc_open64_t)(const char*, int, ...);
#endif

bool initing = false;
static libc_open_t libc_open = NULL;
#ifdef HAVE_OPEN64
static libc_open64_t libc_open64 = NULL;
#endif

#define VISIBLE __attribute__ ((visibility("default")))
#define INTERNAL __attribute__ ((visibility("hidden")))


static FILE* readOutFile = NULL;
static pthread_mutex_t readOutMutex;
static FILE* writeOutFile = NULL;
static pthread_mutex_t writeOutMutex;
/**
 * Log open 
 */
void INTERNAL libOpenlogLogRead(const char* pathname) {
	if(readOutFile != NULL) {
		pthread_mutex_lock(&readOutMutex);
		fprintf(readOutFile, "%s\n", pathname);
		pthread_mutex_unlock(&readOutMutex);
	}
}

/**
 * Log open 
 */
void INTERNAL libOpenlogLogWrite(const char* pathname) {
	if(writeOutFile != NULL) {
		pthread_mutex_lock(&writeOutMutex);
		fprintf(writeOutFile, "%s\n", pathname);
		pthread_mutex_unlock(&writeOutMutex);
	}
}

void __attribute__ ((constructor)) libopenlog_init(void) {
    initing = true;
	// Load symbols from libc
	ASSIGN_DLSYM_OR_DIE(open);
    #ifdef HAVE_OPEN64
	ASSIGN_DLSYM_OR_DIE(open64);
    #endif
	// Initialize read log file
	char* rlogfile = getenv("LIBOPENLOG_RLOGFILE");
	if(rlogfile == NULL) {
		fprintf(stderr, "libopenlog error: LIBOPENLOG_RLOGFILE environment variable needs to be set to a file or to NULL\n");
		_exit(1);
	}
	if(strncasecmp(rlogfile, "NULL", 4) != 0) {
		readOutFile = fopen(rlogfile, "w");
	}
	// Initialize write log file
	char* wlogfile = getenv("LIBOPENLOG_WLOGFILE");
	if(wlogfile == NULL) {
		fprintf(stderr, "libopenlog error: LIBOPENLOG_WLOGFILE environment variable needs to be set to a file or to NULL\n");
		_exit(1);
	}
	if(strncasecmp(wlogfile, "NULL", 4) != 0) {
		writeOutFile = fopen(wlogfile, "w");
	}
	// Log modes
    pthread_mutex_init(&readOutMutex, NULL /* default mutex attributes */);
    pthread_mutex_init(&writeOutMutex, NULL /* default mutex attributes */);
    initing = false;
}

void __attribute__ ((destructor)) libopenlog_deinit(void) {
	if(readOutFile != NULL) {
		fclose(readOutFile);
	}
	if(writeOutFile != NULL) {
		fclose(writeOutFile);
	}
	pthread_mutex_destroy(&readOutMutex);
	pthread_mutex_destroy(&writeOutMutex);
}

int VISIBLE open(const char* pathname, int flags, ...)
{
	va_list ap;
	mode_t mode;

	va_start(ap, flags);
#if SIZEOF_MODE_T < SIZEOF_INT
	mode= (mode_t) va_arg(ap, int);
#else
	mode= va_arg(ap, mode_t);
#endif
	va_end(ap);

	/* In pthread environments the dlsym() may call our open(). */
	/* We simply ignore it because libc is already loaded       */
	if (initing) {
		errno = EFAULT;
		return -1;
	}

    // Log access to file
	if((flags & O_WRONLY) == 0) {
		libOpenlogLogRead(pathname);
	}
	if((flags & (O_RDWR | O_WRONLY | O_RDONLY)) != O_RDONLY) {
		libOpenlogLogWrite(pathname);
	}

	return (*libc_open)(pathname,flags,mode);
}

#if !defined(__USE_FILE_OFFSET64) && defined(HAVE_OPEN64)
int VISIBLE open64(const char* pathname, int flags, ...)
{
	va_list ap;
	mode_t mode;

	va_start(ap, flags);
#if SIZEOF_MODE_T < SIZEOF_INT
	mode = (mode_t) va_arg(ap, int);
#else
	mode = va_arg(ap, mode_t);
#endif
	va_end(ap);

	/* In pthread environments the dlsym() may call our open(). */
	/* We simply ignore it because libc is already loaded       */
	if (initing) {
		errno = EFAULT;
		return -1;
	}

    // Log access to file
	if(flags & (O_RDONLY | O_RDWR)) {
		libOpenlogLogRead(pathname);
	}
	if(flags & (O_RDWR | O_WRONLY)) {
		libOpenlogLogWrite(pathname);
	}

	return (*libc_open64)(pathname,flags,mode);
}
#endif