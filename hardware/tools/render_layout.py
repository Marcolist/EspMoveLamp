#!/usr/bin/env python3
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle,FancyBboxPatch,Circle
ROOT=Path(__file__).resolve().parents[1]
fig,(a,b)=plt.subplots(1,2,figsize=(12,8),gridspec_kw={'width_ratios':[2.77,1]},facecolor='#f5f3ee')
a.set_aspect('equal');a.set_xlim(-10,267);a.set_ylim(-20,156);a.axis('off')
a.add_patch(FancyBboxPatch((0,0),220,130,boxstyle='round,pad=0,rounding_size=3',fc='#dce3e8',ec='#486075',lw=2))
def block(x,y,w,h,color,label):
 a.add_patch(Rectangle((220-x-w,y),w,h,fc=color,ec='#344a5d',lw=1.2));a.text(220-x-w/2,y+h/2,label,ha='center',va='center',fontsize=10)
block(100,25,110,80,'#c0c5ca','5 V / 10 A\n110 x 80 x 36 mm')
block(15,14,27.7,40,'#dfaa73','DS3225')
block(9,84,64,34,'#8ac0ae','QIQIAZI 38 Pin\nPCB placeholder')
block(82,14,3,100,'#7c8e9b','')
a.add_patch(Rectangle((220-128,21),43,88,fill=False,ec='#ad6234',ls='--',lw=2))
a.annotate('Separate\nterminal cover',(120,62),(245,85),ha='center',fontsize=10,arrowprops={'arrowstyle':'->'})
a.plot([220,255],[44,44],color='#354c61',lw=3);a.annotate('to light\nbar',(253,44),(246,62),ha='center',fontsize=10)
a.text(110,143,'LEFT: 220 x 130 x 90 mm',ha='center',fontsize=13,fontweight='bold')
a.text(110,-10,'Front / desk',ha='center',fontsize=10)
a.annotate('PSU ventilation / rear',(65,100),(65,120),ha='center',fontsize=10,arrowprops={'arrowstyle':'->'})
b.axis('off');b.set_xlim(0,100);b.set_ylim(-20,156);b.set_aspect('equal')
b.add_patch(FancyBboxPatch((15,0),70,130,boxstyle='round,pad=0,rounding_size=2',fc='#dce3e8',ec='#486075',lw=2))
b.add_patch(Rectangle((22,32),8,24,fc='#92a9bb',ec='#486075'))
b.text(50,143,'RIGHT\n70 x 130 x 90 mm',ha='center',fontsize=12,fontweight='bold')
b.text(50,89,'Passive\naxle mount',ha='center',fontsize=11)
b.plot([0,22],[44,44],color='#354c61',lw=3)
b.text(50,-15,'CYD stays separate\nin its existing enclosure',ha='center',fontsize=9)
fig.suptitle('EspMoveLamp | Mechanical design v0.9',fontsize=20)
fig.text(.5,.035,'Top view of open bases. PCB dimensions are provisional; test-fit actual parts and cables before printing.',ha='center',fontsize=10)
fig.tight_layout(rect=[0,.07,1,.93]);fig.savefig(ROOT/'docs/layout.png',dpi=150)
