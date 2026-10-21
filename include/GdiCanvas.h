#ifndef GDI_CANVAS_H
#define GDI_CANVAS_H

#include "Canvas.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <map>

// implementacao de Canvas que desenha com a gdi do windows
//
// a gdi faz parte do proprio windows, por isso o simulador nao depende
// de nenhuma biblioteca grafica externa (requisito geral 5)
class GdiCanvas : public Canvas {

public:

    explicit GdiCanvas(HDC dc);

    ~GdiCanvas() override;

    void rect(int x, int y, int w, int h, Rgb fill, FillStyle style, Rgb stroke, int stroke_width) override;

    void line(int x1, int y1, int x2, int y2, Rgb color, int width, bool dashed) override;

    void polygon(const std::vector<std::pair<int, int>>& points, Rgb fill, bool filled, Rgb stroke, int stroke_width) override;

    void circle(int cx, int cy, int radius, Rgb fill, bool filled, Rgb stroke, int stroke_width) override;

    void text(int x, int y, const std::string& text, Rgb color, int size, TextAlign align, bool bold) override;

private:

    HFONT font(int size, bool bold);

    HDC dc_;

    HBITMAP dots_bitmap_;
    HBRUSH dots_brush_;

    // as fontes sao criadas uma vez por tamanho e reaproveitadas,
    // pois criar fontes a cada texto deixaria o desenho lento
    std::map<int, HFONT> fonts_;
};

#endif
