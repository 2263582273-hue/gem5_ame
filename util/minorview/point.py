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


class Point:
    """2D point coordinates/size type"""

    def __init__(self, x=0.0, y=0.0):
        # 统一用 float，避免整数整除在 Python3 下带来意外
        self.x = float(x)
        self.y = float(y)

    # ---------- 基础算术：支持 Point ⊕ Point 和 Point ⊕ 标量 ----------
    def __add__(self, rhs):
        if isinstance(rhs, Point):
            return Point(self.x + rhs.x, self.y + rhs.y)
        return Point(self.x + rhs, self.y + rhs)

    def __radd__(self, lhs):
        # 标量在左侧：标量 + Point
        return self.__add__(lhs)

    def __sub__(self, rhs):
        if isinstance(rhs, Point):
            return Point(self.x - rhs.x, self.y - rhs.y)
        return Point(self.x - rhs, self.y - rhs)

    def __rsub__(self, lhs):
        # 标量在左侧：标量 - Point
        if isinstance(lhs, Point):
            return Point(lhs.x - self.x, lhs.y - self.y)
        return Point(lhs - self.x, lhs - self.y)

    def __mul__(self, rhs):
        # 逐分量相乘或标量缩放
        if isinstance(rhs, Point):
            return Point(self.x * rhs.x, self.y * rhs.y)
        return Point(self.x * rhs, self.y * rhs)

    def __rmul__(self, lhs):
        # 标量在左侧：标量 * Point
        return self.__mul__(lhs)

    # ---------- Python3 真除法 ----------
    def __truediv__(self, rhs):
        # 支持 Point / Point（逐分量），或 Point / 标量
        if isinstance(rhs, Point):
            return Point(self.x / rhs.x, self.y / rhs.y)
        return Point(self.x / rhs, self.y / rhs)

    def __rtruediv__(self, lhs):
        # 标量 / Point（逐分量）或 Point / Point 的反向形式
        if isinstance(lhs, Point):
            return Point(lhs.x / self.x, lhs.y / self.y)
        return Point(lhs / self.x, lhs / self.y)

    # ---------- Python2 兼容（无害保留） ----------
    def __div__(self, rhs):
        # 在 Python3 不会被调用；保留以兼容旧代码
        if isinstance(rhs, Point):
            return Point(self.x / rhs.x, self.y / rhs.y)
        return Point(self.x / rhs, self.y / rhs)

    # ---------- 辅助方法 ----------
    def scale(self, factor):
        return Point(self.x * factor, self.y * factor)

    def to_pair(self):
        return (self.x, self.y)

    def __str__(self):
        return f"Point({self.x:.6f},{self.y:.6f})"

    def __repr__(self):
        return f"Point({self.x:.6f},{self.y:.6f})"

    def is_within_box(self, box):
        """Is this point inside the (centre, size) box box"""
        centre, size = box
        half_size = size.scale(0.5)
        top_left = centre - half_size
        bottom_right = centre + half_size
        return (
            top_left.x < self.x
            and top_left.y < self.y
            and bottom_right.x > self.x
            and bottom_right.y > self.y
        )
