import time
import numpy as np

def create_reference():
    global rv # Wave radial coordinates
    global av # Wave amplitudes
    global ov # Wave amplitude offsets
    x = np.linspace(-15.5,15.5,32)
    y = np.linspace(-11.5,11.5,24)
    xv, yv = np.meshgrid(x, y)
    rv = np.sqrt(xv**2 + yv**2)
    rmin = np.min(rv)
    rmax = np.max(rv)
    av = 127 * (rmax - rv) / (rmax - rmin)
    ov =  66 * (rmax - rv) / (rmax - rmin) + 22

def create_wave(t, f = 1):
    global rv
    global av
    global ov
    return ov + av * np.sin(rv - 2 * np.pi * f * t)

def print_wave(wave):
    for row in range(24):
        print("{r}".format(r=row), end="")
        for col in range(32):
            print(",{w:.2f}".format(w=wave[row,col]), end="")
        print("")

create_reference()

t = 0
while t < 30:
    wave = create_wave(t, 0.1)
    print_wave(wave)
    t += 0.5
