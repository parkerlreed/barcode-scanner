# barcode-scanner
Program to read barcodes from a raw keyboard device and print to terminal (useful for barcode scanners)

Used https://stackoverflow.com/questions/29942421/read-barcodes-from-input-event-linux-c/29956584#29956584 and http://hzqtc.github.io/2012/05/play-mp3-with-libmpg123-and-libao.html to cobble together this program.

Requires libao and mpg123 (headers)

````
Usage: ./example [ -h | --help ]
       ./example INPUT-EVENT-DEVICE IDLE-TIMEOUT PATH-TO-MP3

This program reads barcodes from INPUT-EVENT-DEVICE,
waiting at most IDLE-TIMEOUT seconds for a new barcode.
The INPUT-EVENT-DEVICE is grabbed, the digits do not appear as
inputs in the machine.
You can at any time end the program by sending it a
SIGINT (Ctrl+C), SIGHUP, or SIGTERM signal.
````
