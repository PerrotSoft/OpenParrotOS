#!/usr/bin/env python3
# parrot_paint.py
# ParrotSoft / ParrotOS — ParrotPaint (host)
# PyQt5 paint-like editor:
# - Canvas resize, pixel zoom, grid (toggle), selection/clipboard, undo/redo
# - PNG save, SVG export, C uint32_t export (ARGB or RGB)
# - Window icon auto from "ico.ico" or "ico project.svg" (optional)

import sys, os, math, time
from PyQt5 import QtCore, QtGui, QtWidgets

APP_TITLE = "ParrotSoft ParrotOS — ParrotPaint"
DEFAULT_ICON_CANDIDATES = ["ico.ico", "ico project.svg"]

# -------------------------- Defaults --------------------------
DEF_W, DEF_H = 256, 256
DEF_BG = QtGui.QColor(255, 255, 255, 255)
DEF_FG = QtGui.QColor(0, 0, 0, 255)
DEF_PX_SCALE = 8        # 1 logical pixel = 8x8 screen pixels
DEF_GRID = True
DEF_GRID_STEP = 1       # draw grid every N logical pixels

# -------------------------- Tools --------------------------
TOOL_PEN   = 0
TOOL_LINE  = 1
TOOL_RECT  = 2
TOOL_FILL  = 3
TOOL_ERASE = 4
TOOL_PICK  = 5
TOOL_SEL   = 6

# -------------------------- Utils --------------------------
def clamp(v, a, b): return max(a, min(b, v))

def qimage_copy(img: QtGui.QImage) -> QtGui.QImage:
    # deep copy
    return img.copy()

# -------------------------- Selection Layer --------------------------
class SelectionLayer(QtCore.QObject):
    """
    Holds a rectangular selection taken from the canvas as a QImage fragment.
    Provides move/paint/commit/cancel operations.
    """
    changed = QtCore.pyqtSignal()

    def __init__(self):
        super().__init__()
        self.active = False
        self.rect = QtCore.QRect()
        self.image = QtGui.QImage()
        self.offset = QtCore.QPoint(0,0)  # cursor-drag offset from rect.topLeft
        self.dragging = False
        self.drag_start_pos = QtCore.QPoint()

    def clear(self):
        self.active = False
        self.rect = QtCore.QRect()
        self.image = QtGui.QImage()
        self.dragging = False
        self.changed.emit()

    def set_from_canvas(self, canvas_img: QtGui.QImage, rect: QtCore.QRect):
        r = rect.normalized().intersected(QtCore.QRect(0,0, canvas_img.width(), canvas_img.height()))
        if r.isEmpty():
            self.clear()
            return
        self.rect = r
        self.image = canvas_img.copy(r)
        self.active = True
        self.dragging = False
        self.changed.emit()

    def paint(self, painter: QtGui.QPainter, scale: int):
        if not self.active or self.image.isNull():
            return
        # draw floating selection at self.rect (already updated)
        # draw shadow
        target = QtCore.QRect(self.rect.left()*scale, self.rect.top()*scale,
                              self.rect.width()*scale, self.rect.height()*scale)
        painter.setOpacity(0.9)
        painter.drawImage(target, self.image)
        painter.setOpacity(1.0)
        # draw marquee
        pen = QtGui.QPen(QtGui.QColor(0,0,0,255))
        pen.setWidth( max(1, scale//4) )
        painter.setPen(pen)
        painter.setBrush(QtCore.Qt.NoBrush)
        painter.drawRect(target.adjusted(0,0,-max(1,pen.width()),-max(1,pen.width())))

    def hit(self, pos_px: QtCore.QPoint, scale: int) -> bool:
        """pos_px — logical pixel coordinates (x,y) in canvas coords"""
        return self.active and self.rect.contains(pos_px)

    def start_drag(self, pos_px: QtCore.QPoint):
        self.dragging = True
        self.drag_start_pos = pos_px
        self.offset = pos_px - self.rect.topLeft()

    def drag_move(self, pos_px: QtCore.QPoint, canvas_w: int, canvas_h: int):
        if not self.dragging: return
        topleft = pos_px - self.offset
        # clamp inside canvas
        topleft.setX(clamp(topleft.x(), 0, canvas_w - self.rect.width()))
        topleft.setY(clamp(topleft.y(), 0, canvas_h - self.rect.height()))
        self.rect.moveTopLeft(topleft)
        self.changed.emit()

    def end_drag(self):
        self.dragging = False
        self.changed.emit()

    def commit_to_canvas(self, canvas_img: QtGui.QImage):
        if not self.active or self.image.isNull(): return
        painter = QtGui.QPainter(canvas_img)
        painter.drawImage(self.rect, self.image)
        painter.end()
        self.clear()

# -------------------------- Canvas --------------------------
class CanvasWidget(QtWidgets.QWidget):
    """
    Logical canvas (QImage) + pixel-scale painting. Supports:
    - drawing tools
    - grid rendering
    - selection layer
    - undo/redo snapshots
    """
    status_message = QtCore.pyqtSignal(str)
    color_changed = QtCore.pyqtSignal(QtGui.QColor)

    def __init__(self, w=DEF_W, h=DEF_H, parent=None):
        super().__init__(parent)
        self.setFocusPolicy(QtCore.Qt.StrongFocus)
        self.image = QtGui.QImage(w, h, QtGui.QImage.Format_ARGB32)
        self.image.fill(DEF_BG)
        self.temp = QtGui.QImage()  # preview
        self.pen_w = 1
        self.fg = DEF_FG
        self.bg = DEF_BG
        self.tool = TOOL_PEN
        self.drawing = False
        self.last_px = QtCore.QPoint()
        self.scale = DEF_PX_SCALE  # logical→screen
        self.show_grid = DEF_GRID
        self.grid_step = DEF_GRID_STEP
        self.strokes = []  # for SVG export: (color,width,[(x,y),...])
        self.current_stroke = None

        # selection layer
        self.sel = SelectionLayer()
        self.sel.changed.connect(self.update)

        # undo/redo stacks (hold QImage copies)
        self.undo_stack = []
        self.redo_stack = []
        self.max_undo = 64

        self.setFixedSize(self.image.width()*self.scale, self.image.height()*self.scale)

    # -------- canvas size / scale ----------
    def set_canvas_size(self, w, h, keep_content=True):
        self.push_undo()
        new_img = QtGui.QImage(w, h, QtGui.QImage.Format_ARGB32)
        new_img.fill(self.bg)
        if keep_content:
            painter = QtGui.QPainter(new_img)
            painter.drawImage(0, 0, self.image)
            painter.end()
        self.image = new_img
        self.setFixedSize(self.image.width()*self.scale, self.image.height()*self.scale)
        self.update()

    def set_scale(self, s: int):
        s = max(1, min(128, s))
        self.scale = s
        self.setFixedSize(self.image.width()*self.scale, self.image.height()*self.scale)
        self.update()

    def toggle_grid(self, on=None):
        if on is None:
            self.show_grid = not self.show_grid
        else:
            self.show_grid = bool(on)
        self.update()

    def set_grid_step(self, step: int):
        self.grid_step = max(1, step)
        self.update()

    # -------- undo/redo ----------
    def push_undo(self):
        self.undo_stack.append(qimage_copy(self.image))
        if len(self.undo_stack) > self.max_undo:
            self.undo_stack.pop(0)
        self.redo_stack.clear()

    def undo(self):
        if not self.undo_stack: return
        self.redo_stack.append(qimage_copy(self.image))
        self.image = self.undo_stack.pop()
        self.update()

    def redo(self):
        if not self.redo_stack: return
        self.undo_stack.append(qimage_copy(self.image))
        self.image = self.redo_stack.pop()
        self.update()

    # -------- events ----------
    def sizeHint(self):
        return QtCore.QSize(self.image.width()*self.scale, self.image.height()*self.scale)

    def logical_from_pos(self, ev_pos: QtCore.QPoint) -> QtCore.QPoint:
        x = ev_pos.x() // self.scale
        y = ev_pos.y() // self.scale
        return QtCore.QPoint(clamp(x,0,self.image.width()-1), clamp(y,0,self.image.height()-1))

    def mousePressEvent(self, ev: QtGui.QMouseEvent):
        self.setFocus()
        px = self.logical_from_pos(ev.pos())
        self.drawing = True
        self.last_px = px

        if self.tool in (TOOL_PEN, TOOL_ERASE):
            self.push_undo()
            color = self.bg if self.tool == TOOL_ERASE else self.fg
            self.current_stroke = (QtGui.QColor(color), max(1,self.pen_w), [(px.x(),px.y())])
            self._dot(px, color, self.pen_w)
            self.update()

        elif self.tool == TOOL_FILL:
            self.push_undo()
            self._flood(px.x(), px.y(), self.fg)
            self.update()

        elif self.tool == TOOL_PICK:
            c = QtGui.QColor(self.image.pixel(px))
            self.fg = c
            self.color_changed.emit(c)
            self.status_message.emit(f"Picked color #{c.rgba():08X}")

        elif self.tool in (TOOL_LINE, TOOL_RECT):
            self.temp = self.image.copy()
            self.temp_start = px

        elif self.tool == TOOL_SEL:
            # if clicking inside active selection — start drag
            if self.sel.hit(px, self.scale):
                self.sel.start_drag(px)
            else:
                # start new marquee (store start)
                self.sel.dragging = False
                self.sel_start = px
                self.sel_rect_live = QtCore.QRect(px, px)
                self.temp = self.image.copy()

    def mouseMoveEvent(self, ev: QtGui.QMouseEvent):
        if not self.drawing: return
        px = self.logical_from_pos(ev.pos())

        if self.tool in (TOOL_PEN, TOOL_ERASE):
            color = self.bg if self.tool == TOOL_ERASE else self.fg
            self._line(self.last_px, px, color, self.pen_w)
            if self.current_stroke:
                self.current_stroke[2].append((px.x(), px.y()))
            self.last_px = px
            self.update()

        elif self.tool in (TOOL_LINE, TOOL_RECT):
            self.temp = self.image.copy()
            p = QtGui.QPainter(self.temp)
            pen = QtGui.QPen(self.fg); pen.setWidth(self.pen_w)
            pen.setCapStyle(QtCore.Qt.RoundCap); pen.setJoinStyle(QtCore.Qt.RoundJoin)
            p.setPen(pen); p.setBrush(QtCore.Qt.NoBrush)
            if self.tool == TOOL_LINE:
                p.drawLine(self.temp_start, px)
            else:
                r = QtCore.QRect(self.temp_start, px).normalized()
                p.drawRect(r)
            p.end()
            self.update()

        elif self.tool == TOOL_SEL:
            if self.sel.dragging:
                self.sel.drag_move(px, self.image.width(), self.image.height())
            else:
                self.temp = self.image.copy()
                self.sel_rect_live = QtCore.QRect(self.sel_start, px).normalized()
                # draw the marquee preview onto temp (not selection content)
                pr = QtGui.QPainter(self.temp)
                pen = QtGui.QPen(QtGui.QColor(0,0,0)); pen.setWidth(1)
                pr.setPen(pen); pr.setBrush(QtCore.Qt.NoBrush)
                r = self.sel_rect_live
                pr.drawRect(r)
                pr.end()
            self.update()

    def mouseReleaseEvent(self, ev: QtGui.QMouseEvent):
        if not self.drawing: return
        self.drawing = False
        px = self.logical_from_pos(ev.pos())

        if self.tool in (TOOL_PEN, TOOL_ERASE):
            if self.current_stroke:
                self.strokes.append(self.current_stroke)
            self.current_stroke = None

        elif self.tool == TOOL_LINE:
            self.push_undo()
            self._line(self.temp_start, px, self.fg, self.pen_w)
            self.strokes.append((QtGui.QColor(self.fg), self.pen_w,
                                 [(self.temp_start.x(), self.temp_start.y()), (px.x(), px.y())]))
            self.temp = QtGui.QImage()

        elif self.tool == TOOL_RECT:
            self.push_undo()
            r = QtCore.QRect(self.temp_start, px).normalized()
            self._rect(r, self.fg, self.pen_w, filled=False)
            self.strokes.append((QtGui.QColor(self.fg), self.pen_w,
                                 [(r.left(),r.top()), (r.right(), r.bottom())]))
            self.temp = QtGui.QImage()

        elif self.tool == TOOL_SEL:
            if self.sel.dragging:
                self.sel.end_drag()
            else:
                # finalize marquee → capture selection
                if not self.sel_rect_live.isEmpty():
                    self.sel.set_from_canvas(self.image, self.sel_rect_live)
            self.temp = QtGui.QImage()

        self.update()

    def keyPressEvent(self, ev: QtGui.QKeyEvent):
        mod = ev.modifiers()
        k = ev.key()
        # clipboard ops
        if mod == QtCore.Qt.ControlModifier and k == QtCore.Qt.Key_C:
            self.copy_selection()
            return
        if mod == QtCore.Qt.ControlModifier and k == QtCore.Qt.Key_X:
            self.cut_selection()
            return
        if mod == QtCore.Qt.ControlModifier and k == QtCore.Qt.Key_V:
            self.paste_selection()
            return
        # undo/redo
        if mod == QtCore.Qt.ControlModifier and k == QtCore.Qt.Key_Z:
            self.undo(); return
        if mod == QtCore.Qt.ControlModifier and k == QtCore.Qt.Key_Y:
            self.redo(); return
        # delete
        if k == QtCore.Qt.Key_Delete:
            self.delete_selection(); return
        # grid toggle
        if k == QtCore.Qt.Key_G:
            self.toggle_grid(); return
        # zoom
        if k in (QtCore.Qt.Key_Plus, QtCore.Qt.Key_Equal):
            self.set_scale(self.scale + 1); return
        if k == QtCore.Qt.Key_Minus:
            self.set_scale(self.scale - 1); return
        if k == QtCore.Qt.Key_1:
            self.set_scale(1); return

    # -------- clipboard/selection ops ----------
    def copy_selection(self):
        if not self.sel.active or self.sel.image.isNull():
            self.status_message.emit("Нет активного выделения.")
            return
        cb = QtWidgets.QApplication.clipboard()
        cb.setImage(self.sel.image)
        self.status_message.emit("Скопировано в буфер обмена.")

    def cut_selection(self):
        if not self.sel.active or self.sel.image.isNull():
            self.status_message.emit("Нет активного выделения для вырезания.")
            return
        self.push_undo()
        # copy to clipboard
        self.copy_selection()
        # clear area from canvas
        painter = QtGui.QPainter(self.image)
        painter.fillRect(self.sel.rect, self.bg)
        painter.end()
        self.sel.clear()
        self.update()
        self.status_message.emit("Вырезано.")

    def paste_selection(self):
        cb = QtWidgets.QApplication.clipboard()
        img = cb.image()
        if img.isNull():
            self.status_message.emit("В буфере нет изображения.")
            return
        # place as floating selection near (0,0)
        r = QtCore.QRect(0, 0, min(img.width(), self.image.width()),
                               min(img.height(), self.image.height()))
        self.sel.active = True
        self.sel.rect = r
        self.sel.image = img.copy(r)
        self.sel.dragging = False
        self.update()
        self.status_message.emit("Вставлено (перетащи и Enter/ЛКМ отпусти для фиксации).")

    def delete_selection(self):
        if not self.sel.active: return
        self.push_undo()
        painter = QtGui.QPainter(self.image)
        painter.fillRect(self.sel.rect, self.bg)
        painter.end()
        self.sel.clear()
        self.update()

    # -------- drawing primitives on self.image ----------
    def _dot(self, p: QtCore.QPoint, color: QtGui.QColor, w: int):
        painter = QtGui.QPainter(self.image)
        pen = QtGui.QPen(color); pen.setWidth(max(1,w))
        pen.setCapStyle(QtCore.Qt.RoundCap); pen.setJoinStyle(QtCore.Qt.RoundJoin)
        painter.setPen(pen); painter.drawPoint(p)
        painter.end()

    def _line(self, p0: QtCore.QPoint, p1: QtCore.QPoint, color: QtGui.QColor, w: int):
        painter = QtGui.QPainter(self.image)
        pen = QtGui.QPen(color); pen.setWidth(max(1,w))
        pen.setCapStyle(QtCore.Qt.RoundCap); pen.setJoinStyle(QtCore.Qt.RoundJoin)
        painter.setPen(pen); painter.drawLine(p0, p1)
        painter.end()

    def _rect(self, r: QtCore.QRect, color: QtGui.QColor, w: int, filled=False):
        painter = QtGui.QPainter(self.image)
        pen = QtGui.QPen(color); pen.setWidth(max(1,w))
        painter.setPen(pen)
        if filled:
            painter.fillRect(r, QtGui.QBrush(color))
        else:
            painter.setBrush(QtCore.Qt.NoBrush)
            painter.drawRect(r)
        painter.end()

    def _flood(self, sx:int, sy:int, color: QtGui.QColor):
        if sx<0 or sy<0 or sx>=self.image.width() or sy>=self.image.height(): return
        target = QtGui.QColor(self.image.pixel(sx, sy))
        if target == color: return
        target_rgba = target.rgba(); new_rgba = color.rgba()
        w, h = self.image.width(), self.image.height()
        stack = [(sx, sy)]
        while stack:
            x,y = stack.pop()
            if x<0 or y<0 or x>=w or y>=h: continue
            if QtGui.QColor(self.image.pixel(x,y)).rgba() != target_rgba: continue
            self.image.setPixel(x,y, new_rgba)
            stack.append((x+1,y)); stack.append((x-1,y))
            stack.append((x,y+1)); stack.append((x,y-1))

    # -------- paintEvent ----------
    def paintEvent(self, ev):
        painter = QtGui.QPainter(self)
        # base: show either temp or image, scaled by nearest-neighbor
        src = self.temp if not self.temp.isNull() else self.image
        painter.drawImage(self.rect(), src.scaled(self.width(), self.height(), transformMode=QtCore.Qt.FastTransformation))

        # grid
        if self.show_grid and self.scale >= 4:
            self._paint_grid(painter)

        # selection overlay
        self.sel.paint(painter, self.scale)

        painter.end()

    def _paint_grid(self, painter: QtGui.QPainter):
        pen = QtGui.QPen(QtGui.QColor(0,0,0,60))
        pen.setWidth(1)
        painter.setPen(pen)
        w, h = self.image.width(), self.image.height()
        step = max(1, self.grid_step)
        S = self.scale
        # vertical lines
        for x in range(0, w+1, step):
            xx = x*S
            painter.drawLine(xx, 0, xx, h*S)
        # horizontal lines
        for y in range(0, h+1, step):
            yy = y*S
            painter.drawLine(0, yy, w*S, yy)

    # -------- exports ----------
    def save_png(self, path: str):
        self.image.save(path, "PNG")

    def export_c_array(self, path: str, varname="bitmap", include_alpha=True):
        w,h = self.image.width(), self.image.height()
        fmt = "0x{A:02X}{R:02X}{G:02X}{B:02X}" if include_alpha else "0x{R:02X}{G:02X}{B:02X}"
        with open(path, "w", encoding="utf-8") as f:
            f.write("// Generated by ParrotPaint\n#include <stdint.h>\n\n")
            f.write(f"const uint32_t {varname}[{w*h}] = {{\n")
            for y in range(h):
                row=[]
                for x in range(w):
                    c = QtGui.QColor(self.image.pixel(x,y))
                    A,R,G,B = c.alpha(), c.red(), c.green(), c.blue()
                    row.append(fmt.format(A=A,R=R,G=G,B=B))
                f.write("  " + ", ".join(row) + (",\n" if y<h-1 else "\n"))
            f.write("};\n")
            f.write(f"const int WIDTH={w};\nconst int HEIGHT={h};\n")

    def export_svg(self, path: str):
        # strokes: (QColor,width,[(x,y)...])
        w,h = self.image.width(), self.image.height()
        with open(path, "w", encoding="utf-8") as f:
            f.write('<?xml version="1.0" encoding="utf-8"?>\n')
            f.write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">\n')
            f.write(f'<rect width="100%" height="100%" fill="rgba({self.bg.red()},{self.bg.green()},{self.bg.blue()},{self.bg.alpha()})"/>\n')
            for (col, wid, pts) in self.strokes:
                if not pts: continue
                f.write('<polyline fill="none" ')
                f.write(f'stroke="rgb({col.red()},{col.green()},{col.blue()})" stroke-width="{wid}" ')
                f.write('stroke-linecap="round" stroke-linejoin="round" points="')
                f.write(" ".join(f"{x},{y}" for (x,y) in pts))
                f.write('"/>\n')
            f.write("</svg>\n")

# -------------------------- Settings Dialog --------------------------
class SettingsDialog(QtWidgets.QDialog):
    """
    Canvas & view settings:
    - canvas width/height
    - background color
    - pixel scale
    - grid on/off & step
    """
    def __init__(self, parent, canvas: CanvasWidget):
        super().__init__(parent)
        self.setWindowTitle("Настройки — ParrotPaint")
        self.setModal(True)
        self.canvas = canvas

        form = QtWidgets.QFormLayout(self)

        self.w_spin = QtWidgets.QSpinBox(); self.w_spin.setRange(1, 4096); self.w_spin.setValue(canvas.image.width())
        self.h_spin = QtWidgets.QSpinBox(); self.h_spin.setRange(1, 4096); self.h_spin.setValue(canvas.image.height())
        self.scale_spin = QtWidgets.QSpinBox(); self.scale_spin.setRange(1, 128); self.scale_spin.setValue(canvas.scale)
        self.grid_check = QtWidgets.QCheckBox("Показывать сетку"); self.grid_check.setChecked(canvas.show_grid)
        self.grid_step_spin = QtWidgets.QSpinBox(); self.grid_step_spin.setRange(1, 128); self.grid_step_spin.setValue(canvas.grid_step)

        self.bg_btn = QtWidgets.QPushButton("Выбрать цвет…")
        self.bg_preview = QtWidgets.QLabel()
        self.bg_preview.setFixedSize(40,20); self.bg_preview.setAutoFillBackground(True)
        self._set_preview_color(self.bg_preview, canvas.bg)

        bg_box = QtWidgets.QHBoxLayout(); bg_box.addWidget(self.bg_btn); bg_box.addWidget(self.bg_preview)
        bg_wrap = QtWidgets.QWidget(); bg_wrap.setLayout(bg_box)

        form.addRow("Ширина полотна (px):", self.w_spin)
        form.addRow("Высота полотна (px):", self.h_spin)
        form.addRow("Масштаб пикселя:", self.scale_spin)
        form.addRow("", self.grid_check)
        form.addRow("Шаг сетки (лог. пиксели):", self.grid_step_spin)
        form.addRow("Фон:", bg_wrap)

        btns = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok|QtWidgets.QDialogButtonBox.Cancel)
        form.addRow(btns)

        self.bg_btn.clicked.connect(self.pick_bg)
        btns.accepted.connect(self.accept)
        btns.rejected.connect(self.reject)

    def _set_preview_color(self, label: QtWidgets.QLabel, c: QtGui.QColor):
        pal = label.palette()
        pal.setColor(QtGui.QPalette.Window, c)
        label.setPalette(pal)

    def pick_bg(self):
        c = QtWidgets.QColorDialog.getColor(self.canvas.bg, self, "Фон")
        if c.isValid():
            self._set_preview_color(self.bg_preview, c)

    def result_values(self):
        # Returns settings chosen in dialog.
        return {
            "w": self.w_spin.value(),
            "h": self.h_spin.value(),
            "scale": self.scale_spin.value(),
            "grid": self.grid_check.isChecked(),
            "grid_step": self.grid_step_spin.value(),
            "bg": self.bg_preview.palette().color(QtGui.QPalette.Window)
        }

# -------------------------- Main Window --------------------------
class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle(APP_TITLE)
        self._maybe_set_icon()
        self.canvas = CanvasWidget()
        self.setCentralWidget(self._wrap_scroll(self.canvas))
        self._build_ui()
        self.statusBar().showMessage("Готово")

        # shortcuts
        QtWidgets.QShortcut(QtGui.QKeySequence("Ctrl+Z"), self, activated=self.canvas.undo)
        QtWidgets.QShortcut(QtGui.QKeySequence("Ctrl+Y"), self, activated=self.canvas.redo)
        QtWidgets.QShortcut(QtGui.QKeySequence("Ctrl+C"), self, activated=self.canvas.copy_selection)
        QtWidgets.QShortcut(QtGui.QKeySequence("Ctrl+X"), self, activated=self.canvas.cut_selection)
        QtWidgets.QShortcut(QtGui.QKeySequence("Ctrl+V"), self, activated=self.canvas.paste_selection)

        self.canvas.status_message.connect(self.statusBar().showMessage)
        self.canvas.color_changed.connect(self._update_color_swatch)

    def _wrap_scroll(self, widget):
        scroll = QtWidgets.QScrollArea()
        scroll.setWidgetResizable(False)
        scroll.setWidget(widget)
        return scroll

    def _maybe_set_icon(self):
        for p in DEFAULT_ICON_CANDIDATES:
            if os.path.exists(p):
                self.setWindowIcon(QtGui.QIcon(p))
                break

    def _build_ui(self):
        tb = QtWidgets.QToolBar("Tools")
        self.addToolBar(tb)

        # --- tool actions ---
        def make_act(text, cb):
            a = QtWidgets.QAction(text, self)
            a.triggered.connect(cb)
            tb.addAction(a)
            return a

        make_act("Pen", lambda: self._set_tool(TOOL_PEN))
        make_act("Line", lambda: self._set_tool(TOOL_LINE))
        make_act("Rect", lambda: self._set_tool(TOOL_RECT))
        make_act("Fill", lambda: self._set_tool(TOOL_FILL))
        make_act("Eraser", lambda: self._set_tool(TOOL_ERASE))
        make_act("Picker", lambda: self._set_tool(TOOL_PICK))
        make_act("Select", lambda: self._set_tool(TOOL_SEL))

        tb.addSeparator()
        color_btn = QtWidgets.QPushButton("Color"); color_btn.clicked.connect(self._pick_color); tb.addWidget(color_btn)
        self.size_spin = QtWidgets.QSpinBox(); self.size_spin.setRange(1,128); self.size_spin.setValue(self.canvas.pen_w)
        self.size_spin.valueChanged.connect(lambda v: setattr(self.canvas, "pen_w", v))
        tb.addWidget(QtWidgets.QLabel("Pen")); tb.addWidget(self.size_spin)

        tb.addSeparator()
        save_png_btn = QtWidgets.QPushButton("Save PNG"); save_png_btn.clicked.connect(self._save_png); tb.addWidget(save_png_btn)
        save_svg_btn = QtWidgets.QPushButton("Save SVG"); save_svg_btn.clicked.connect(self._save_svg); tb.addWidget(save_svg_btn)
        export_c_btn = QtWidgets.QPushButton("Export C"); export_c_btn.clicked.connect(self._export_c); tb.addWidget(export_c_btn)

        tb.addSeparator()
        grid_btn = QtWidgets.QPushButton("Grid"); grid_btn.setCheckable(True); grid_btn.setChecked(self.canvas.show_grid)
        grid_btn.toggled.connect(self.canvas.toggle_grid); tb.addWidget(grid_btn)

        zoom_in = QtWidgets.QPushButton("+"); zoom_in.clicked.connect(lambda: self.canvas.set_scale(self.canvas.scale+1)); tb.addWidget(zoom_in)
        zoom_out = QtWidgets.QPushButton("−"); zoom_out.clicked.connect(lambda: self.canvas.set_scale(self.canvas.scale-1)); tb.addWidget(zoom_out)
        zoom_1 = QtWidgets.QPushButton("1:1"); zoom_1.clicked.connect(lambda: self.canvas.set_scale(1)); tb.addWidget(zoom_1)

        # Status widgets
        self.color_lbl = QtWidgets.QLabel(); self._update_color_swatch(self.canvas.fg)
        self.statusBar().addPermanentWidget(self.color_lbl)

        # --- menu ---
        m = self.menuBar()
        filem = m.addMenu("&Файл")
        act_new = QtWidgets.QAction("Новый", self); act_new.setShortcut("Ctrl+N"); act_new.triggered.connect(self._new); filem.addAction(act_new)
        act_open = QtWidgets.QAction("Открыть PNG…", self); act_open.setShortcut("Ctrl+O"); act_open.triggered.connect(self._open_png); filem.addAction(act_open)
        act_save = QtWidgets.QAction("Сохранить PNG…", self); act_save.setShortcut("Ctrl+S"); act_save.triggered.connect(self._save_png); filem.addAction(act_save)
        filem.addSeparator()
        act_exit = QtWidgets.QAction("Выход", self); act_exit.triggered.connect(self.close); filem.addAction(act_exit)

        editm = m.addMenu("&Правка")
        act_undo = QtWidgets.QAction("Отменить", self); act_undo.setShortcut("Ctrl+Z"); act_undo.triggered.connect(self.canvas.undo); editm.addAction(act_undo)
        act_redo = QtWidgets.QAction("Повторить", self); act_redo.setShortcut("Ctrl+Y"); act_redo.triggered.connect(self.canvas.redo); editm.addAction(act_redo)
        editm.addSeparator()
        act_copy = QtWidgets.QAction("Копировать", self); act_copy.setShortcut("Ctrl+C"); act_copy.triggered.connect(self.canvas.copy_selection); editm.addAction(act_copy)
        act_cut = QtWidgets.QAction("Вырезать", self); act_cut.setShortcut("Ctrl+X"); act_cut.triggered.connect(self.canvas.cut_selection); editm.addAction(act_cut)
        act_paste = QtWidgets.QAction("Вставить", self); act_paste.setShortcut("Ctrl+V"); act_paste.triggered.connect(self.canvas.paste_selection); editm.addAction(act_paste)
        act_del = QtWidgets.QAction("Удалить", self); act_del.setShortcut("Del"); act_del.triggered.connect(self.canvas.delete_selection); editm.addAction(act_del)

        viewm = m.addMenu("&Вид")
        act_grid = QtWidgets.QAction("Показывать сетку", self, checkable=True, checked=self.canvas.show_grid)
        act_grid.toggled.connect(self.canvas.toggle_grid); viewm.addAction(act_grid)
        act_zoom_in = QtWidgets.QAction("Увеличить", self); act_zoom_in.setShortcut("Ctrl++"); act_zoom_in.triggered.connect(lambda: self.canvas.set_scale(self.canvas.scale+1)); viewm.addAction(act_zoom_in)
        act_zoom_out = QtWidgets.QAction("Уменьшить", self); act_zoom_out.setShortcut("Ctrl+-"); act_zoom_out.triggered.connect(lambda: self.canvas.set_scale(self.canvas.scale-1)); viewm.addAction(act_zoom_out)
        act_zoom1 = QtWidgets.QAction("Масштаб 1:1", self); act_zoom1.setShortcut("Ctrl+1"); act_zoom1.triggered.connect(lambda: self.canvas.set_scale(1)); viewm.addAction(act_zoom1)

        toolsm = m.addMenu("&Инструменты")
        act_sel = QtWidgets.QAction("Режим выделения", self); act_sel.triggered.connect(lambda: self._set_tool(TOOL_SEL)); toolsm.addAction(act_sel)
        act_settings = QtWidgets.QAction("Настройки…", self); act_settings.triggered.connect(self._settings); toolsm.addAction(act_settings)
        act_icon = QtWidgets.QAction("Установить иконку окна…", self); act_icon.triggered.connect(self._choose_icon); toolsm.addAction(act_icon)

        exportm = m.addMenu("&Экспорт")
        act_svg = QtWidgets.QAction("Экспорт SVG…", self); act_svg.triggered.connect(self._save_svg); exportm.addAction(act_svg)
        act_c = QtWidgets.QAction("Экспорт C массива…", self); act_c.triggered.connect(self._export_c); exportm.addAction(act_c)

    # ---------- UI helpers ----------
    def _set_tool(self, t):
        self.canvas.tool = t
        tools = {
            TOOL_PEN: "Кисть", TOOL_LINE:"Линия", TOOL_RECT:"Прямоугольник",
            TOOL_FILL:"Заливка", TOOL_ERASE:"Ластик", TOOL_PICK:"Пипетка", TOOL_SEL:"Выделение"
        }
        self.statusBar().showMessage(f"Инструмент: {tools.get(t,'?')}")

    def _pick_color(self):
        c = QtWidgets.QColorDialog.getColor(self.canvas.fg, self, "Цвет кисти")
        if c.isValid():
            self.canvas.fg = c
            self._update_color_swatch(c)

    def _update_color_swatch(self, c: QtGui.QColor):
        pm = QtGui.QPixmap(24,24); pm.fill(c)
        self.color_lbl.setPixmap(pm)

    # ---------- file ops ----------
    def _new(self):
        if QtWidgets.QMessageBox.question(self, "Новый", "Очистить холст?") == QtWidgets.QMessageBox.Yes:
            self.canvas.push_undo()
            self.canvas.image.fill(self.canvas.bg)
            self.canvas.strokes.clear()
            self.canvas.sel.clear()
            self.canvas.update()

    def _open_png(self):
        fn, _ = QtWidgets.QFileDialog.getOpenFileName(self, "Открыть PNG", "", "PNG files (*.png)")
        if not fn: return
        img = QtGui.QImage(fn)
        if img.isNull():
            QtWidgets.QMessageBox.warning(self, "Ошибка", "Не удалось открыть PNG.")
            return
        self.canvas.push_undo()
        self.canvas.image = img.convertToFormat(QtGui.QImage.Format_ARGB32)
        self.canvas.setFixedSize(self.canvas.image.width()*self.canvas.scale, self.canvas.image.height()*self.canvas.scale)
        self.canvas.update()

    def _save_png(self):
        fn, _ = QtWidgets.QFileDialog.getSaveFileName(self, "Сохранить PNG", "image.png", "PNG files (*.png)")
        if not fn: return
        self.canvas.save_png(fn)
        self.statusBar().showMessage(f"PNG сохранён: {fn}")

    def _save_svg(self):
        fn, _ = QtWidgets.QFileDialog.getSaveFileName(self, "Экспорт SVG", "image.svg", "SVG files (*.svg)")
        if not fn: return
        self.canvas.export_svg(fn)
        self.statusBar().showMessage(f"SVG сохранён: {fn}")

    def _export_c(self):
        fn, _ = QtWidgets.QFileDialog.getSaveFileName(self, "Экспорт C массива", "bitmap.h", "C header (*.h)")
        if not fn: return
        inc_alpha = QtWidgets.QMessageBox.question(self, "Альфа-канал", "Включить альфа-канал в uint32_t?", QtWidgets.QMessageBox.Yes|QtWidgets.QMessageBox.No) == QtWidgets.QMessageBox.Yes
        self.canvas.export_c_array(fn, varname="bitmap", include_alpha=inc_alpha)
        self.statusBar().showMessage(f"C header сохранён: {fn}")

    # ---------- settings ----------
    def _settings(self):
        dlg = SettingsDialog(self, self.canvas)
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        v = dlg.result_values()
        # apply
        if v["w"] != self.canvas.image.width() or v["h"] != self.canvas.image.height():
            self.canvas.set_canvas_size(v["w"], v["h"], keep_content=True)
        self.canvas.bg = v["bg"]
        self.canvas.set_scale(v["scale"])
        self.canvas.toggle_grid(v["grid"])
        self.canvas.set_grid_step(v["grid_step"])
        self.canvas.update()

    def _choose_icon(self):
        fn, _ = QtWidgets.QFileDialog.getOpenFileName(self, "Иконка окна", "", "Icons (*.ico *.png *.svg)")
        if not fn: return
        self.setWindowIcon(QtGui.QIcon(fn))
        self.statusBar().showMessage(f"Иконка установлена: {fn}")

# -------------------------- main --------------------------
def main():
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("ParrotPaint")
    app.setOrganizationName("ParrotSoft")
    app.setApplicationDisplayName("ParrotSoft ParrotOS — ParrotPaint")
    # try set app icon too
    for p in DEFAULT_ICON_CANDIDATES:
        if os.path.exists(p):
            app.setWindowIcon(QtGui.QIcon(p))
            break

    win = MainWindow()
    win.resize( min(1024, win.canvas.width()+80), min(768, win.canvas.height()+120) )
    win.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()
