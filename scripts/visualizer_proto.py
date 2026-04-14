"""Shared protobuf imports and protocol constants for the visualizer."""

from __future__ import annotations

import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PROTO_DIR = os.path.join(REPO_ROOT, "Appli", "Proto")
POWER_PROTO_DIR = os.path.join(REPO_ROOT, "External", "fyp-power-measure")

for proto_dir in (PROTO_DIR, POWER_PROTO_DIR):
    if proto_dir not in sys.path:
        sys.path.insert(0, proto_dir)

import messages_pb2  # noqa: E402
import power_sample_pb2  # noqa: E402

# TofAlert.flags (protobuf); keep in sync with Appli/Proto/messages.proto
TOF_PB_FLAG_ALERT = 1 << 0
TOF_PB_FLAG_STALE = 1 << 1
