# ClassMLX

This is largely based on the Adafruit_MLX90640 library, which in turn was based
on Melexis N.V. source.

The aim of the present code is to use as far as possible the teensy's superior I2C
handling to speed up and smooth out the process of pulling IR camera data from the
MLX90640, with the side benefit of no longer having to install dozens of unnecessary
Adafruit libraries.

## Dependencies
ClassMLX was written for use with Richard Gemmell's teensy4_i2c library.
