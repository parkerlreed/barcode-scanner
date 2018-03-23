#ifndef   BARCODE_H
#define   BARCODE_H
#include <stdlib.h>
#include <signal.h>

/* This flags turns nonzero if any signal
 * installed with install_done is caught.
*/
extern volatile sig_atomic_t done;

/* Install signals that set 'done'.
*/
int install_done(const int signum);

/* Barcode device description.
 * Do not meddle with the internals;
 * this is here only to allow you
 * to allocate one statically.
*/
typedef struct {
    int             fd;
    volatile int    timeout;
    timer_t         timer;
} barcode_dev;

/* Close a barcode device.
 * Returns 0 if success, nonzero errno error code otherwise.
*/
int barcode_close(barcode_dev *const dev);

/* Open a barcode device.
 * Returns 0 if success, nonzero errno error code otherwise.
*/
int barcode_open(barcode_dev *const dev, const char *const device_path);

/* Read a barcode, but do not spend more than maximum_ms.
 * Returns the length of the barcode read.
 * (although at most length-1 characters are saved at the buffer,
 *  the total length of the barcode is returned.)
 * errno is always set; 0 if success, error code otherwise.
 * If the reading timed out, errno will be set to ETIMEDOUT.
*/
size_t barcode_read(barcode_dev *const dev,
                    char *const buffer, const size_t length,
                    const unsigned long maximum_ms);

#endif /* BARCODE_H */
