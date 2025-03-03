import sys
import argparse
import csv

import numpy as np
import cv2

parser = argparse.ArgumentParser(description="Replay/Convert IRCam log data")

parser.add_argument('--file', help='Specify input logger file to replay.',  default='', type=str)
parser.add_argument('--save', help='Specify video file to save replay.',    default='', type=str)

args = parser.parse_args()

if len(args.file) == 0:
    print("replay: please specify input file with --file")
    sys.exit(2)
try:
    csvfile = open(args.file, newline="")
except:
    print("replay: unable to open file for reading: " + args.file)
    sys.exit(1)

video = None

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

def update_figure(t):
    if True:
        Tlist = T_mat.flatten()
        T_min = np.min(Tlist)
        T_max = np.max(Tlist)
        title = "Temp. [°C] Min={n:.2f} Max={x:.2f}".format(n=T_min, x=T_max)
        axr.set_title(title)
        axl.set_title("Time {t:.1f}s".format(t=t))
        color = map_temperatures_to_colors(Tlist)
        coll.cmap = ListedColormap(color)
        n, _ = np.histogram(Tlist, bins)
        for count, rect in zip(n, H.patches):
            rect.set_height(count)
        fig.canvas.draw()
        img_plot = np.array(fig.canvas.renderer.buffer_rgba())
        img_frame = cv2.cvtColor(img_plot, cv2.COLOR_RGBA2BGR)
        cv2.imshow(args.file, img_frame)
        cv2.waitKey(100)

        if len(args.save):
            global video
            if video is None:
                fps = 2
                height, width, _ = img_frame.shape
                codec = cv2.VideoWriter_fourcc(*'mp4v')
                video = cv2.VideoWriter(args.save, codec, fps, (width, height))
            video.write(img_frame)

def csv_write_row(row, temperatures):
    if csv is not None:
        csv.write("{r}".format(r=row))
        for t in temperatures:
            csv.write(",{t:.2f}".format(t=t))
        csv.write("\n")

def set_pixel_temperature(row, col, T): # range is -40 .. 216 degC
    global T_mat
    T_mat[row,col] = T

Nrow = 24
Ncol = 32
T_lo = -40
T_hi = 216
Tlist = np.linspace(T_lo, T_hi, Nrow * Ncol)
T_mat = Tlist.reshape((Nrow,Ncol))

if True:
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

    fig.canvas.draw()

bVisible = False
reader = csv.reader(csvfile)
row_no = 0
t = 0
for csvrow in reader:
    if bVisible:
        if cv2.getWindowProperty(args.file, cv2.WND_PROP_VISIBLE) <= 0:
            print("replay: interrupted!")
            break
    row_no += 1
    if len(csvrow) != 33:
        print("replay: row {r} incomplete or incorrectly formatted.".format(r=row_no))
        break
    row = int(csvrow[0])
    if row < 0 or row > 23:
        print("replay: row {r} row number out of range.".format(r=row_no))
        break
    for col in range(32):
        T = float(csvrow[col+1])
        if T < -40:
            T = -40
        if T > 216:
            T = 216
        set_pixel_temperature(row, col, T)
    if row == 23:
        t += 0.5
        update_figure(t)
        bVisible = True

csvfile.close()

if video is not None:
    video.release()

while cv2.getWindowProperty(args.file, cv2.WND_PROP_VISIBLE) > 0:
    if cv2.waitKey(100) >= 0:
        break
