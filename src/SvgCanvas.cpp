#include "SvgCanvas.h"

#include <cstdio>

namespace {

std::string color(Rgb value) {

    char buffer[8];

    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", value.r, value.g, value.b);

    return buffer;
}

// caracteres especiais do xml precisam ser escapados dentro do texto
std::string escape(const std::string& text) {

    std::string result;

    for (char character : text) {
        switch (character) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            default: result += character; break;
        }
    }

    return result;
}

std::string stroke_attributes(Rgb stroke, int stroke_width) {

    if (stroke_width <= 0) {
        return " stroke=\"none\"";
    }

    return " stroke=\"" + color(stroke) + "\" stroke-width=\"" + std::to_string(stroke_width) + "\"";
}

}

SvgCanvas::SvgCanvas(int width, int height)
    : width_(width), height_(height)
{
}

void SvgCanvas::rect(int x, int y, int w, int h, Rgb fill, FillStyle style, Rgb stroke, int stroke_width) {

    std::string fill_value;

    switch (style) {
        case FillStyle::Solid: fill_value = color(fill); break;
        case FillStyle::None:  fill_value = "none"; break;
        case FillStyle::Dots:  fill_value = "url(#dots)"; break;
        case FillStyle::Hatch: fill_value = "url(#hatch)"; break;
    }

    body_ << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h
          << "\" fill=\"" << fill_value << "\"" << stroke_attributes(stroke, stroke_width) << "/>\n";
}

void SvgCanvas::line(int x1, int y1, int x2, int y2, Rgb value, int width, bool dashed) {

    body_ << "<line x1=\"" << x1 << "\" y1=\"" << y1 << "\" x2=\"" << x2 << "\" y2=\"" << y2
          << "\" stroke=\"" << color(value) << "\" stroke-width=\"" << width << "\""
          << (dashed ? " stroke-dasharray=\"3,3\"" : "") << "/>\n";
}

void SvgCanvas::polygon(const std::vector<std::pair<int, int>>& points, Rgb fill, bool filled, Rgb stroke, int stroke_width) {

    body_ << "<polygon points=\"";

    for (const auto& point : points) {
        body_ << point.first << "," << point.second << " ";
    }

    body_ << "\" fill=\"" << (filled ? color(fill) : "none") << "\"" << stroke_attributes(stroke, stroke_width) << "/>\n";
}

void SvgCanvas::circle(int cx, int cy, int radius, Rgb fill, bool filled, Rgb stroke, int stroke_width) {

    body_ << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << radius
          << "\" fill=\"" << (filled ? color(fill) : "none") << "\"" << stroke_attributes(stroke, stroke_width) << "/>\n";
}

void SvgCanvas::text(int x, int y, const std::string& text, Rgb value, int size, TextAlign align, bool bold) {

    const char* anchor = align == TextAlign::Left ? "start" : (align == TextAlign::Center ? "middle" : "end");

    body_ << "<text x=\"" << x << "\" y=\"" << y << "\" font-size=\"" << size
          << "\" text-anchor=\"" << anchor << "\" dominant-baseline=\"central\" fill=\"" << color(value) << "\""
          << (bold ? " font-weight=\"bold\"" : "") << ">" << escape(text) << "</text>\n";
}

std::string SvgCanvas::document() const {

    std::ostringstream out;

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width_ << "\" height=\"" << height_
        << "\" viewBox=\"0 0 " << width_ << " " << height_ << "\" font-family=\"Segoe UI, Arial, sans-serif\">\n"
        << "<defs>\n"
        // preenchimento pontilhado preto: tarefa suspensa (requisito 2.1)
        << "<pattern id=\"dots\" width=\"4\" height=\"4\" patternUnits=\"userSpaceOnUse\">"
        << "<rect width=\"4\" height=\"4\" fill=\"#FFFFFF\"/><circle cx=\"1\" cy=\"1\" r=\"1\" fill=\"#000000\"/>"
        << "<circle cx=\"3\" cy=\"3\" r=\"1\" fill=\"#000000\"/></pattern>\n"
        // listras cinza: processador desligado
        << "<pattern id=\"hatch\" width=\"6\" height=\"6\" patternUnits=\"userSpaceOnUse\" patternTransform=\"rotate(45)\">"
        << "<rect width=\"6\" height=\"6\" fill=\"#EBEBEB\"/><line x1=\"0\" y1=\"0\" x2=\"0\" y2=\"6\" stroke=\"#969696\" stroke-width=\"2\"/></pattern>\n"
        << "</defs>\n"
        << "<rect width=\"100%\" height=\"100%\" fill=\"#FFFFFF\"/>\n"
        << body_.str()
        << "</svg>\n";

    return out.str();
}
