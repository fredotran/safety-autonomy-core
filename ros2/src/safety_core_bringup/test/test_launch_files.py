# Copyright (c) 2026 RedEarth OS
# SPDX-License-Identifier: MIT

"""Structural validation of safety_core_bringup assets.

Runs in colcon test via ament_cmake_pytest — no nodes are started. Catches the
two most common breakage classes: Python syntax errors in launch files and
unparseable YAML/XMl configs, both of which otherwise only surface at demo
launch time.
"""

import py_compile
from pathlib import Path

import pytest
import yaml

PKG = Path(__file__).resolve().parent.parent
LAUNCH_DIR = PKG / "launch"
CONFIG_DIR = PKG / "config"
RVIZ_DIR = PKG / "rviz"

LAUNCH_FILES = sorted(LAUNCH_DIR.glob("*.launch.py"))
YAML_FILES = sorted(CONFIG_DIR.glob("*.yaml"))
RVIZ_FILES = sorted(RVIZ_DIR.glob("*.rviz"))


def test_launch_files_exist():
    assert LAUNCH_FILES, f"no launch files found in {LAUNCH_DIR}"


def test_yaml_files_exist():
    assert YAML_FILES, f"no yaml configs found in {CONFIG_DIR}"


@pytest.mark.parametrize("launch_file", LAUNCH_FILES, ids=lambda p: p.name)
def test_launch_file_compiles(launch_file):
    py_compile.compile(str(launch_file), doraise=True)


@pytest.mark.parametrize("launch_file", LAUNCH_FILES, ids=lambda p: p.name)
def test_launch_file_returns_launch_description(launch_file):
    """Every launch file must expose generate_launch_description() or use
    the OpaqueFunction pattern — checked statically so no ROS env is needed."""
    source = launch_file.read_text()
    assert "LaunchDescription" in source, f"{launch_file.name}: no LaunchDescription"
    assert "def generate_launch_description" in source, (
        f"{launch_file.name}: missing generate_launch_description()"
    )


launch_mod = pytest.importorskip("launch", reason="launch package not importable")


@pytest.mark.parametrize("launch_file", LAUNCH_FILES, ids=lambda p: p.name)
def test_launch_file_loads(launch_file):
    """Fully evaluate the launch file through the launch API."""
    from launch.launch_description_sources import PythonLaunchDescriptionSource

    source = PythonLaunchDescriptionSource(str(launch_file))
    ld = source.get_launch_description(launch_mod.LaunchContext())
    assert ld is not None


@pytest.mark.parametrize("yaml_file", YAML_FILES, ids=lambda p: p.name)
def test_yaml_parses(yaml_file):
    content = yaml.safe_load(yaml_file.read_text())
    assert isinstance(content, dict), f"{yaml_file.name}: top-level must be a mapping"


def test_safety_params_has_expected_keys():
    content = yaml.safe_load((CONFIG_DIR / "safety_params.yaml").read_text())
    assert content is not None
    # Each entry must look like a ros__parameters block
    for node_name, body in content.items():
        assert isinstance(body, dict), f"{node_name}: not a mapping"
        assert "ros__parameters" in body, f"{node_name}: missing ros__parameters"


@pytest.mark.parametrize("rviz_file", RVIZ_FILES, ids=lambda p: p.name)
def test_rviz_config_is_valid_yaml(rviz_file):
    # RViz 2 configs are YAML (not XML).
    content = yaml.safe_load(rviz_file.read_text())
    assert isinstance(content, dict), f"{rviz_file.name}: not a mapping"
