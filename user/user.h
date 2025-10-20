#ifndef USER_INC_USER_H
#define USER_INC_USER_H

#ifndef NULL
#define NULL ((void *) 0)
#endif

struct timeval;
struct sockaddr;
struct stat;
struct in_addr;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int ioctl(int, unsigned long, void*);
int mount(const char *dev, const char *path, const char *fs_type);
int ls_ext2(char *path);
int gettimeofday0(struct timeval *tv, void *tz);
int socket(int, int, int);
int bind(int, struct sockaddr*, int);
int recvfrom(int, char*, int, struct sockaddr*, int*);
int sendto(int, char*, int, struct sockaddr*, int);
int connect(int, struct sockaddr*, int);
int listen(int, int);
int accept(int, struct sockaddr*, int*);
int recv(int, char*, int);
int send(int, char*, int);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
uint16_t htons(uint16_t);
uint16_t ntohs(uint16_t);
uint32_t htonl(uint32_t);
uint32_t ntohl(uint32_t);
long strtol(const char*, char**, int);
int inet_pton(int, const char*, void*);

// strtoul.c
unsigned long strtoul(const char *nptr, char **endptr, int base);

#endif
