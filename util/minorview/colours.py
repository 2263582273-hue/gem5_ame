# Copyright (c) 2013 ARM Limited
# All rights reserved
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import cairo
import gi

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gdk
from gi.repository import Gtk as gtk


def parse_color(color_str: str) -> Gdk.RGBA:
    rgba = Gdk.RGBA()
    if not rgba.parse(color_str):
        # 如果解析失败，给一个默认的显眼颜色
        rgba.red, rgba.green, rgba.blue, rgba.alpha = (
            1.0,
            0.0,
            1.0,
            1.0,
        )  # magenta
    return rgba


def set_source_color(cr, rgba):
    """Use a Gdk.RGBA with a Cairo context"""
    cr.set_source_rgba(rgba.red, rgba.green, rgba.blue, rgba.alpha)


# All the miscellaneous colours used in the interface
unknownColour = parse_color("grey80")
blockedColour = parse_color("grey")
bubbleColour = parse_color("bisque")
emptySlotColour = parse_color("grey90")
reservedSlotColour = parse_color("cyan")
errorColour = parse_color("blue")
backgroundColour = parse_color("white")
faultColour = parse_color("dark cyan")
readColour = parse_color("red")
writeColour = parse_color("white")
black = parse_color("black")


def name_to_colour(name):
    """Convert a colour name to a GdkColor"""
    try:
        ret = parse_color(name)
    except:
        ret = unknownColour
    return ret


number_colour_code = list(
    map(
        name_to_colour,
        [
            "black",
            "brown",
            "red",
            "orange",
            "yellow",
            "green",
            "blue",
            "violet",
            "grey",
            "white",
        ],
    )
)


def number_to_colour(num):
    """Convert the last decimal digit of an integer into a resistor
    stripe colour"""
    return number_colour_code[num % 10]
