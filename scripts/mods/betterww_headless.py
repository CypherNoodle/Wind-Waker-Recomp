#!/usr/bin/env python3
"""Run Better Wind Waker's patcher without its Qt window.

usage: betterww_headless.py BETTERWW_SRC CLEAN.iso OUT_DIR   (writes OUT_DIR/bluewake.iso)
Uses Better Wind Waker's own default settings (settings.txt).
"""
import os
import sys
from collections import OrderedDict

src, iso, out = sys.argv[1:4]
iso, out = os.path.abspath(iso), os.path.abspath(out)
sys.path.insert(0, src)
os.chdir(src)
import yaml  # noqa: E402
from randomizer import Randomizer  # noqa: E402

options = yaml.safe_load(open("settings.txt"))
options = {k: v for k, v in options.items() if k not in ("clean_iso_path", "output_folder", "seed")}
os.makedirs(out, exist_ok=True)
patcher = Randomizer("bluewake", iso, out, options, cmd_line_args=OrderedDict([("-nologs", None)]))
for message, _ in patcher.randomize():
    print("betterww:", message, flush=True)
