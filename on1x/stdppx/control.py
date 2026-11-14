#!/usr/bin/env python3
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'stdlib'))
from ppx import serve
from passes import control
serve(control)
