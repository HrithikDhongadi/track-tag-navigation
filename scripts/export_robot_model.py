#!/usr/bin/env python3
"""Export the tuned inline AMR from a world as a reusable SDF model package."""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('world', type=Path, help='tuned world containing model name="amr"')
    p.add_argument('--output', type=Path, default=Path('models/amr'))
    p.add_argument('--force', action='store_true')
    a = p.parse_args()
    model = ET.parse(a.world).getroot().find("world/model[@name='amr']")
    if model is None:
        p.error('No model named amr in the input world.')
    output = a.output.resolve()
    sdf, config = output / 'model.sdf', output / 'model.config'
    if not a.force and (sdf.exists() or config.exists()):
        p.error(f'{output} already exists; use --force to replace it.')
    output.mkdir(parents=True, exist_ok=True)
    model.find('pose').text = '0 0 0 0 0 0'  # World placement belongs in map JSON.
    root = ET.Element('sdf', version='1.8'); root.append(model)
    ET.indent(root, space='  ')
    ET.ElementTree(root).write(sdf, encoding='utf-8', xml_declaration=True)
    config.write_text("""<?xml version='1.0'?>
<model>
  <name>amr</name>
  <version>1.0</version>
  <sdf version='1.8'>model.sdf</sdf>
</model>
""", encoding='utf-8')
    print(f'Created reusable robot package: {output}')


if __name__ == '__main__':
    main()
