from pathlib import Path
import re,json,numpy as np
import sys
from build import ROOT,read_stl
p=ROOT/'stl'
cache={}
def inside(name,pt):
 if name not in cache:cache[name]=read_stl(p/(name+'.stl'))
 x,y,z=pt;zs=[]
 for a,b,c in cache[name]:
  mat=np.column_stack([b[:2]-a[:2],c[:2]-a[:2]])
  if abs(np.linalg.det(mat))<1e-9:continue
  u,v=np.linalg.solve(mat,np.array([x,y])-a[:2])
  if u>=-1e-7 and v>=-1e-7 and u+v<=1+1e-7:
   hit=a[2]+u*(b[2]-a[2])+v*(c[2]-a[2])
   if hit>z+1e-6:zs.append(round(float(hit),4))
 return len(set(zs))%2==1
F='profile_front_coupling_disc_2x';R='servo_rear_coupling_disc'
cases=[(F,'Front upper LED entry',(10,0.2,2)),(F,'Front lower LED entry',(10,20.2,2)),(F,'Top LED entry',(0.2,30,2)),(F,'Bottom LED entry',(0.2,-10,2)),(F,'Top cable channel',(7,27,1.5)),(F,'Bottom cable channel',(7,-10,1.5)),(F,'Shared cable channel',(14,18,1.5)),(F,'Concealed cable channel',(14,8,1.5)),(R,'Rear disc cable outlet',(14,8,9)),(R,'Open servo-side horn pocket',(-20,0.2,3)),(R,'Hub access',(0.2,0.2,10)),(R,'Fixed M3 hole',(-24,0.2,10))]
results=[]
for name,label,point in cases:
 ok=not inside(name,point);assert ok,(label,point)
 results.append({'part':name,'check':label,'point_mm':point,'free_space':ok})
assert inside(F,(14,8,7)), 'square outward opening must be closed'
assert inside(F,(-24,0,7)), 'round outward opening must be closed'
assert not inside(F,(-24,0,1.5)), 'blind screw-head pocket must stay clear'
# Profile end-stop solid outside channels; prevents metal from blocking routes.
assert inside(F,(-5,20,3.5))
results.append({'check':'Extrusion end-stop','point_mm':[-5,20,3.5],'solid':True})
(ROOT/'docs/passage-validation.json').write_text(json.dumps(results,indent=2,ensure_ascii=False));print('13 targeted passage/end-stop checks passed')
