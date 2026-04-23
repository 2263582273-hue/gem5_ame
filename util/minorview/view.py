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


import re

import cairo
import gi
from gi.repository import GLib

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gdk
from gi.repository import Gtk as gtk

from . import (
    blobs,
    colours,
    model,
    parse,
)
from .model import (
    BlobDataSelect,
    BlobModel,
    Id,
    special_state_chars,
)
from .point import Point


class BlobView:
    """The canvas view of the pipeline"""

    def __init__(self, model):
        # A unit blob will appear at size blobSize inside a space of
        #   size pitch.
        self.blobSize = Point(45.0, 45.0)
        self.pitch = Point(60.0, 60.0)
        self.origin = Point(50.0, 50.0)
        # Some common line definitions to cut down on arbitrary
        #   set_line_widths
        self.thickLineWidth = 10.0
        self.thinLineWidth = 4.0
        self.midLineWidth = 6.0
        # The scale from the units of pitch to device units (nominally
        #   pixels for 1.0 to 1.0
        self.masterScale = Point(0.6, 0.6)
        self.model = model
        self.fillColour = colours.emptySlotColour
        self.timeIndex = 0
        self.time = 0
        self.positions = []
        self.controlbar = None
        # The sequence number selector state
        self.dataSelect = BlobDataSelect()
        # Offset of this view's time from self.time used for miniviews
        #   This is actually an offset of the index into the array of times
        #   seen in the event file)
        self.timeOffset = 0
        # Maximum view size for initial window mapping
        self.initialHeight = 600.0

        # Overlays are speech bubbles explaining blob data
        self.overlays = []

        self.da = gtk.DrawingArea()

        def draw(widget, cr):
            positions = self.draw_to_cr(cr)  # 用当前 self.time 绘制
            positions.reverse()
            self.positions = positions  # 点击命中用
            return False

        self.da.connect("draw", draw)
        # Handy offsets from the blob size
        self.blobIndent = (self.pitch - self.blobSize).scale(0.5)
        self.blobIndentFactor = self.blobIndent / self.pitch

    def set_scale(self, s):
        """设置缩放并刷新画布大小"""
        s = max(0.2, min(3.0, float(s)))  # 限定 0.2x ~ 3x
        self.masterScale = Point(s, s)
        self.set_da_size()
        self.redraw()

    def zoom(self, factor):
        """按倍数缩放（例如 0.8 缩小、1.25 放大）"""
        self.set_scale(self.masterScale.x * float(factor))

    def add_control_bar(self, controlbar):
        """Add a BlobController to this view"""
        self.controlbar = controlbar

    def draw_to_png(self, filename):
        """Draw the view to a PNG file"""
        surface = cairo.ImageSurface(
            cairo.FORMAT_ARGB32,
            self.da.get_allocation().width,
            self.da.get_allocation().height,
        )
        cr = cairo.Context(surface)
        self.draw_to_cr(cr)
        surface.write_to_png(filename)

    def draw_to_cr(self, cr):
        """Draw to a given CairoContext"""
        colours.set_source_color(cr, colours.backgroundColour)
        cr.set_line_width(self.thickLineWidth)
        cr.paint()
        cr.save()
        cr.scale(*self.masterScale.to_pair())
        cr.translate(*self.origin.to_pair())

        positions = []  # {}

        # Draw each blob
        for blob in self.model.blobs:
            blob_event = self.model.find_unit_event_by_time(
                blob.unit, self.time
            )

            cr.save()
            pos = blob.render(cr, self, blob_event, self.dataSelect, self.time)
            cr.restore()
            if pos is not None:
                (centre, size) = pos
                positions.append((blob, centre, size))

        # Draw all the overlays over the top
        for overlay in self.overlays:
            overlay.show(cr)

        cr.restore()

        return positions

    def redraw(self):
        """Redraw the whole view"""
        self.da.queue_draw()

    def set_time_index(self, idx):
        """将视图时间设置为 times[idx]（安全截断），并重绘。"""
        if not self.model.times:
            self.timeIndex = 0
            self.time = 0
        else:
            idx = max(0, min(idx, len(self.model.times) - 1))
            self.timeIndex = idx
            self.time = self.model.times[idx]
        self.redraw()

    def get_pic_size(self):
        """Return the size of ASCII-art picture of the pipeline scaled by
        the blob pitch"""
        return self.origin + self.pitch * (
            self.model.picSize + Point(1.0, 1.0)
        )

    def set_da_size(self):
        """让 DrawingArea 的请求尺寸等于画布天然像素大小（可滚动看全）"""
        pic_w, pic_h = (
            self.get_pic_size().to_pair()
        )  # 画布的自然宽高（未乘缩放）
        # 乘以缩放后再加一点边距，避免边缘被紧贴裁切
        W = int(pic_w * self.masterScale.x) + 40
        H = int(pic_h * self.masterScale.y) + 40
        self.da.set_size_request(W, H)


class BlobController:
    """The controller bar for the viewer"""

    def __init__(
        self, model, view, defaultEventFile="", defaultPictureFile=""
    ):
        self.model = model
        self.view = view
        self.playTimer = None
        self.filenameEntry = gtk.Entry()
        self.filenameEntry.set_text(defaultEventFile)
        self.pictureEntry = gtk.Entry()
        self.pictureEntry.set_text(defaultPictureFile)
        self.timeEntry = None
        self.defaultEventFile = defaultEventFile
        self.startTime = None
        self.endTime = None

        self.otherViews = []

        def make_bar(elems):
            box = gtk.HBox(homogeneous=False, spacing=2)
            box.set_border_width(2)
            for widget, signal, handler in elems:
                if signal is not None:
                    widget.connect(signal, handler)
                box.pack_start(widget, False, True, 0)
            return box

        self.timeEntry = gtk.Entry()

        t = gtk.ToggleButton("T")
        t.set_active(False)
        s = gtk.ToggleButton("S")
        s.set_active(True)
        p = gtk.ToggleButton("P")
        p.set_active(True)
        l = gtk.ToggleButton("L")
        l.set_active(True)
        f = gtk.ToggleButton("F")
        f.set_active(True)
        e = gtk.ToggleButton("E")
        e.set_active(True)

        # Should really generate this from above
        self.view.dataSelect.ids = set("SPLFE")

        self.bar = gtk.VBox()
        self.bar.set_homogeneous(False)

        row1 = make_bar(
            [
                (gtk.Button("Start"), "clicked", self.time_start),
                (gtk.Button("End"), "clicked", self.time_end),
                (gtk.Button("Back"), "clicked", self.time_back),
                (gtk.Button("Forward"), "clicked", self.time_forward),
                (gtk.Button("Play"), "clicked", self.time_play),
                (gtk.Button("Stop"), "clicked", self.time_stop),
                (self.timeEntry, "activate", self.time_set),
                (gtk.Label("Visible ids:"), None, None),
                (t, "clicked", self.toggle_id("T")),
                (gtk.Label("/"), None, None),
                (s, "clicked", self.toggle_id("S")),
                (gtk.Label("."), None, None),
                (p, "clicked", self.toggle_id("P")),
                (gtk.Label("/"), None, None),
                (l, "clicked", self.toggle_id("L")),
                (gtk.Label("/"), None, None),
                (f, "clicked", self.toggle_id("F")),
                (gtk.Label("."), None, None),
                (e, "clicked", self.toggle_id("E")),
                (self.filenameEntry, "activate", self.load_events),
                (gtk.Button("Reload"), "clicked", self.load_events),
            ]
        )

        self.bar.pack_start(row1, False, True, 0)
        self.set_time_index(0)

    def toggle_id(self, id):
        """One of the sequence number selector buttons has been toggled"""

        def toggle(button):
            if button.get_active():
                self.view.dataSelect.ids.add(id)
            else:
                self.view.dataSelect.ids.discard(id)

            # Always leave one thing visible
            if len(self.view.dataSelect.ids) == 0:
                self.view.dataSelect.ids.add(id)
                button.set_active(True)
            self.view.redraw()

        return toggle

    def set_time_index(self, time):
        """Set the time index in the view"""
        self.view.set_time_index(time)

        for view in self.otherViews:
            view.set_time_index(time)
            view.redraw()

        self.timeEntry.set_text(
            str(int(self.view.time)) if self.view.time is not None else ""
        )

    def time_start(self, button):
        """Start pressed"""
        self.set_time_index(0)
        self.view.redraw()

    def time_end(self, button):
        """End pressed"""
        self.set_time_index(len(self.model.times) - 1)
        self.view.redraw()

    def time_forward(self, button):
        """Step forward pressed"""
        self.set_time_index(
            min(self.view.timeIndex + 1, len(self.model.times) - 1)
        )
        self.view.redraw()
        Gdk.flush()

    def time_back(self, button):
        """Step back pressed"""
        self.set_time_index(max(self.view.timeIndex - 1, 0))
        self.view.redraw()

    def time_set(self, entry):
        """Time dialogue changed.  Need to find a suitable time
        <= the entry's time"""
        newTime = self.model.find_time_index(int(entry.get_text()))
        self.set_time_index(newTime)
        self.view.redraw()

    def time_step(self):
        """Time step while playing"""
        if (
            not self.playTimer
            or self.view.timeIndex == len(self.model.times) - 1
        ):
            self.time_stop(None)
            return False
        else:
            self.time_forward(None)
            return True

    def time_play(self, play):
        """Automatically advance time every 100 ms"""
        if not self.playTimer:
            self.playTimer = GLib.timeout_add(100, self.time_step)

    def time_stop(self, play):
        """Stop play pressed"""
        if self.playTimer:
            GLib.source_remove(self.playTimer)
            self.playTimer = None

    def load_events(self, button):
        """Reload events file"""
        self.model.load_events(
            self.filenameEntry.get_text(),
            startTime=self.startTime,
            endTime=self.endTime,
        )
        self.set_time_index(
            min(len(self.model.times) - 1, self.view.timeIndex)
        )
        self.view.redraw()


class Overlay:
    """An Overlay is a speech bubble explaining the data in a blob"""

    def __init__(self, model, view, point, blob):
        self.model = model
        self.view = view
        self.point = point
        self.blob = blob

    def find_event(self):
        """Find the event for a changing time and a fixed blob"""
        return self.model.find_unit_event_by_time(
            self.blob.unit, self.view.time
        )

    def show(self, cr):
        """Draw the overlay"""
        event = self.find_event()

        if event is None:
            return

        insts = event.find_ided_objects(self.model, self.blob.picChar, False)

        cr.set_line_width(self.view.thinLineWidth)
        cr.translate(*(Point(0.0, 0.0) - self.view.origin).to_pair())
        cr.scale(*(Point(1.0, 1.0) / self.view.masterScale).to_pair())

        # Get formatted data from the insts to format into a table
        lines = list(inst.table_line() for inst in insts)

        text_size = 10.0
        cr.set_font_size(text_size)

        def text_width(str):
            xb, yb, width, height, dx, dy = cr.text_extents(str)
            return width

        # Find the maximum number of columns and the widths of each column
        num_columns = 0
        for line in lines:
            num_columns = max(num_columns, len(line))

        widths = [0] * num_columns
        for line in lines:
            for i in range(len(line)):
                widths[i] = max(widths[i], text_width(line[i]))

        # Calculate the size of the speech bubble
        column_gap = 1 * text_size
        id_width = 6 * text_size
        total_width = sum(widths) + id_width + column_gap * (num_columns + 1)
        gap_step = Point(1.0, 0.0).scale(column_gap)

        text_point = self.point
        text_step = Point(0.0, text_size)

        size = Point(total_width, text_size * len(insts))

        # Draw the speech bubble
        blobs.speech_bubble(cr, self.point, size, text_size)
        colours.set_source_color(cr, colours.backgroundColour)
        cr.fill_preserve()
        colours.set_source_color(cr, colours.black)
        cr.stroke()

        text_point += Point(1.0, 1.0).scale(2.0 * text_size)

        id_size = Point(id_width, text_size)

        # Draw the rows in the table
        for i in range(0, len(insts)):
            row_point = text_point
            inst = insts[i]
            line = lines[i]
            blobs.striped_box(
                cr,
                row_point + id_size.scale(0.5),
                id_size,
                inst.id.to_striped_block(self.view.dataSelect),
            )
            colours.set_source_color(cr, colours.black)

            row_point += Point(1.0, 0.0).scale(id_width)
            row_point += text_step
            # Draw the columns of each row
            for j in range(0, len(line)):
                row_point += gap_step
                cr.move_to(*row_point.to_pair())
                cr.show_text(line[j])
                row_point += Point(1.0, 0.0).scale(widths[j])

            text_point += text_step


class BlobWindow:
    """The top-level window and its mouse control"""

    def __init__(self, model, view, controller):
        self.model = model
        self.view = view
        self.controller = controller
        self.controlbar = None
        self.window = None
        self.miniViewCount = 0

    def add_control_bar(self, controlbar):
        self.controlbar = controlbar

    def show_window(self):
        self.window = gtk.Window()
        self.window.set_default_size(1280, 800)  # 默认窗口大小
        self.window.set_position(
            gtk.WindowPosition.CENTER
        )  # 置中，防止标题栏被挤出屏幕

        self.vbox = gtk.VBox()
        self.vbox.set_homogeneous(False)
        if self.controlbar:
            self.vbox.pack_start(self.controlbar, False, True, 0)
        self.zoomBar = gtk.HBox(homogeneous=False, spacing=4)
        self.zoomBar.set_border_width(2)
        self.zoomBar.pack_start(gtk.Label("Zoom:"), False, False, 0)
        btnOut = gtk.Button("-")
        btnFit = gtk.Button("Fit")
        btnIn = gtk.Button("+")
        self.zoomBar.pack_start(btnOut, False, False, 0)
        self.zoomBar.pack_start(btnFit, False, False, 0)
        self.zoomBar.pack_start(btnIn, False, False, 0)
        self.vbox.pack_start(self.zoomBar, False, True, 0)

        # 再放 scroller + 画布
        self.scroller = gtk.ScrolledWindow()
        self.scroller.set_policy(
            gtk.PolicyType.AUTOMATIC, gtk.PolicyType.AUTOMATIC
        )
        self.scroller.add(self.view.da)
        self.vbox.add(self.scroller)

        # 连接事件
        btnOut.connect("clicked", lambda *_: self.view.zoom(0.8))
        btnIn.connect("clicked", lambda *_: self.view.zoom(1.25))

        def on_zoom_fit(*_):
            # 以 scrolled window 的可视区域来计算最合适缩放
            # 注意：DA 在 ScrolledWindow → Viewport 里，取 scroller 的 allocation 即可
            alloc = self.scroller.get_allocation()
            avail_w = max(1, alloc.width - 20)
            avail_h = max(1, alloc.height - 20)

            nat_w, nat_h = (
                self.view.get_pic_size().to_pair()
            )  # 天然尺寸（未缩放）
            if nat_w <= 0 or nat_h <= 0:
                return
            s = min(avail_w / nat_w, avail_h / nat_h)
            self.view.set_scale(s)

        btnFit.connect("clicked", on_zoom_fit)

        if self.miniViewCount > 0:
            self.miniViews = []
            self.miniViewHBox = gtk.HBox(homogeneous=True, spacing=2)

            # Draw mini views
            for i in range(1, self.miniViewCount + 1):
                miniView = BlobView(self.model)
                miniView.set_time_index(0)
                miniView.masterScale = Point(0.1, 0.1)
                miniView.set_da_size()
                miniView.timeOffset = i + 1
                self.miniViews.append(miniView)
                self.miniViewHBox.pack_start(miniView.da, False, True, 0)

            self.controller.otherViews = self.miniViews
            self.vbox.add(self.miniViewHBox)

        self.window.add(self.vbox)

        def show_event(picChar, event):
            print("**** Comments for", event.unit, "at time", self.view.time)
            for name, value in event.pairs.items():
                print(name, "=", value)
            for comment in event.comments:
                print(comment)
            if picChar in event.visuals:
                # blocks = event.visuals[picChar].elems()
                print("**** Colour data")
                objs = event.find_ided_objects(self.model, picChar, True)
                for obj in objs:
                    print(" ".join(obj.table_line()))

        def clicked_da(da, b):
            point = Point(b.x, b.y)

            overlay = None
            for blob, centre, size in self.view.positions:
                if point.is_within_box((centre, size)):
                    event = self.model.find_unit_event_by_time(
                        blob.unit, self.view.time
                    )
                    if event is not None:
                        if overlay is None:
                            overlay = Overlay(
                                self.model, self.view, point, blob
                            )
                        show_event(blob.picChar, event)
            if overlay is not None:
                self.view.overlays = [overlay]
            else:
                self.view.overlays = []

            self.view.redraw()

        # Set initial size and event callbacks
        self.view.set_da_size()
        self.view.da.add_events(Gdk.EventMask.BUTTON_PRESS_MASK)
        self.view.da.connect("button-press-event", clicked_da)
        self.window.connect("destroy", lambda widget: gtk.main_quit())

        def resize(widget, allocation):
            # 只重绘，别改 masterScale
            self.view.overlays = []
            self.view.redraw()

        self.scroller.connect("size-allocate", resize)

        self.window.show_all()
        GLib.idle_add(lambda: (on_zoom_fit(), False))
