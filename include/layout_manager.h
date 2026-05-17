#pragma once

struct Rect {
    int x;
    int y;
    int w;
    int h;
};

class LayoutManager {
   public:
    LayoutManager(int screenW, int screenH);

    Rect grid(int cols, int rows, int col, int row, int colSpan = 1, int rowSpan = 1);

    Rect inset(Rect r, int padding);

    Rect safeArea(int padding);

   private:
    int _w;
    int _h;
};