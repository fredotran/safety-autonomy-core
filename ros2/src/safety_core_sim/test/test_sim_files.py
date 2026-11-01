# Copyright (c) 2026 RedEarth OS
# SPDX-License-Identifier: MIT

"""Structural validation of safety_core_sim assets.

XML well-formedness for URDF/xacro/SDF and YAML parse for the ros_gz bridge
config — the failures these catch normally surface as opaque Gazebo startup
crashes.
"""

import py_compile
import xml.etree.ElementTree as ET
from pathlib import Path

import pytest
import yaml

PKG = Path(__file__).resolve().parent.parent


@pytest.mark.parametrize(
    "xml_file",
    sorted((PKG / "description").rglob("*.xacro")) + sorted((PKG / "description").rglob("*.urdf")),
    ids=lambda p: p.name,
)
def test_description_xml_wellformed(xml_file):
    # xacro files contain ${...} expressions but must still be well-formed XML
    ET.parse(str(xml_file))


@pytest.mark.parametrize(
    "world_file",
    sorted((PKG / "worlds").glob("*.sdf")),
    ids=lambda p: p.name,
)
def test_world_sdf_wellformed(world_file):
    root = ET.parse(str(world_file)).getroot()
    assert root.tag == "sdf"


@pytest.mark.parametrize(
    "yaml_file",
    sorted((PKG / "config").glob("*.yaml")),
    ids=lambda p: p.name,
)
def test_bridge_yaml_parses(yaml_file):
    yaml.safe_load(yaml_file.read_text())


@pytest.mark.parametrize(
    "launch_file",
    sorted((PKG / "launch").glob("*.launch.py")),
    ids=lambda p: p.name,
)
def test_launch_file_compiles(launch_file):
    py_compile.compile(str(launch_file), doraise=True)
