#ifndef SVG_CANVAS_H
#define SVG_CANVAS_H

#include "Canvas.h"

#include <sstream>
#include <string>

// implementacao de Canvas que gera um documento svg
//
// svg foi escolhido como formato da imagem final porque e texto puro (nao
// precisa de nenhuma biblioteca de compressao como png/jpg exigiriam), abre
// em qualquer navegador e nao tem limite pratico de largura, o que permite
// representar simulacoes de qualquer duracao (requisito 2.4)
class SvgCanvas : public Canvas {

public:

    SvgCanvas(int width, int height);

    void rect(int x, int y, int w, int h, Rgb fill, FillStyle style, Rgb stroke, int stroke_width) override;

    void line(int x1, int y1, int x2, int y2, Rgb color, int width, bool dashed) override;

    void polygon(const std::vector<std::pair<int, int>>& points, Rgb fill, bool filled, Rgb stroke, int stroke_width) override;

    void circle(int cx, int cy, int radius, Rgb fill, bool filled, Rgb stroke, int stroke_width) override;

    void text(int x, int y, const std::string& text, Rgb color, int size, TextAlign align, bool bold) override;

    // documento completo
    std::string document() const;

private:

    int width_;
    int height_;
    std::ostringstream body_;
};

#endif
