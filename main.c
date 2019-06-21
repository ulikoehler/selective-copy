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
#include <sys/socket.h>
#include <arpa/inet.h>
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

static const char* prefix = NULL; // Only paths with this (absolute) prefix will be considered
static size_t prefixLength = 0;

static int readOutSock = -1;
static pthread_mutex_t readOutMutex;
static int writeOutSock = -1;
static pthread_mutex_t writeOutMutex;

void INTERNAL libOpenlogLogRead(const char* pathname) {
	if(readOutSock != -1) {
		pthread_mutex_lock(&readOutMutex);
		// Get actual path
		char* abspath = realpath(pathname, NULL);
		// Check prefix, if it doesnt match, ignore
		if (prefix != NULL && strncmp(abspath, prefix, prefixLength) != 0) {
			return;
		}
		// Write to socket
		write(readOutSock, abspath, strlen(abspath));
		write(readOutSock, "\n", 1);
		// Cleanup
		free(abspath);
		pthread_mutex_unlock(&readOutMutex);
	}
}

void INTERNAL libOpenlogLogWrite(const char* pathname) {
	if(writeOutSock != -1) {
		pthread_mutex_lock(&writeOutMutex);
		// Get actual path
		char* abspath = realpath(pathname, NULL);
		// Check prefix, if it doesnt match, ignore
		if (prefix != NULL && strncmp(abspath, prefix, prefixLength) != 0) {
			return;
		}
		// Write to socket
		write(writeOutSock, abspath, strlen(abspath));
		write(writeOutSock, "\n", 1);
		// Cleanup
		free(abspath);
		pthread_mutex_unlock(&writeOutMutex);
	}
}

int INTERNAL openSock(const char* host, int port) {
	int connfd;
	struct sockaddr_in server;
	
	//Create socket
	int sockfd = socket(AF_INET , SOCK_STREAM , 0);
	if (sockfd == -1) {
        fprintf(stderr, "libopenlog: Creating TCP socket failed.\n"); 
        _exit(2); 
		return -1;
	}
	
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(host);
	server.sin_port = htons(port);

    if (connect(sockfd, &server, sizeof(server)) != 0) { 
        fprintf(stderr, "libopenlog: Connecting to %s port %d failed.\n", host, port); 
        _exit(2);
		return -1;
    }
	return sockfd;
}

void __attribute__ ((constructor)) libopenlog_init(void) {
    initing = true;
	// Load symbols from libc
	ASSIGN_DLSYM_OR_DIE(open);
    #ifdef HAVE_OPEN64
	ASSIGN_DLSYM_OR_DIE(open64);
    #endif
	// Get prefix of paths to consider
	prefix = (const char*)getenv("LIBOPENLOG_PREFIX");
 	if(prefix != NULL) {
		prefixLength = strlen(prefix);
 	}
	// Initialize read log file
	readOutSock = openSock("127.0.0.1", 13485);
	writeOutSock = openSock("127.0.0.1", 13486);
	// Log modes
    pthread_mutex_init(&readOutMutex, NULL /* default mutex attributes */);
    pthread_mutex_init(&writeOutMutex, NULL /* default mutex attributes */);
    initing = false;
}

void __attribute__ ((destructor)) libopenlog_deinit(void) {
	if(readOutSock != -1) {
		close(readOutSock);
	}
	if(writeOutSock != -1) {
		close(writeOutSock);
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