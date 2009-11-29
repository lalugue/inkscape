#!/usr/bin/env python

"""
wmf_output.py
Module for running UniConverter wmf exporting commands in Inkscape extensions

Copyright (C) 2009 Nicolas Dufour (jazzynico)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
"""

import sys
from uniconv_output import run, get_command

cmd = get_command()
run((cmd + ' "%s" ') % sys.argv[1], "UniConvertor", ".wmf")

# vim: expandtab shiftwidth=4 tabstop=8 softtabstop=4 encoding=utf-8 textwidth=99