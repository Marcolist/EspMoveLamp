#!/usr/bin/env python3
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle,FancyBboxPatch,Circle
ROOT=Path(__file__).resolve().parents[1]
fig,(a,b)=plt.subplots(1,2,figsize=(12,8),gridspec_kw={'width_ratios':[1.8,1]},facecolor='#f5f3ee')
a.set_aspect('equal');a.set_xlim(-10,207);a.set_ylim(-17,208);a.axis('off')
a.add_patch(FancyBboxPatch((0,0),160,190,boxstyle='round,pad=0,rounding_size=3',fc='#dce3e8',ec='#486075',lw=2))
def block(x,y,w,h,color,label):
 a.add_patch(Rectangle((160-x-w,y),w,h,fc=color,ec='#344a5d',lw=1.2));a.text(160-x-w/2,y+h/2,label,ha='center',va='center',fontsize=10)
block(20,75,110,80,'#c0c5ca','5 V / 10 A\n110 x 80 x 36 mm')
block(15,14,27.7,40,'#dfaa73','DS3225')
block(80,20,64,34,'#8ac0ae','QIQIAZI 38 Pin\nPCB placeholder')
block(70,64,83,3,'#7c8e9b','')
a.add_patch(Rectangle((160-48,71),43,88,fill=False,ec='#ad6234',ls='--',lw=2))
a.annotate('Separate\nterminal cover',(145,110),(178,125),ha='center',fontsize=10,arrowprops={'arrowstyle':'->'})
a.plot([160,195],[44,44],color='#354c61',lw=3);a.annotate('to light\nbar',(193,44),(187,62),ha='center',fontsize=10)
a.text(80,198,'LEFT: 160 x 190 x 100 mm',ha='center',fontsize=13,fontweight='bold')
a.text(80,-10,'Front / desk',ha='center',fontsize=10)
a.annotate('PSU ventilation / rear',(75,158),(75,179),ha='center',fontsize=10,arrowprops={'arrowstyle':'->'})
b.axis('off');b.set_xlim(0,100);b.set_ylim(0,190);b.set_aspect('equal')
b.add_patch(FancyBboxPatch((15,35),70,100,boxstyle='round,pad=0,rounding_size=2',fc='#dce3e8',ec='#486075',lw=2))
b.add_patch(Rectangle((22,67),8,24,fc='#92a9bb',ec='#486075'))
b.text(50,152,'RIGHT\n70 x 100 x 90 mm',ha='center',fontsize=12,fontweight='bold')
b.text(50,115,'Passive\naxle mount',ha='center',fontsize=11)
b.plot([0,22],[79,79],color='#354c61',lw=3)
b.text(50,19,'CYD stays separate\nin its existing enclosure',ha='center',fontsize=11)
fig.suptitle('EspMoveLamp | Mechanical design v0.9',fontsize=20)
fig.text(.5,.035,'Top view of open bases. PCB dimensions are provisional; test-fit actual parts and cables before printing.',ha='center',fontsize=10)
fig.tight_layout(rect=[0,.07,1,.93]);fig.savefig(ROOT/'docs/layout.png',dpi=150)
