#!/usr/bin/env python3
"""Create track_with_qr.sdf from your CURRENT track_with_robot.sdf.
Preserves physics/cameras/robot. Requires python3-qrcode (Ubuntu package).
Marker is 50 mm INCLUDING its four-module white border, 85 mm left of
the bottom straight's centreline. Assumes a 6 m floor with line at y=-2.
"""
import argparse
from pathlib import Path
import xml.etree.ElementTree as E
import qrcode

PROJECT_ROOT = Path(__file__).resolve().parents[1]

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('world', type=Path)
p.add_argument('--x', type=float, default=.75)
p.add_argument('--y', type=float, default=-1.915)
p.add_argument('--force', action='store_true')
a=p.parse_args()
src=a.world.expanduser().resolve()
dst=src.parent/'track_with_qr.sdf'
png=PROJECT_ROOT/'assets'/'checkpoints'/'Station_A.png'
if src==dst: p.error('Use track_with_robot.sdf as input.')
if not a.force and (dst.exists() or png.exists()): p.error('Outputs exist; use --force to replace them.')
root=E.parse(src).getroot()
w=root.find('world')
if w is None: p.error('Input has no world.')
if w.find("model[@name='qr_station_a']") is not None: p.error('Checkpoint already exists.')
qr=qrcode.QRCode(version=1, error_correction=qrcode.constants.ERROR_CORRECT_M, box_size=16, border=4)
qr.add_data('Station_A'); qr.make(fit=False)
qr.make_image(fill_color='black',back_color='white').convert('RGB').save(png)
m=E.SubElement(w,'model',name='qr_station_a')
E.SubElement(m,'static').text='true'
E.SubElement(m,'pose').text=f'{a.x} {a.y} 0.001 0 0 0'
l=E.SubElement(m,'link',name='marker')
v=E.SubElement(l,'visual',name='qr_visual')
g=E.SubElement(E.SubElement(v,'geometry'),'plane')
E.SubElement(g,'normal').text='0 0 1'
E.SubElement(g,'size').text='0.05 0.05'
mat=E.SubElement(v,'material')
for key in ('ambient','diffuse'): E.SubElement(mat,key).text='1 1 1 1'
E.SubElement(mat,'specular').text='0 0 0 1'
metal=E.SubElement(E.SubElement(mat,'pbr'),'metal')
E.SubElement(metal,'albedo_map').text='../assets/checkpoints/Station_A.png'
E.SubElement(metal,'metalness').text='0'
E.SubElement(metal,'roughness').text='1'
# Visual-only at 1 mm above floor: no bump for wheels, no overlapping surfaces.
E.indent(root,space='  ')
E.ElementTree(root).write(dst,encoding='utf-8',xml_declaration=True)
print(f'Created {dst}\nCreated {png}\nOriginal world and rates preserved.')
