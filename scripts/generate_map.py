#!/usr/bin/env python3
"""Generate a checkpoint world and QR textures from an editable map JSON file.

Example:
  python3 scripts/generate_map.py maps/warehouse.json --output sdf/warehouse.sdf

The input template is not changed. Generated QR codes are visual-only planes,
so the floor PNG remains an independently editable Inkscape asset.
"""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

import qrcode
from PIL import Image, ImageOps

PROJECT_ROOT = Path(__file__).resolve().parents[1]
QR_SIZE_M = 0.05
TRACK_OVERLAY_CUTOFF = 180


def project_path(value, label):
    path = (PROJECT_ROOT / value).resolve()
    try:
        path.relative_to(PROJECT_ROOT)
    except ValueError:
        raise ValueError(f"{label} must be inside the project: {value}")
    return path


def sdf_path(output, asset):
    return os.path.relpath(asset, output.parent).replace(os.sep, '/')


def model_name(identifier):
    return 'qr_' + re.sub(r'[^a-z0-9_]+', '_', identifier.lower()).strip('_')


def add_marker(world, identifier, x, y, yaw, texture_path):
    model = ET.SubElement(world, 'model', name=model_name(identifier))
    ET.SubElement(model, 'static').text = 'true'
    ET.SubElement(model, 'pose').text = f'{x} {y} 0.001 0 0 {yaw}'
    link = ET.SubElement(model, 'link', name='marker')
    visual = ET.SubElement(link, 'visual', name='qr_visual')
    plane = ET.SubElement(ET.SubElement(visual, 'geometry'), 'plane')
    ET.SubElement(plane, 'normal').text = '0 0 1'
    ET.SubElement(plane, 'size').text = f'{QR_SIZE_M} {QR_SIZE_M}'
    material = ET.SubElement(visual, 'material')
    for name in ('ambient', 'diffuse'):
        ET.SubElement(material, name).text = '1 1 1 1'
    ET.SubElement(material, 'specular').text = '0 0 0 1'
    metal = ET.SubElement(ET.SubElement(material, 'pbr'), 'metal')
    ET.SubElement(metal, 'albedo_map').text = texture_path
    ET.SubElement(metal, 'metalness').text = '0'
    ET.SubElement(metal, 'roughness').text = '1'


def compose_floor_texture(track_path, surface_path, output_path):
    """Overlay the dark track pixels on a light surface texture.

    The editable track layout is expected to have a light background and dark
    route strokes. Only pixels darker than TRACK_OVERLAY_CUTOFF are retained,
    so the surface remains visible everywhere else.
    """
    try:
        with Image.open(track_path) as source:
            track = source.convert('RGB')
        with Image.open(surface_path) as source:
            resampling = getattr(Image, 'Resampling', Image).LANCZOS
            surface = ImageOps.fit(source.convert('RGB'), track.size,
                                   method=resampling)
    except OSError as error:
        raise ValueError(f'Cannot read track or surface texture: {error}') from error

    luminance = ImageOps.grayscale(track)
    mask = luminance.point(lambda value: 255 if value < TRACK_OVERLAY_CUTOFF else 0)
    composite = Image.composite(track, surface, mask)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    composite.save(output_path, 'PNG')


def validate_navigation(navigation, checkpoint_ids):
    """Validate the routing layer without coupling it to SDF generation."""
    if navigation is None:
        return
    if not isinstance(navigation, dict):
        raise ValueError("navigation must be an object.")
    if navigation.get("schema_version", 1) != 1:
        raise ValueError("navigation.schema_version must be 1.")
    start = navigation.get("default_start", "")
    if start and start not in checkpoint_ids:
        raise ValueError("navigation.default_start must name a checkpoint.")
    edges = navigation.get("edges", [])
    if not isinstance(edges, list):
        raise ValueError("navigation.edges must be a list.")
    ids = set()
    pairs = set()
    for edge in edges:
        if not isinstance(edge, dict):
            raise ValueError("Each navigation edge must be an object.")
        edge_id = str(edge.get("id", "")).strip()
        source = str(edge.get("from", "")).strip()
        destination = str(edge.get("to", "")).strip()
        if not edge_id or edge_id in ids:
            raise ValueError("Navigation edge IDs must be non-empty and unique.")
        if source not in checkpoint_ids or destination not in checkpoint_ids or source == destination:
            raise ValueError(f"Invalid navigation edge: {source} -> {destination}.")
        if (source, destination) in pairs:
            raise ValueError(f"Duplicate navigation edge: {source} -> {destination}.")
        try:
            cost = float(edge["cost_m"])
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError(f"Navigation edge {edge_id} needs a positive cost_m.") from error
        if cost <= 0:
            raise ValueError(f"Navigation edge {edge_id} needs a positive cost_m.")
        maneuver = edge.get("maneuver", {"type": "follow"})
        if not isinstance(maneuver, dict) or maneuver.get("type") not in ("follow", "junction_turn"):
            raise ValueError(f"Navigation edge {edge_id} has an invalid maneuver.")
        if maneuver["type"] == "junction_turn" and maneuver.get("direction") not in ("left", "right", "straight"):
            raise ValueError(f"Navigation edge {edge_id} has an invalid turn direction.")
        ids.add(edge_id)
        pairs.add((source, destination))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('map_json', type=Path)
    parser.add_argument('--output', type=Path, required=True,
                        help='new SDF path, relative to the project root')
    parser.add_argument('--force', action='store_true',
                        help='replace only this generated SDF and its QR textures')
    args = parser.parse_args()

    config_file = args.map_json.expanduser().resolve()
    output = args.output.expanduser()
    if not output.is_absolute():
        output = PROJECT_ROOT / output
    output = output.resolve()
    try:
        output.relative_to(PROJECT_ROOT)
    except ValueError:
        parser.error('--output must be inside the project')
    map_id = re.sub(r'[^a-z0-9_]+', '_', config_file.stem.lower()).strip('_') or 'map'
    try:
        config = json.loads(config_file.read_text(encoding='utf-8'))
        template = project_path(config.get('template', 'sdf/track_with_robot.sdf'), 'template')
        has_track = 'track_png' in config
        has_surface = 'surface_png' in config
        if has_track != has_surface:
            raise ValueError('track_png and surface_png must be supplied together.')
        if has_track:
            track = project_path(config['track_png'], 'track_png')
            surface = project_path(config['surface_png'], 'surface_png')
            floor = PROJECT_ROOT / 'assets' / 'generated_floors' / f'{map_id}.png'
        else:
            floor = project_path(config['floor_png'], 'floor_png')
            track = surface = None
        checkpoints = config['checkpoints']
        floor_size = config.get('floor_size_m', [6.0, 6.0])
        floor_width, floor_height = float(floor_size[0]), float(floor_size[1])
        robot = config.get('robot', {})
        robot_uri = str(robot.get('model_uri', 'model://amr'))
        robot_pose = [float(value) for value in robot.get(
            'start_pose', [0, -2, .005, 0, 0, 0])]
    except (KeyError, IndexError, TypeError, OSError, ValueError, json.JSONDecodeError) as error:
        parser.error(str(error))
    if floor_width <= 0 or floor_height <= 0:
        parser.error('floor_size_m values must be positive.')
    if not robot_uri.startswith('model://') or len(robot_pose) != 6:
        parser.error('robot requires a model:// URI and a six-value start_pose.')
    if not template.is_file() or (track is None and not floor.is_file()) or \
            (track is not None and (not track.is_file() or not surface.is_file())):
        parser.error('The template and selected floor assets must exist.')
    if not isinstance(checkpoints, list) or not checkpoints:
        parser.error('checkpoints must be a non-empty list.')

    qr_dir = PROJECT_ROOT / 'assets' / 'checkpoints' / map_id
    markers = []
    names = set()
    try:
        for checkpoint in checkpoints:
            identifier = str(checkpoint['id']).strip()
            x, y, yaw = (float(checkpoint['x']), float(checkpoint['y']),
                         float(checkpoint.get('yaw', 0)))
            if not identifier or identifier in names:
                raise ValueError('Checkpoint IDs must be non-empty and unique.')
            if not (-floor_width / 2 <= x <= floor_width / 2 and
                    -floor_height / 2 <= y <= floor_height / 2):
                raise ValueError(f'{identifier} is outside the floor bounds.')
            names.add(identifier)
            markers.append((identifier, x, y, yaw, qr_dir / f'{identifier}.png'))
    except (KeyError, TypeError, ValueError) as error:
        parser.error(str(error))
    try:
        validate_navigation(config.get("navigation"), names)
    except ValueError as error:
        parser.error(str(error))
    targets = [output, *(marker[4] for marker in markers)]
    if track is not None:
        targets.append(floor)
    if not args.force and any(path.exists() for path in targets):
        parser.error('Output exists; choose a new map name or use --force.')

    if track is not None:
        try:
            compose_floor_texture(track, surface, floor)
        except ValueError as error:
            parser.error(str(error))

    root = ET.parse(template).getroot()
    world = root.find('world')
    if world is None:
        parser.error('Template has no world.')
    if any(world.find(f"model[@name='{model_name(identifier)}']") is not None
           for identifier, *_ in markers):
        parser.error('Template already contains one of these checkpoint models.')
    floor_maps = list(root.iter('albedo_map'))
    if not floor_maps:
        parser.error('Template has no floor texture.')
    floor_maps[0].text = sdf_path(output, floor)
    floor_model = world.find("model[@name='track_floor']")
    if floor_model is None:
        parser.error('Template has no track_floor model.')
    for plane_size in floor_model.findall('.//plane/size'):
        plane_size.text = f'{floor_width} {floor_height}'
    # Keep the generated world small: reference the reusable robot model rather
    # than copying its links, wheels, sensors, and plugins into every map world.
    inline_robot = world.find("model[@name='amr']")
    if inline_robot is not None:
        world.remove(inline_robot)
    robot_include = ET.SubElement(world, 'include')
    ET.SubElement(robot_include, 'uri').text = robot_uri
    ET.SubElement(robot_include, 'name').text = 'amr'
    ET.SubElement(robot_include, 'pose').text = ' '.join(str(value) for value in robot_pose)

    qr_dir.mkdir(parents=True, exist_ok=True)
    for identifier, x, y, yaw, png in markers:
        qr = qrcode.QRCode(version=1, error_correction=qrcode.constants.ERROR_CORRECT_M,
                           box_size=16, border=4)
        qr.add_data(identifier)
        qr.make(fit=False)
        qr.make_image(fill_color='black', back_color='white').convert('RGB').save(png)
        add_marker(world, identifier, x, y, yaw, sdf_path(output, png))
    ET.indent(root, space='  ')
    output.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(output, encoding='utf-8', xml_declaration=True)
    print(f'Created {output}')
    if track is not None:
        print(f'Created composited floor texture {floor}')
    print(f'Created {len(markers)} QR texture(s) in {qr_dir}')
    print('Template world and current tuned worlds were not modified.')


if __name__ == '__main__':
    main()
