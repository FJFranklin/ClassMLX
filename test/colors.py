import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.colors import ListedColormap
from matplotlib.collections import PatchCollection

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

Nrow = 24
Ncol = 32

patch = []
for row in range(Nrow):
    for col in range(Ncol):
        patch.append(patches.Rectangle((col,row), 1, 1))

T_lo = -40
T_hi = 216
Tlist = np.linspace(T_lo, T_hi, Nrow * Ncol)
T_mat = Tlist.reshape((Nrow,Ncol))

Nbin = 16
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
axr.set_ylim(top=100)

bCycle = True
def on_close(event):
    global bCycle
    bCycle = False
    print('Closed Figure!')

fig.canvas.mpl_connect('close_event', on_close)
plt.show(block=False)

while bCycle:
    plt.pause(0.5)
    T_mat = np.random.uniform(T_lo, T_hi, (Nrow,Ncol))
    Tlist = T_mat.flatten()
    color = map_temperatures_to_colors(Tlist)
    coll.cmap = ListedColormap(color)
    n, _ = np.histogram(Tlist, bins)
    for count, rect in zip(n, H.patches):
        rect.set_height(count)
