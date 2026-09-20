#!/usr/bin/env python3
"""Add a ROS-free Fortress robot to a COPY of an existing track world.

Usage: python3 add_amr.py /path/to/track_world.sdf
Outputs alongside input: amr_model.sdf, track_with_robot.sdf.
Original world and texture are unchanged. Python standard library only.
Robot front is +X, left is +Y, up is +Z. Coordinates are metres.
This installs the robot and raw camera streams, not autonomous control or
QR decoding. Native velocity topic: /amr/cmd_vel (ignition.msgs.Twist).
"""
import argparse
import copy
import math
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


def tag(parent, element_name, text=None, **attrs):
    node = ET.SubElement(parent, element_name, attrs)
    if text is not None:
        node.text = str(text)
    return node


def inertia(link, mass, xyz):
    node = tag(link, 'inertial')
    tag(node, 'mass', mass)
    tensor = tag(node, 'inertia')
    for key, value in zip(('ixx', 'iyy', 'izz'), xyz):
        tag(tensor, key, value)
    for key in ('ixy', 'ixz', 'iyz'):
        tag(tensor, key, 0)


def shape(link, name, kind, dimensions, pose='0 0 0 0 0 0',
          color='0.05 0.35 0.8 1', friction=1.0, collision=True):
    for node_type in (('visual', 'collision') if collision else ('visual',)):
        node = tag(link, node_type, name=name + '_' + node_type)
        tag(node, 'pose', pose)
        geom = tag(tag(node, 'geometry'), kind)
        for key, value in dimensions.items():
            tag(geom, key, value)
        if node_type == 'visual':
            mat = tag(node, 'material')
            tag(mat, 'ambient', color)
            tag(mat, 'diffuse', color)
        else:
            ode = tag(tag(tag(node, 'surface'), 'friction'), 'ode')
            tag(ode, 'mu', friction)
            tag(ode, 'mu2', friction)


def camera(link, name, x, y, z, width, height, fov, rate):
    sensor = tag(link, 'sensor', name=name, type='camera')
    # Gazebo camera looks along +X; positive 90-degree pitch points down.
    tag(sensor, 'pose', f'{x} {y} {z} 0 {math.pi / 2} 0')
    tag(sensor, 'always_on', 'true')
    tag(sensor, 'update_rate', rate)
    tag(sensor, 'topic', f'/amr/{name}/image')
    cam = tag(sensor, 'camera')
    tag(cam, 'horizontal_fov', fov)
    img = tag(cam, 'image')
    tag(img, 'width', width)
    tag(img, 'height', height)
    tag(img, 'format', 'R8G8B8')
    clip = tag(cam, 'clip')
    tag(clip, 'near', 0.005)
    tag(clip, 'far', 2)


def build_robot(x, y, yaw):
    model = ET.Element('model', name='amr')
    tag(model, 'pose', f'{x} {y} 0.005 0 0 {yaw}')
    tag(model, 'static', 'false')
    base = tag(model, 'link', name='base_link')
    tag(base, 'pose', '0 0 0.16 0 0 0')
    # 4 kg, 0.40 x 0.28 x 0.08 m box.
    inertia(base, 4, (4*(.28**2+.08**2)/12,
                      4*(.40**2+.08**2)/12,
                      4*(.40**2+.28**2)/12))
    shape(base, 'chassis', 'box', {'size': '0.40 0.28 0.08'})
    shape(base, 'front_mark', 'box', {'size': '0.04 0.20 0.005'},
          '0.16 0 0.043 0 0 0', '1 0.55 0.05 1', collision=False)

    # Cameras below the chassis; lenses sit below their visible housings.
    shape(base, 'sensor_strip', 'box', {'size': '0.025 0.11 0.012'},
          '0.17 0 -0.049 0 0 0', '0.1 0.1 0.1 1', collision=False)
    for i, lateral in enumerate((.04, .02, 0, -.02, -.04)):
        camera(base, f'line_{i}', .17, lateral, -.06, 16, 16, .10, 15)
        shape(base, f'line_housing_{i}', 'box', {'size': '0.008 0.008 0.004'},
              f'0.17 {lateral} -0.057 0 0 0', '0.9 0.1 0.1 1', collision=False)
    camera(base, 'qr', 0, 0, -.06, 640, 480, 1.8, 5)
    shape(base, 'qr_housing', 'box', {'size': '0.025 0.025 0.012'},
          '0 0 -0.049 0 0 0', '0.1 0.1 0.1 1', collision=False)

    for side, lateral in (('left', .17), ('right', -.17)):
        sign = 1 if lateral > 0 else -1
        # Visual-only brackets join the chassis bottom to the wheel axle.
        shape(base, side+'_motor_mount', 'box', {'size': '0.06 0.026 0.055'},
              f'0 {sign*.122} -0.0575 0 0 0',
              '0.22 0.25 0.28 1', collision=False)
        shape(base, side+'_axle', 'cylinder', {'radius': .012, 'length': .05},
              f'0 {sign*.145} -0.08 {math.pi/2} 0 0',
              '0.65 0.68 0.72 1', collision=False)
        name = side + '_wheel'
        link = tag(model, 'link', name=name)
        tag(link, 'pose', f'0 {lateral} 0.08 0 0 0')
        radial = .3*(3*.08**2+.04**2)/12
        inertia(link, .3, (radial, .3*.08**2/2, radial))
        shape(link, name, 'cylinder', {'radius': .08, 'length': .04},
              f'0 0 0 {math.pi/2} 0 0', '0.08 0.08 0.08 1')
        shape(link, side+'_hub', 'cylinder', {'radius': .028, 'length': .004},
              f'0 {sign*.021} 0 {math.pi/2} 0 0',
              '0.65 0.68 0.72 1', collision=False)
        joint = tag(model, 'joint', name=name+'_joint', type='revolute')
        tag(joint, 'parent', 'base_link')
        tag(joint, 'child', name)
        axis = tag(joint, 'axis')
        tag(axis, 'xyz', '0 1 0')
        limits = tag(axis, 'limit')
        tag(limits, 'lower', -1e16)
        tag(limits, 'upper', 1e16)
        tag(limits, 'effort', 10)
        tag(limits, 'velocity', 20)

    # Low-friction spherical skids approximate caster supports for this demo.
    # These are intentionally not detailed swivel-caster mechanisms.
    for side, longitudinal in (('front', .12), ('rear', -.15)):
        # Ball-transfer style mounting hardware; original spherical skid
        # collision and friction remain unchanged. Bottom of ball is exposed.
        shape(base, side+'_support_plate', 'box', {'size': '0.065 0.065 0.008'},
              f'{longitudinal} 0 -0.04 0 0 0',
              '0.25 0.28 0.32 1', collision=False)
        shape(base, side+'_support_stem', 'cylinder', {'radius': .013, 'length': .052},
              f'{longitudinal} 0 -0.066 0 0 0',
              '0.65 0.68 0.72 1', collision=False)
        shape(base, side+'_support_housing', 'cylinder', {'radius': .033, 'length': .022},
              f'{longitudinal} 0 -0.10 0 0 0',
              '0.25 0.28 0.32 1', collision=False)
        name = side + '_support'
        link = tag(model, 'link', name=name)
        tag(link, 'pose', f'{longitudinal} 0 0.03 0 0 0')
        inertia(link, .1, (0.000036,)*3)
        shape(link, name, 'sphere', {'radius': .03},
              color='0.5 0.5 0.5 1', friction=.001)
        joint = tag(model, 'joint', name=name+'_joint', type='fixed')
        tag(joint, 'parent', 'base_link')
        tag(joint, 'child', name)

    drive = tag(model, 'plugin',
                filename='libignition-gazebo-diff-drive-system.so',
                name='ignition::gazebo::systems::DiffDrive')
    for key, value in {
        'left_joint': 'left_wheel_joint',
        'right_joint': 'right_wheel_joint',
        'wheel_separation': .34, 'wheel_radius': .08,
        'topic': '/amr/cmd_vel', 'odom_topic': '/amr/odometry',
        'odom_publish_frequency': 30,
    }.items():
        tag(drive, key, value)
    return model


def write_xml(root, path):
    ET.indent(root, space='  ')
    ET.ElementTree(root).write(path, encoding='utf-8', xml_declaration=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('world', type=Path)
    parser.add_argument('--x', type=float, default=0)
    parser.add_argument('--y', type=float, default=-2)
    parser.add_argument('--yaw', type=float, default=0, help='radians')
    parser.add_argument('--force', action='store_true', help='replace generated outputs')
    args = parser.parse_args()
    source = args.world.expanduser().resolve()
    root = ET.parse(source).getroot()
    world = root.find('world')
    if world is None:
        parser.error('Input must contain an SDF world.')
    if world.find("model[@name='amr']") is not None:
        parser.error('Input already contains amr. Use the original track_world.sdf.')
    model_path = source.parent / 'amr_model.sdf'
    output = source.parent / 'track_with_robot.sdf'
    if source in (model_path, output):
        parser.error('Use the original track_world.sdf, not a generated output.')
    for path in (model_path, output):
        if path.exists() and not args.force:
            parser.error(f'{path} exists. Use --force to regenerate these output files.')

    # Resolve local textures while retaining model:// and other resource URIs.
    for node in root.iter('albedo_map'):
        value = (node.text or '').strip()
        candidate = source.parent / value
        if value and '://' not in value and candidate.is_file():
            node.text = str(candidate.resolve())

    for system, filename in (
        ('Physics', 'physics'), ('UserCommands', 'user-commands'),
        ('SceneBroadcaster', 'scene-broadcaster'), ('Sensors', 'sensors')):
        name = 'ignition::gazebo::systems::' + system
        plugin = next((p for p in world.findall('plugin') if p.get('name') == name), None)
        if plugin is None:
            plugin = tag(world, 'plugin', name=name,
                         filename=f'libignition-gazebo-{filename}-system.so')
        if system == 'Sensors':
            engine = plugin.find('render_engine')
            if engine is None:
                engine = tag(plugin, 'render_engine')
            engine.text = 'ogre2'

    robot = build_robot(args.x, args.y, args.yaw)
    model_root = ET.Element('sdf', version='1.8')
    model_root.append(copy.deepcopy(robot))
    world.append(robot)
    write_xml(model_root, model_path)
    write_xml(root, output)
    print(f'Created: {model_path}\nCreated: {output}')
    print('Original world unchanged. Launch track_with_robot.sdf and press Play.')
    print('Robot starts near the bottom straight at x=0, y=-2, facing +X.')
    print('Streams: /amr/line_0/image through /amr/line_4/image; /amr/qr/image')
    print('line_0 is leftmost; line_4 is rightmost. Raw RGB streams, not binary yet.')
    print('No automatic line following or QR decoding is installed by this script.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ET.ParseError) as error:
        sys.exit(str(error))
