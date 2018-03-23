#define  _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <linux/input.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/* Link against the rt library; -lrt. */

#define UNUSED          __attribute__((unused))
#define TIMEOUT_SIGNAL  (SIGRTMAX-0)

/*
 * done - flag used to exit program at SIGINT, SIGTERM etc.
*/

volatile sig_atomic_t done = 0;

static void handle_done(int signum UNUSED)
{
    done = 1;
}

int install_done(const int signum)
{
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_handler = handle_done;
    act.sa_flags = 0;
    if (sigaction(signum, &act, NULL) == -1)
        return errno;
    return 0;
}

/*
 * Barcode input event device, and associated timeout timer.
*/

typedef struct {
    int             fd;
    volatile int    timeout;
    timer_t         timer;
} barcode_dev;

static void handle_timeout(int signum UNUSED, siginfo_t *info, void *context UNUSED)
{
    if (info && info->si_code == SI_TIMER && info->si_value.sival_ptr)
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)
        __atomic_add_fetch((int *)info->si_value.sival_ptr, 1, __ATOMIC_SEQ_CST);
#else
        __sync_add_and_fetch((int *)info->si_value.sival_ptr, 1);
#endif
}

static int install_timeouts(void)
{
    static int installed = 0;

    if (!installed) {
        struct sigaction act;
        sigemptyset(&act.sa_mask);
        act.sa_sigaction = handle_timeout;
        act.sa_flags = SA_SIGINFO;
        if (sigaction(TIMEOUT_SIGNAL, &act, NULL) == -1)
            return errno;
        installed = 1;        
    }

    return 0;
}

int barcode_close(barcode_dev *const dev)
{
    int retval = 0;

    if (!dev)
        return 0;

    if (dev->fd != -1)
        if (close(dev->fd) == -1)
            retval = errno;

    dev->fd = -1;

    if (dev->timer)
        if (timer_delete(dev->timer) == -1)
            if (!retval)
                retval = errno;

    dev->timer = (timer_t)0;

    /* Handle all pending TIMEOUT_SIGNALs */
    while (1) {
        struct timespec t;
        siginfo_t info;
        sigset_t s;

        t.tv_sec = (time_t)0;
        t.tv_nsec = 0L;
        sigemptyset(&s);

        if (sigtimedwait(&s, &info, &t) != TIMEOUT_SIGNAL)
            break;

        if (info.si_code != SI_TIMER || !info.si_value.sival_ptr)
            continue;

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)
        __atomic_add_fetch((int *)info.si_value.sival_ptr, 1, __ATOMIC_SEQ_CST);
#else
        __sync_add_and_fetch((int *)info.si_value.sival_ptr, 1);
#endif
    }

    return errno = retval;
}

int barcode_open(barcode_dev *const dev, const char *const device_path)
{
    struct sigevent event;
    int fd;

    if (!dev)
        return errno = EINVAL;

    dev->fd = -1;
    dev->timeout = -1;
    dev->timer = (timer_t)0;

    if (!device_path || !*device_path)
        return errno = EINVAL;

    if (install_timeouts())
        return errno;

    do {
        fd = open(device_path, O_RDONLY | O_NOCTTY | O_CLOEXEC);
    } while (fd == -1 && errno == EINTR);
    if (fd == -1)
        return errno;

    errno = 0;
    if (ioctl(fd, EVIOCGRAB, 1)) {
        const int saved_errno = errno;
        close(fd);
        return errno = (saved_errno) ? errno : EACCES;
    }

    dev->fd = fd;

    memset(&event, 0, sizeof event);
    event.sigev_notify = SIGEV_SIGNAL;
    event.sigev_signo = TIMEOUT_SIGNAL;
    event.sigev_value.sival_ptr = (void *)&(dev->timeout);
    if (timer_create(CLOCK_REALTIME, &event, &dev->timer) == -1) {
        const int saved_errno = errno;
        close(fd);
        return errno = (saved_errno) ? errno : EMFILE;
    }

    return errno = 0;
}

size_t barcode_read(barcode_dev *const dev,
                    char *const buffer, const size_t length,
                    const unsigned long maximum_ms)
{
    memset(buffer, 0, length);
    struct itimerspec it;
    size_t len = 0;
    int status = ETIMEDOUT;

    if (!dev || !buffer || length < 2 || maximum_ms < 1UL) {
        errno = EINVAL;
        return (size_t)0;
    }

    /* Initial timeout. */
    it.it_value.tv_sec = maximum_ms / 1000UL;
    it.it_value.tv_nsec = (maximum_ms % 1000UL) * 1000000L;

    /* After elapsing, fire every 10 ms. */
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_nsec = 10000000L;

    if (timer_settime(dev->timer, 0, &it, NULL) == -1)
        return (size_t)0;

    /* Because of the repeated elapsing, it is safe to
     * clear the timeout flag here. */
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR >= 7)
    __atomic_store_n((int *)&(dev->timeout), 0, __ATOMIC_SEQ_CST);
#else
    __sync_fetch_and_and((int *)&(dev->timeout), 0);
#endif

    while (!dev->timeout) {
        struct input_event ev;
        ssize_t n;
        int digit;

        n = read(dev->fd, &ev, sizeof ev);
        if (n == (ssize_t)-1) {
            if (errno == EINTR)
                continue;
            status = errno;
            break;

        } else
        if (n == sizeof ev) {

            /* We consider only key presses and autorepeats. */
            if (ev.type != EV_KEY || (ev.value != 1 && ev.value != 2))
                continue;

            switch (ev.code) {
            case KEY_0: digit = '0'; break;
            case KEY_1: digit = '1'; break;
            case KEY_2: digit = '2'; break;
            case KEY_3: digit = '3'; break;
            case KEY_4: digit = '4'; break;
            case KEY_5: digit = '5'; break;
            case KEY_6: digit = '6'; break;
            case KEY_7: digit = '7'; break;
            case KEY_8: digit = '8'; break;
            case KEY_9: digit = '9'; break;
            default:    digit = '\0';
            }

            /* Non-digit key ends the code, except at beginning of code. */
            if (digit == '\0') {
                if (!len)
                    continue;
                status = 0;
                break;
            }

            if (len < length)
                buffer[len] = digit;
            len++;

            continue;

        } else
        if (n == (ssize_t)0) {
            status = ENOENT;
            break;                

        } else {
            status = EIO;
            break;
        }
    }

    /* Cancel timeout. */
    it.it_value.tv_sec = 0;
    it.it_value.tv_nsec = 0;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_nsec = 0L;
    (void)timer_settime(dev->timer, 0, &it, NULL);

    errno = status;
    return len;
}
