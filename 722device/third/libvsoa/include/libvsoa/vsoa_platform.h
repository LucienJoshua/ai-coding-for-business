/*
 * Copyright (c) 2022 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_platform.h Vehicle SOA on different operating systems.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_PLATFORM_H
#define VSOA_PLATFORM_H

#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(__WINDOWS__)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 /* We need Windows Vista or higher version functions */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h> /* We use MinGW pthread environment */
#include <semaphore.h>

#elif defined(__MATRIX653__)
#include <MATRIX653.h>
#include <mx_libc.h>
#include <sys/socket.h>

#else /* Standard UNIX compatible system environment */
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#if defined(__APPLE__)
#include <fcntl.h>
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif
#endif

#if !defined(__WINDOWS__)
#include <net/if.h>
#elif !defined(IF_NAMESIZE)
#define IF_NAMESIZE  16
#endif

#if defined(SYLIXOS) || defined(__linux__)
#include <sys/eventfd.h>
#endif

#if defined(__WINDOWS__)
#define VSOA_NO_IPV6  1
#endif

#if defined(SYLIXOS) || defined(__APPLE__)
#define VSOA_HAS_SIN_LEN  1
#endif

#if defined(SYLIXOS) || defined(__linux__) || defined(__APPLE__)
#define VSOA_INHERIT_NODELAY 1
#endif

/* Memory barrier */
#if defined(__GNUC__)
#define vsoa_memory_barrier()   __sync_synchronize()
#else
#define vsoa_memory_barrier()
#endif

/* Fast lock */
#if defined(__APPLE__)
typedef pthread_mutex_t         vsoa_spin_t;
#define VSOA_SPIN_INITIALIZER   PTHREAD_MUTEX_INITIALIZER
#define vsoa_spin_init(spin)    pthread_mutex_init(spin, NULL)
#define vsoa_spin_destroy(spin) pthread_mutex_destroy(spin)
#define vsoa_spin_lock(spin)    pthread_mutex_lock(spin)
#define vsoa_spin_unlock(spin)  pthread_mutex_unlock(spin)

#elif defined(__MATRIX653__)
typedef MX_UINT8                vsoa_spin_t;
#define VSOA_SPIN_INITIALIZER   0
#define vsoa_spin_init(spin)
#define vsoa_spin_destroy(spin) 
#define vsoa_spin_lock(spin)    mx_libc_mutex_lock(INFINITE_TIME_VALUE)
#define vsoa_spin_unlock(spin)  mx_libc_mutex_unlock()

#else
typedef pthread_spinlock_t      vsoa_spin_t;
#define VSOA_SPIN_INITIALIZER   PTHREAD_SPINLOCK_INITIALIZER
#define vsoa_spin_init(spin)    pthread_spin_init(spin, 0)
#define vsoa_spin_destroy(spin) pthread_spin_destroy(spin)
#define vsoa_spin_lock(spin)    pthread_spin_lock(spin)
#define vsoa_spin_unlock(spin)  pthread_spin_unlock(spin)
#endif

/* Mutex lock */
#if defined(__MATRIX653__)
typedef MX_UINT8                vsoa_mutex_t;
#define VSOA_MUTEX_INITIALIZER  0
#define vsoa_mutex_init(m)      0
#define vsoa_mutex_destroy(m)
#define vsoa_mutex_lock(m)      mx_libc_mutex_lock(INFINITE_TIME_VALUE)
#define vsoa_mutex_unlock(m)    mx_libc_mutex_unlock()

#else
typedef pthread_mutex_t         vsoa_mutex_t;
#define VSOA_MUTEX_INITIALIZER  PTHREAD_MUTEX_INITIALIZER
#define vsoa_mutex_init(m)      pthread_mutex_init(m, NULL)
#define vsoa_mutex_destroy(m)   pthread_mutex_destroy(m)
#define vsoa_mutex_lock(m)      pthread_mutex_lock(m)
#define vsoa_mutex_unlock(m)    pthread_mutex_unlock(m)
#endif

/* Semaphore */
#define VSOA_SEM_WAIT_INFINITE  (-1)
#if defined(__APPLE__)
typedef dispatch_semaphore_t    vsoa_sem_t;
#define vsoa_sem_init(sem, v) \
({ \
    int ret; \
    *(sem) = dispatch_semaphore_create((intptr_t)v); \
    ret = (*(sem) == NULL) ? -1 : 0; \
    ret; \
})
#define vsoa_sem_destroy(sem)   dispatch_release(*(sem))
#define vsoa_sem_post(sem)      dispatch_semaphore_signal(*(sem))
#define vsoa_sem_wait(sem, timeout) \
({ \
    int ret; \
    dispatch_time_t time; \
    if (timeout > 0) { \
        time = dispatch_time(DISPATCH_TIME_NOW, timeout); \
    } else if (timeout == 0) { \
        time = DISPATCH_TIME_NOW; \
    } else { \
        time = DISPATCH_TIME_FOREVER; \
    } \
    ret = dispatch_semaphore_wait(*(sem), time) ? -1 : 0; \
    ret; \
})
#define vsoa_sem_clear(sem) \
{ \
    while (0 == dispatch_semaphore_wait(*(sem), DISPATCH_TIME_NOW)); \
}

#elif defined(__MATRIX653__)
typedef SEMAPHORE_ID_TYPE       vsoa_sem_t;
#define vsoa_sem_init(sem, v) \
({ \
    int ret; \
    RETURN_CODE_TYPE rc; \
    CREATE_SEMAPHORE("", (SEMAPHORE_VALUE_TYPE)v, MAX_SEMAPHORE_VALUE, FIFO, sem, &rc); \
    ret = (rc == NO_ERROR) ? 0 : -1; \
    ret; \
})
#define vsoa_sem_destroy(sem)
#define vsoa_sem_post(sem) \
{ \
    RETURN_CODE_TYPE rc; \
    SIGNAL_SEMAPHORE(*(sem), &rc); \
}
#define vsoa_sem_wait(sem, timeout) \
({ \
    int ret; \
    RETURN_CODE_TYPE rc; \
    SYSTEM_TIME_TYPE to = timeout < 0 ? INFINITE_TIME_VALUE : timeout; \
    WAIT_SEMAPHORE(*(sem), to, &rc); \
    ret = (rc == NO_ERROR) ? 0 : -1; \
    ret; \
})
#define vsoa_sem_clear(sem) \
{ \
    RETURN_CODE_TYPE rc; \
    do { \
        WAIT_SEMAPHORE(*(sem), 0, &rc); \
    } while (rc == NO_ERROR); \
}

#else
typedef sem_t                   vsoa_sem_t;
#define vsoa_sem_init(sem, v)   sem_init(sem, 0, v)
#define vsoa_sem_destroy(sem)   sem_destroy(sem)
#define vsoa_sem_post(sem)      sem_post(sem)
#define vsoa_sem_wait(sem, timeout) \
({ \
    int ret; \
    if (timeout < 0) { \
        do { \
            ret = sem_wait(sem); \
        } while (ret < 0 && errno == EINTR); \
    } else if (timeout == 0) { \
        ret = sem_trywait(sem); \
    } else { \
        struct timespec abstime; \
        clock_gettime(CLOCK_REALTIME, &abstime); \
        abstime.tv_sec  += timeout / 1000000000; \
        abstime.tv_nsec += timeout % 1000000000; \
        if (abstime.tv_nsec >= 1000000000) { \
            abstime.tv_nsec -= 1000000000; \
            abstime.tv_sec++; \
        } \
        do { \
            ret = sem_timedwait(sem, &abstime); \
        } while (ret < 0 && errno == EINTR); \
    } \
    ret; \
})
#define vsoa_sem_clear(sem) \
{ \
    int cnt = 0; \
    sem_getvalue(sem, &cnt); \
    while (cnt-- > 0) { \
        sem_trywait(sem); \
    } \
}
#endif

/* Multi-threading */
#if defined(__MATRIX653__)
typedef PROCESS_ID_TYPE                 vsoa_thread_t;
#define vsoa_thread_create(id, f, a) \
({ \
    int ret; \
    PROCESS_ATTRIBUTE_TYPE attr = {  .ENTRY_POINT   = (SYSTEM_ADDRESS_TYPE)f, \
                                     .STACK_SIZE    = (STACK_SIZE_TYPE)(16 * 1024), \
                                     .BASE_PRIORITY = (PRIORITY_TYPE)64, \
                                     .PERIOD        = (SYSTEM_TIME_TYPE)INFINITE_TIME_VALUE, \
                                     .TIME_CAPACITY = (SYSTEM_TIME_TYPE)INFINITE_TIME_VALUE, \
                                     .DEADLINE      = (DEADLINE_TYPE)SOFT }; \
    strcpy(attr.NAME, "t_vsoa"); \
    if (NO_ERROR == mx_process_create(&attr, a, id)) { \
        mx_process_init_libc(*(id)); \
        mx_process_start(*(id)); \
        ret = 0; \
    } else { \
        ret = -1; \
    } \
    ret; \
})
#define vsoa_thread_wait(id) \
do { \
    PROCESS_STATUS_TYPE status; \
    if (NO_ERROR == mx_process_get_status(*(id), &status)) { \
        if (status.PROCESS_STATE == DORMANT || status.PROCESS_STATE == FAULTED) { \
            break; \
        } else { \
            mx_process_timed_wait(10 * MX_NANOSEC_PER_MILLISEC); \
        } \
    } else { \
        break; \
    } \
} while (true)
#define vsoa_thread_exit()              mx_process_stop_self()
#define vsoa_thread_msleep(ms)          mx_process_timed_wait((SYSTEM_TIME_TYPE)(ms) * MX_NANOSEC_PER_MILLISEC)

#else
typedef pthread_t                       vsoa_thread_t;
#define vsoa_thread_create(id, f, a)    pthread_create(id, NULL, f, a)
#define vsoa_thread_wait(id)            pthread_join(*(id), NULL)
#define vsoa_thread_exit()              pthread_exit(NULL)
#if defined(SYLIXOS)
#define vsoa_thread_msleep(ms)          Lw_Time_MSleep(ms)
#elif defined(__WINDOWS__)
#define vsoa_thread_msleep(ms)          Sleep(ms)
#else
#define vsoa_thread_msleep(ms)          usleep((ms) * 1000)
#endif
#endif

/* System current running time (ns) */
#if defined(__MATRIX653__)
#define vsoa_current_time() \
({ \
    SYSTEM_TIME_TYPE current; \
    RETURN_CODE_TYPE retcode; \
    GET_TIME(&current, &retcode); \
    (int64_t)current; \
})
#else
#define vsoa_current_time() \
({ \
    int64_t current; \
    struct timespec ts; \
    clock_gettime(CLOCK_MONOTONIC, &ts); \
    current = (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec; \
    current; \
})
#endif

/* MacOS does not have following macro */
#if defined(__APPLE__)
#define TCP_KEEPIDLE    TCP_KEEPALIVE
#endif

/* Some OS unsupport this flag */
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT    0
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL    0
#endif

/* errno in Windows */
#if defined(__WINDOWS__)
#ifndef EWOULDBLOCK
#define EWOULDBLOCK     WSAEWOULDBLOCK
#endif

#ifndef EALREADY
#define EALREADY        WSAEALREADY
#endif

#ifndef EINPROGRESS
#define EINPROGRESS     WSAEINPROGRESS
#endif
#endif

/*
 * Windows missing string functions
 */
#if defined(__WINDOWS__)
#define bzero(p, s)     memset(p, 0, s)
#define index(s, c)     strchr(s, c)
#define rindex(s, c)    strrchr(s, c)
#endif

/*
 * Ioctl socket
 */
#if defined(__WINDOWS__) || defined(__MATRIX653__)
#define ioctl(s, c, a)  ioctlsocket(s, c, a)
#endif

/*
 * Close socket
 */
#if defined(__WINDOWS__) || defined(__MATRIX653__)
#define close_socket(s)  closesocket(s)
#else
#define close_socket(s)  close(s)
#endif

/*
 * Shutdown socket
 */
#if defined(__WINDOWS__)
#define shutdown_socket(s)  shutdown(s, SD_BOTH)
#else
#define shutdown_socket(s)  shutdown(s, SHUT_RDWR)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Create socket
 */
static inline int create_socket (int f, int t, int p, bool n)
{
#if defined(SYLIXOS) || defined(__linux__)
    int s;

    if (n) {
        t |= SOCK_NONBLOCK;
    }
    s = socket(f, t | SOCK_CLOEXEC, p);
#else
    int s, on = 1;

    s = socket(f, t, p);
    if (n && s >= 0) {
        ioctl(s, FIONBIO, &on);
    }
#endif

    return  (s);
}

/*
 * Socket set linger time
 */
static inline void vsoa_socket_linger (int s, int time)
{
    struct linger linger = {
        .l_onoff = 1, .l_linger = time
    };

    setsockopt(s, SOL_SOCKET, SO_LINGER, &linger, sizeof(struct linger));
}

/*
 * Socket set priority
 */
static inline void vsoa_socket_priority (int s, int prio)
{
#if defined(SO_PRIORITY)
#ifdef TCP_REALTIME
    int on = 1;

    setsockopt(s, IPPROTO_TCP, TCP_REALTIME, &on, sizeof(int));
#endif /* TCP_REALTIME */
    setsockopt(s, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(int));
#elif defined(SO_NET_SERVICE_TYPE)
    int type;

    if (prio < 1){
        type = NET_SERVICE_TYPE_BE;
    } else if (prio < 3) {
        type = NET_SERVICE_TYPE_RD;
    } else if (prio < 5) {
        type = NET_SERVICE_TYPE_RV;
    } else {
        type = NET_SERVICE_TYPE_VO;
    }
    setsockopt(s, SOL_SOCKET, SO_NET_SERVICE_TYPE, &type, sizeof(int));
#else /* SO_PRIORITY */
    int dscp = prio << 5;

    if (prio) {
        dscp |= 0x08;
    }
    setsockopt(s, IPPROTO_IP, IP_TOS, &dscp, sizeof(int));
#endif /* !SO_PRIORITY */
}

/*
 * Socket send timeout
 */
static inline void vsoa_socket_sndto (int s, const struct timeval *timeout)
{
#if defined(__WINDOWS__)
    DWORD dwto;

    if (timeout) {
        dwto = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
    } else {
        dwto = 0;
    }
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&dwto, sizeof(DWORD));
#else
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, timeout, sizeof(struct timeval));
#endif
}

/*
 * Bind socket to interface
 */
static inline bool vsoa_socket_bindif (int s, const char *ifname)
{
#if defined(SO_BINDTODEVICE)
    struct ifreq ifreq;

    if (!ifname || strlen(ifname) >= IF_NAMESIZE) {
        return  (false);
    }

    strcpy(ifreq.ifr_name, ifname);

    if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE, (const void *)&ifreq, sizeof(struct ifreq))) {
        return  (false);
    } else {
        return  (true);
    }

#else
    return  (false);
#endif
}

/*
 * Create event pair
 */
static inline bool vsoa_event_pair_create (int evtfd[2])
{
#if defined(SYLIXOS) || defined(__linux__)
    evtfd[0] = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (evtfd[0] < 0) {
        return  (false);
    }
    evtfd[1] = evtfd[0];

#elif defined(__APPLE__)
    int flags;

    if (pipe(evtfd)) {
        return  (false);
    }

    flags  = ioctl(evtfd[0], F_GETFL);
    flags |= O_NONBLOCK | O_CLOEXEC;
    ioctl(evtfd[0], F_SETFL, flags);

    flags  = ioctl(evtfd[1], F_GETFL);
    flags |= O_CLOEXEC;
    ioctl(evtfd[1], F_SETFL, flags);

#else
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    evtfd[0] = create_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, true);
    if (evtfd[0] < 0) {
        return  (false);
    }

    evtfd[1] = create_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, true);
    if (evtfd[1] < 0) {
        close_socket(evtfd[0]);
        return  (false);
    }

    bzero(&addr, sizeof(struct sockaddr_in));

#ifdef VSOA_HAS_SIN_LEN
    addr.sin_len = addr_len;
#endif

    addr.sin_family      = AF_INET;
    addr.sin_port        = 0;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    bind(evtfd[0], (struct sockaddr *)&addr, addr_len);
    getsockname(evtfd[0], (struct sockaddr *)&addr, &addr_len);
    connect(evtfd[1], (struct sockaddr *)&addr, addr_len);
#endif

    return  (true);
}

/*
 * Close event pair
 */
static inline void vsoa_event_pair_close (int evtfd[2])
{
#if defined(SYLIXOS) || defined(__linux__)
    close(evtfd[0]);
#elif defined(__APPLE__)
    close(evtfd[0]);
    close(evtfd[1]);
#else
    close_socket(evtfd[0]);
    close_socket(evtfd[1]);
#endif

    evtfd[1] = evtfd[0] = -1;
}

/*
 * Signal event pair
 */
static inline void vsoa_event_pair_signal (int wfd)
{
#if defined(SYLIXOS) || defined(__linux__)
    eventfd_t  event = 1;
    eventfd_write(wfd, event);
#elif defined(__APPLE__)
    uint8_t  event = 1;
    write(wfd, &event, 1);
#else
    uint8_t  event = 1;
    send(wfd, &event, 1, MSG_NOSIGNAL);
#endif
}

/*
 * Read event pair
 */
static inline void vsoa_event_pair_fetch (int rfd)
{
#if defined(SYLIXOS) || defined(__linux__)
    eventfd_t  event;
    eventfd_read(rfd, &event);
#elif defined(__APPLE__)
    uint8_t  event;
    read(rfd, &event, 1);
#else
    uint8_t  event;
    recv(rfd, &event, 1, MSG_DONTWAIT);
#endif
}

/*
 * Multiplexing IO Select
 */
static inline int vsoa_select (int w, fd_set *rfds, fd_set *wfds, fd_set *efds, const struct timespec *timeout)
{
#if defined(__WINDOWS__) || defined(__MATRIX653__)
    struct timeval to, *pto;

    if (timeout) {
        to.tv_sec  = timeout->tv_sec;
        to.tv_usec = timeout->tv_nsec / 1000;
        pto        = &to;
    } else {
        pto = NULL;
    }

#if defined(__WINDOWS__)
    return  (select(w, rfds, wfds, efds, pto));
#else
    return  (lwip_select(w, rfds, wfds, efds, pto));
#endif
#else
    return  (pselect(w, rfds, wfds, efds, timeout, NULL));
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* VSOA_PLATFORM_H */
/*
 * end
 */
