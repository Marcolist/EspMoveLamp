#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,json
ROOT=Path(__file__).resolve().parents[1]
checks=['collision_hood_parts','collision_hood_psu','collision_hood_esp','collision_shroud_psu']
results=[]
with tempfile.TemporaryDirectory() as d:
 for part in checks:
  r=subprocess.run(['openscad','-o',str(Path(d)/(part+'.stl')),'-D',f'part="{part}"',str(ROOT/'cad/EspMoveLamp.scad')],capture_output=True,text=True)
  empty='top level object is empty' in r.stderr
  results.append({'check':part,'empty':empty});print(part,'OK' if empty else 'COLLISION',flush=True)
(ROOT/'docs/collision-validation.json').write_text(json.dumps(results,indent=2)+'\n')
assert all(r['empty'] for r in results),results
