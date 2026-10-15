#!/usr/bin/env python3
"""Export current mechanical prototype and validate STL topology. Needs OpenSCAD,numpy."""
from pathlib import Path
import argparse,subprocess,struct,re,json,collections
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
PARTS=[('left_base','left_electronics_base'),('left_hood','left_electronics_cover'),('servo_mount','left_ds3225_servo_mount'),('right_base','right_axle_base'),('right_hood','right_cover'),('horn_cap','servo_rear_coupling_disc'),('axle_cap','axle_rear_coupling_disc'),('profile_cap','profile_front_coupling_disc_2x'),('psu_strap','psu_retaining_bracket_2x'),('esp_carrier','esp32_pcb_carrier'),('esp_finger','esp32_pcb_retainer_2x'),('terminal_shroud','psu_terminal_cover')]
def read_stl(path):
 data=path.read_bytes()
 if len(data)>=84 and len(data)==84+50*struct.unpack_from('<I',data,80)[0]:
  count=struct.unpack_from('<I',data,80)[0];return np.array([struct.unpack_from('<12f',data,84+50*i)[3:] for i in range(count)]).reshape(-1,3,3)
 return np.array(re.findall(rb'vertex\s+([-\d.e+]+)\s+([-\d.e+]+)\s+([-\d.e+]+)',data),float).reshape(-1,3,3)
def validate(t):
 # Quantize STL coordinates consistently for numeric topology checks.
 q=np.round(t,5)
 edges=collections.Counter(tuple(sorted([tuple(tri[a]),tuple(tri[b])])) for tri in q for a,b in [(0,1),(1,2),(2,0)])
 assert all(n==2 for n in edges.values()),'Non-manifold edge'
 adj=collections.defaultdict(set)
 for a,b in edges:adj[a].add(b);adj[b].add(a)
 unseen=set(adj);components=0
 while unseen:
  components+=1;stack=[unseen.pop()]
  while stack:
   for b in adj[stack.pop()]:
    if b in unseen:unseen.remove(b);stack.append(b)
 assert components==1,('Disconnected component',components)
 return {'closed_manifold_edges':True,'surface_components':components,'triangles':len(t),'size_mm':(t.max((0,1))-t.min((0,1))).round(3).tolist()}
def binary(path,t):
 with path.open('wb') as f:
  f.write(b'EspMoveLamp v0.9 millimeters; prototype'.ljust(80,b' '));f.write(struct.pack('<I',len(t)))
  for tri in t:
   n=np.cross(tri[1]-tri[0],tri[2]-tri[0]);n/=max(np.linalg.norm(n),1e-12)
   f.write(struct.pack('<12fH',*n,*tri.flatten(),0))
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--check-only',action='store_true');a=ap.parse_args()
 out=ROOT/'stl';out.mkdir(exist_ok=True);results={}
 for part,name in PARTS:
  path=out/(name+'.stl')
  if not a.check_only:
   r=subprocess.run(['openscad','-o',str(path),'-D',f'part="{part}"',str(ROOT/'cad/EspMoveLamp.scad')],capture_output=True,text=True)
   if r.returncode:raise RuntimeError(r.stderr)
  t=read_stl(path);results[name]=validate(t)
  if not a.check_only:binary(path,t);validate(read_stl(path))
  print(name,'OK',flush=True)
 (ROOT/'docs/mesh-validation.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
