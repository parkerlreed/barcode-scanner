#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include "barcode.h"
#include <ao/ao.h>
#include <mpg123.h>

#define BITS 8

#define BARCODE_MAXLEN  1023

void player (int argc, char *argv[])
{
    mpg123_handle *mh;
    unsigned char *buffer;
    size_t buffer_size;
    size_t done;
    int err;

    int driver;
    ao_device *dev2;

    ao_sample_format format;
    int channels, encoding;
    long rate;

    /* initializations */
    ao_initialize();
    driver = ao_default_driver_id();
    mpg123_init();
    mh = mpg123_new(NULL, &err);
    buffer_size = mpg123_outblock(mh);
    buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));

    /* open the file and get the decoding format */
    mpg123_open(mh, argv[3]);
    mpg123_getformat(mh, &rate, &channels, &encoding);

    /* set the output format and open the output device */
    format.bits = mpg123_encsize(encoding) * BITS;
    format.rate = rate;
    format.channels = channels;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = 0;
    dev2 = ao_open_live(driver, &format, NULL);
    /* decode and play */
    while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK)
        ao_play(dev2, buffer, done);

    /* clean up */
    free(buffer);
    ao_close(dev2);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    ao_shutdown();

}

int main(int argc, char *argv[])
{
    barcode_dev    dev;
    unsigned long  ms;
    int            status, exitcode;

    if (argc != 4 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        fprintf(stderr, "\n");
        fprintf(stderr, "Usage: %s [ -h | --help ]\n", argv[0]);
        fprintf(stderr, "       %s INPUT-EVENT-DEVICE IDLE-TIMEOUT PATH-TO-MP3\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "This program reads barcodes from INPUT-EVENT-DEVICE,\n");
        fprintf(stderr, "waiting at most IDLE-TIMEOUT seconds for a new barcode.\n");
        fprintf(stderr, "The INPUT-EVENT-DEVICE is grabbed, the digits do not appear as\n");
        fprintf(stderr, "inputs in the machine.\n");
        fprintf(stderr, "You can at any time end the program by sending it a\n");
        fprintf(stderr, "SIGINT (Ctrl+C), SIGHUP, or SIGTERM signal.\n");
        fprintf(stderr, "\n");
        return EXIT_FAILURE;
    }

    if (install_done(SIGHUP) ||
        install_done(SIGTERM)) {
        fprintf(stderr, "Cannot install signal handlers: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    {
        double value, check;
        char dummy;
        if (sscanf(argv[2], " %lf %c", &value, &dummy) != 1 || value < 0.001) {
            fprintf(stderr, "%s: Invalid idle timeout value (in seconds).\n", argv[2]);
            return EXIT_FAILURE;
        }
        ms = (unsigned long)(value * 1000.0);
        check = (double)ms / 1000.0;

        if (value < check - 0.001 || value > check + 0.001 || ms < 1UL) {
            fprintf(stderr, "%s: Idle timeout is too long.\n", argv[2]);
            return EXIT_FAILURE;
        }
    }

    if (barcode_open(&dev, argv[1])) {
        fprintf(stderr, "%s: Cannot open barcode input event device: %s.\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }

    while (1) {
        char    code[BARCODE_MAXLEN + 1];
        size_t  len;

        if (done) {
            status = EINTR;
            break;
        }

        len = barcode_read(&dev, code, sizeof code, ms);
        if (errno) {
            status = errno;
            break;
        }
        if (len < (size_t)1) {
            status = ETIMEDOUT;
            break;
        }

        printf("%s\n", code);
        player(argc, argv);

        fflush(stdout);
    }

    if (status == EINTR) {
        fprintf(stderr, "Signaled to exit. Complying.\n");
        fflush(stderr);
        exitcode = EXIT_SUCCESS;
    } else
    if (status == ETIMEDOUT) {
        fprintf(stderr, "Timed out, no more barcodes.\n");
        fflush(stderr);
        exitcode = EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Error reading input event device %s: %s.\n", argv[1], strerror(status));
        fflush(stderr);
        exitcode = EXIT_FAILURE;
    }

    if (barcode_close(&dev)) {
        fprintf(stderr, "Warning: Error closing input event device %s: %s.\n", argv[1], strerror(errno));
        fflush(stderr);
        exitcode = EXIT_FAILURE;
    }

    return exitcode;
}
