#!/usr/bin/env python3
"""
Setup script that reads metadata from pyproject.toml
and defines C extension modules.
"""
from setuptools import setup, Extension

# Metadata comes from pyproject.toml
# C extensions must be defined here as pyproject.toml doesn't support them in the standard format
setup(
    ext_modules=[
        Extension('cpmapi', ['pmapi.c'], libraries=['pcp']),
        Extension('cpmda', ['pmda.c'], libraries=['pcp_pmda', 'pcp']),
        Extension('cpmgui', ['pmgui.c'], libraries=['pcp_gui', 'pcp']),
        Extension('cpmi', ['pmi.c'], libraries=['pcp_import', 'pcp']),
        Extension('cmmv', ['mmv.c'], libraries=['pcp_mmv', 'pcp']),
    ],
)
