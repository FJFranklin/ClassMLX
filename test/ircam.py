import sys
import time
import serial
import argparse

from datetime import datetime
from queue import Queue

import numpy as np

parser = argparse.ArgumentParser(description="Plotter/Logger for teensy-4 / MLX90640")

parser.add_argument('--device',  help='Specify serial device.',   default='/dev/ttyACM0', type=str)
parser.add_argument('--prefix',  help='Folder or prefix for log files.',      default='', type=str)
parser.add_argument('--logger',  help='Log received data.',                   action='store_true')
parser.add_argument('--show',    help='Show camera view and plot histogram.', action='store_true')
parser.add_argument('--shift',   help='Shift temperature before plotting.',   default=0.0, type=float)
parser.add_argument('--service', help='Retry device until available.',        action='store_true')

args = parser.parse_args()

bRunCam = args.logger or args.show
if not bRunCam:
    print("ircam: please specify either --logger or --show")
    sys.exit(2)

while True:
    try:
        teensy = serial.Serial(args.device)
    except:
        if not args.service:
            print("ircam: unable to open serial device: " + args.device)
            sys.exit(1)
        time.sleep(1) # try again in a bit
        continue
    break

if args.logger:
    filename = args.prefix + datetime.now().strftime("IRCam-%Y%m%d-%H%M%S.csv")
    try:
        csv = open(filename, "w")
    except:
        print("ircam: unable to open file for writing: " + filename)
        sys.exit(1)
else:
    csv = None

color_ranges = [
    [ 150, (255,255,  0), 216, (255,245,158) ],
    [ 100, (255, 77,  0), 150, (255,255,  0) ],
    [  40, (247,221,219), 100, (255,  0,  0) ],
    [  30, (255,179,220),  40, (218,112,214) ],
    [   5, ( 46,139, 87),  30, (228,250,228) ],
    [   0, (  0,  0,255),   5, ( 46,139, 87) ],
    [ -40, (218,240,255),   0, (102,190,249) ]]

def map_temperatures_to_colors(temperature_list):
    rgblist = []
    for T in temperature_list:
        rgb = (0, 0, 0) # default to black
        for cr in color_ranges:
            T_min, rgb_min, T_max, rgb_max = cr
            if T >= T_min and T <= T_max:
                r_min, g_min, b_min = rgb_min
                r_max, g_max, b_max = rgb_max
                r = r_min + (r_max - r_min) * (float(T) - T_min) / (T_max - T_min)
                g = g_min + (g_max - g_min) * (float(T) - T_min) / (T_max - T_min)
                b = b_min + (b_max - b_min) * (float(T) - T_min) / (T_max - T_min)
                rgb = (r/255, g/255, b/255)
                break
        rgblist.append(rgb)
    return rgblist

def on_close(event):
    global bRunCam
    bRunCam = False

def update_figure():
    if args.show:
        Tlist = T_mat.flatten()
        T_min = np.min(Tlist)
        T_max = np.max(Tlist)
        title = "Temp. [°C] Min={n:.2f} Max={x:.2f}".format(n=T_min, x=T_max)
        axr.set_title(title)
        color = map_temperatures_to_colors(Tlist)
        coll.cmap = ListedColormap(color)
        n, _ = np.histogram(Tlist, bins)
        for count, rect in zip(n, H.patches):
            rect.set_height(count)
        plt.pause(0.1)

def csv_write_row(row, temperatures):
    if csv is not None:
        csv.write("{r}".format(r=row))
        for t in temperatures:
            csv.write(",{t:.2f}".format(t=t))
        csv.write("\n")

def b64_decypher(c):
    is_b64 = True
    value = 0
    if c >= '0' and c <= '9':
        value = ord(c) - ord('0')
    elif c >= 'a' and c <= 'z':
        value = 10 + ord(c) - ord('a')
    elif c >= 'A' and c <= 'Z':
        value = 36 + ord(c) - ord('A')
    elif c == '=':
        value = 62
    elif c == '%':
        value = 63
    else:
        is_b64 = False
    return is_b64, value

def set_pixel_temperature(row, col, T): # range is -40 .. 216 degC
    global T_mat
    T_mat[row,col] = T + args.shift # shift is a hack for test purposes

Nrow = 24
Ncol = 32
T_lo = -40
T_hi = 216
Tlist = np.linspace(T_lo, T_hi, Nrow * Ncol)
T_mat = Tlist.reshape((Nrow,Ncol))

if args.show:
    import matplotlib.pyplot as plt
    import matplotlib.patches as patches
    from matplotlib.colors import ListedColormap
    from matplotlib.collections import PatchCollection

    patch = []
    for row in range(Nrow):
        for col in range(Ncol):
            patch.append(patches.Rectangle((col,row), 1, 1))

    Nbin = 32
    bins = np.linspace(T_lo, T_hi, Nbin + 1)
    binc = map_temperatures_to_colors((bins[:-1] + bins[1:])/2)

    color = map_temperatures_to_colors(Tlist)

    cmap = ListedColormap(color)
    coll = PatchCollection(patch, cmap=cmap)
    coll.set_array(np.arange(len(patch)))

    plt.rcParams['figure.figsize'] = [12, 4]
    fig, (axl, axr) = plt.subplots(1, 2)
    axl.add_collection(coll)
    _, _, H = axr.hist(Tlist, bins, lw=1, ec="grey")

    for rgb, rect in zip(binc, H.patches):
        rect.set(fc=rgb)

    axl.set_xlim([0,Ncol])
    axl.set_ylim([0,Nrow])
    axr.set_xlim([-40,216])
    axr.set_ylim(top=100)

    fig.canvas.mpl_connect('close_event', on_close)
    plt.show(block=False)
    plt.pause(1) # Give the plot time to sort itself out

# We're ready to go. Set the camera's refresh rate to 2Hz and switch to auto reporting
teensy.write(b';rate 2;auto on;')

Q = Queue(maxsize=0)
temperatures = None
row = -1
col = -2
bb1 = True
T12 =  0
while bRunCam:
    if Q.empty():
        try:
            bytes = teensy.readline()
        except:
            break # disconnected?
        for c in bytes.decode():
            Q.put(c)
        continue

    c = Q.get()
    if True:
        if col == -2: # we're waiting to start a new row
            if c == '{': # we're starting a new row
                row = -1 # we don't know the row yet...
                col = -1 # ... but we're ready to find out
            else:
                pass     # something's gone wrong, but ignore
            continue
        if col == -1: # the first character received should indicate which row
            is_b64, value = b64_decypher(c)
            if not is_b64:
                #print('Unexpected character; not a row number')
                col = -2 # reset
            elif value < 0 or value > 23:
                #print('Row number out of range')
                col = -2 # reset
            else:
                row = value # we're ready to read the row now
                col = 0
                bb1 = True  # reading characters in pairs; the next is the first
                temperatures = []
            continue
        if col == 32: # all going well, this should be the end of the row
            if c == '}':
                #print("Success, row={r}".format(r=row))
                col = -2 # success! -> reset
                csv_write_row(row, temperatures)
                temperatures = None
                if row == 23:
                    update_figure()
            else:
                #print('Unexpected character at end of row')
                col = -2 # something's gone wrong; reset
            continue
        # at this point, we have row=0..23 and col=0..31
        is_b64, value = b64_decypher(c)
        if not is_b64:
            #print('Unexpected character')
            col = -2 # something's gone wrong; reset
            continue
        if bb1: # it's the first of a pair; record and go get the next character
            T12 = value * 64
            bb1 = False
            continue
        T12 = (T12 + value) / 16.0 - 40
        temperatures.append(T12)
        set_pixel_temperature(row, col, T12) # range is -40 .. 216 degC
        bb1 = True
        col += 1

if csv is not None:
    csv.close()

try:
    teensy.write(b'auto off;')
    teensy.close()
except:
    if not args.service:
        print("ircam: device disconnected?")
