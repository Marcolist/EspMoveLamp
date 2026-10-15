"""Render exported housing assemblies together at the same scale."""
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from matplotlib import font_manager
from build import ROOT,read_stl
p=ROOT/'stl'
font_path=font_manager.findfont('DejaVu Sans')
font=ImageFont.truetype(font_path,17)
big=ImageFont.truetype(font_path,24)

meshes=[]
for name,dx in [('left_electronics_base',0),('left_electronics_cover',0),('right_axle_base',300),('right_cover',300)]:
 mesh=read_stl(p/(name+'.stl')).copy();mesh[:,:,0]+=dx;meshes.append(mesh)
t=np.concatenate(meshes)
cam=np.array([0.6,-1.,0.7]);cam/=np.linalg.norm(cam)
right=np.cross([0,0,1],cam);right/=np.linalg.norm(right);up=np.cross(cam,right)
basis=np.stack([right,up,cam],axis=1);v=t@basis
mn=v.min((0,1));mx=v.max((0,1));scale=min(1550/(mx[0]-mn[0]),560/(mx[1]-mn[1]));v[:,:,0]=(v[:,:,0]-(mn[0]+mx[0])/2)*scale+900;v[:,:,1]=350-(v[:,:,1]-(mn[1]+mx[1])/2)*scale
pix=np.full((700,1800,3),247,dtype=np.uint8);dep=np.full((700,1800),-np.inf)
for tri,orig in zip(v,t):
 x0=max(0,int(np.floor(tri[:,0].min())));x1=min(1799,int(np.ceil(tri[:,0].max())));y0=max(0,int(np.floor(tri[:,1].min())));y1=min(699,int(np.ceil(tri[:,1].max())))
 if x1<x0 or y1<y0:continue
 a,b,c=tri;den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
 if abs(den)<1e-8:continue
 yy,xx=np.mgrid[y0:y1+1,x0:x1+1];xx=xx+.5;yy=yy+.5
 w0=((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/den
 w1=((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/den;w2=1-w0-w1
 z=w0*a[2]+w1*b[2]+w2*c[2];mask=(w0>=-1e-5)&(w1>=-1e-5)&(w2>=-1e-5)&(z>dep[y0:y1+1,x0:x1+1])
 n=np.cross(orig[1]-orig[0],orig[2]-orig[0]);n/=max(np.linalg.norm(n),1e-10)
 shade=.35+.65*abs(n@cam);color=np.array([117,151,178])*shade
 dep[y0:y1+1,x0:x1+1][mask]=z[mask];pix[y0:y1+1,x0:x1+1][mask]=color.astype(np.uint8)
canvas=Image.new('RGB',(1800,930),'#f5f3ee');d=ImageDraw.Draw(canvas)
canvas.paste(Image.fromarray(pix),(0,90))
d.text((70,35),'EspMoveLamp | Matching compact housings',font=big,fill='#203549')
d.text((230,820),'LEFT 220 x 130 x 90 mm      |      RIGHT 70 x 130 x 90 mm',font=big,fill='#203549')
d.text((170,875),'Same depth, height and side contour. Internal PSU. Spacing shortened; light bar omitted.',font=font,fill='#486075')
canvas.save(ROOT/'docs/housings-preview.png')
