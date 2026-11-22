#include "layout_manager.h"

LayoutManager::LayoutManager(int screenW, int screenH) {
    _w = screenW;
    _h = screenH;
}

Rect LayoutManager::grid(int cols, int rows, int col, int row, int colSpan, int rowSpan) {
    Rect r;
    r.w = _w / cols * colSpan;
    r.h = _h / rows * rowSpan;
    r.x = _w / cols * col;
    r.y = _h / rows * row;
    return r;
}

Rect LayoutManager::inset(Rect r, int padding) {
    r.x += padding;
    r.y += padding;
    r.w -= padding * 2;
    r.h -= padding * 2;
    return r;
}

Rect LayoutManager::safeArea(int padding) {
    return inset({0, 0, _w, _h}, padding);
}
