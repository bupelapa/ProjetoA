#include "GdiCanvas.h"

#include "Util.h"

namespace {

COLORREF to_colorref(Rgb color) {
    return RGB(color.r, color.g, color.b);
}

}

GdiCanvas::GdiCanvas(HDC dc)
    : dc_(dc)
{
    // padrao 8x8 monocromatico de pontos; em bitmaps monocromaticos o bit 0
    // usa a cor do texto (preto) e o bit 1 a cor de fundo (branco)
    static const WORD pattern[8] = {0x77, 0xFF, 0xDD, 0xFF, 0x77, 0xFF, 0xDD, 0xFF};

    dots_bitmap_ = CreateBitmap(8, 8, 1, 1, pattern);
    dots_brush_ = CreatePatternBrush(dots_bitmap_);

    SetBkMode(dc_, TRANSPARENT);
}

GdiCanvas::~GdiCanvas() {

    DeleteObject(dots_brush_);
    DeleteObject(dots_bitmap_);

    for (auto& entry : fonts_) {
        DeleteObject(entry.second);
    }
}

HFONT GdiCanvas::font(int size, bool bold) {

    int key = size * 2 + (bold ? 1 : 0);

    auto found = fonts_.find(key);

    if (found != fonts_.end()) {
        return found->second;
    }

    HFONT created = CreateFontW(
        -size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );

    fonts_[key] = created;

    return created;
}

void GdiCanvas::rect(int x, int y, int w, int h, Rgb fill, FillStyle style, Rgb stroke, int stroke_width) {

    RECT area = {x, y, x + w, y + h};

    switch (style) {

        case FillStyle::Solid: {
            HBRUSH brush = CreateSolidBrush(to_colorref(fill));
            FillRect(dc_, &area, brush);
            DeleteObject(brush);
            break;
        }

        case FillStyle::None:
            break;

        case FillStyle::Dots: {
            COLORREF old_text = SetTextColor(dc_, RGB(0, 0, 0));
            COLORREF old_back = SetBkColor(dc_, RGB(255, 255, 255));
            FillRect(dc_, &area, dots_brush_);
            SetTextColor(dc_, old_text);
            SetBkColor(dc_, old_back);
            break;
        }

        case FillStyle::Hatch: {
            HBRUSH brush = CreateHatchBrush(HS_BDIAGONAL, to_colorref(fill));
            int old_mode = SetBkMode(dc_, OPAQUE);
            COLORREF old_back = SetBkColor(dc_, RGB(235, 235, 235));
            FillRect(dc_, &area, brush);
            SetBkMode(dc_, old_mode);
            SetBkColor(dc_, old_back);
            DeleteObject(brush);
            break;
        }
    }

    if (stroke_width > 0) {

        HPEN pen = CreatePen(PS_SOLID, stroke_width, to_colorref(stroke));
        HGDIOBJ old_pen = SelectObject(dc_, pen);
        HGDIOBJ old_brush = SelectObject(dc_, GetStockObject(NULL_BRUSH));

        Rectangle(dc_, x, y, x + w + 1, y + h + 1);

        SelectObject(dc_, old_brush);
        SelectObject(dc_, old_pen);
        DeleteObject(pen);
    }
}

void GdiCanvas::line(int x1, int y1, int x2, int y2, Rgb color, int width, bool dashed) {

    // a gdi so desenha tracejado com espessura 1
    HPEN pen = CreatePen(dashed ? PS_DOT : PS_SOLID, dashed ? 1 : width, to_colorref(color));
    HGDIOBJ old_pen = SelectObject(dc_, pen);

    MoveToEx(dc_, x1, y1, nullptr);
    LineTo(dc_, x2, y2);

    SelectObject(dc_, old_pen);
    DeleteObject(pen);
}

void GdiCanvas::polygon(const std::vector<std::pair<int, int>>& points, Rgb fill, bool filled, Rgb stroke, int stroke_width) {

    std::vector<POINT> converted;

    for (const auto& point : points) {
        converted.push_back(POINT{point.first, point.second});
    }

    HPEN pen = stroke_width > 0 ? CreatePen(PS_SOLID, stroke_width, to_colorref(stroke)) : nullptr;
    HBRUSH brush = filled ? CreateSolidBrush(to_colorref(fill)) : nullptr;

    HGDIOBJ old_pen = SelectObject(dc_, pen ? static_cast<HGDIOBJ>(pen) : GetStockObject(NULL_PEN));
    HGDIOBJ old_brush = SelectObject(dc_, brush ? static_cast<HGDIOBJ>(brush) : GetStockObject(NULL_BRUSH));

    Polygon(dc_, converted.data(), static_cast<int>(converted.size()));

    SelectObject(dc_, old_brush);
    SelectObject(dc_, old_pen);

    if (pen) {
        DeleteObject(pen);
    }

    if (brush) {
        DeleteObject(brush);
    }
}

void GdiCanvas::circle(int cx, int cy, int radius, Rgb fill, bool filled, Rgb stroke, int stroke_width) {

    HPEN pen = stroke_width > 0 ? CreatePen(PS_SOLID, stroke_width, to_colorref(stroke)) : nullptr;
    HBRUSH brush = filled ? CreateSolidBrush(to_colorref(fill)) : nullptr;

    HGDIOBJ old_pen = SelectObject(dc_, pen ? static_cast<HGDIOBJ>(pen) : GetStockObject(NULL_PEN));
    HGDIOBJ old_brush = SelectObject(dc_, brush ? static_cast<HGDIOBJ>(brush) : GetStockObject(NULL_BRUSH));

    Ellipse(dc_, cx - radius, cy - radius, cx + radius + 1, cy + radius + 1);

    SelectObject(dc_, old_brush);
    SelectObject(dc_, old_pen);

    if (pen) {
        DeleteObject(pen);
    }

    if (brush) {
        DeleteObject(brush);
    }
}

void GdiCanvas::text(int x, int y, const std::string& text, Rgb color, int size, TextAlign align, bool bold) {

    std::wstring wide = utf8_to_wide(text);

    HGDIOBJ old_font = SelectObject(dc_, font(size, bold));

    SIZE extent = {0, 0};

    GetTextExtentPoint32W(dc_, wide.c_str(), static_cast<int>(wide.size()), &extent);

    int left = x;

    if (align == TextAlign::Center) {
        left = x - extent.cx / 2;
    }
    else if (align == TextAlign::Right) {
        left = x - extent.cx;
    }

    SetTextColor(dc_, to_colorref(color));
    SetTextAlign(dc_, TA_LEFT | TA_TOP);

    TextOutW(dc_, left, y - extent.cy / 2, wide.c_str(), static_cast<int>(wide.size()));

    SelectObject(dc_, old_font);
}
