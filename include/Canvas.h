#ifndef CANVAS_H
#define CANVAS_H

#include <string>
#include <utility>
#include <vector>

// cor rgb simples, independente de qualquer biblioteca grafica
struct Rgb {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

// tipos de preenchimento de retangulos usados no gantt
enum class FillStyle {
    Solid,  // tarefa executando
    None,   // tarefa pronta (ausencia de cor)
    Dots,   // tarefa suspensa (preto pontilhado)
    Hatch   // cpu desligada (listras cinza)
};

enum class TextAlign { Left, Center, Right };

// superficie de desenho abstrata
//
// decisao de projeto: o grafico de gantt e desenhado uma unica vez, em
// GanttRenderer, sobre esta interface. existem duas implementacoes:
//   - GdiCanvas: desenha na janela (visualizacao passo a passo)
//   - SvgCanvas: gera o arquivo de imagem ao final da simulacao
// assim a imagem exportada e identica ao que aparece na tela, sem ser
// um "print" da janela (requisito 2.4)
class Canvas {

public:

    virtual ~Canvas() = default;

    // stroke_width = 0 desenha o retangulo sem borda
    virtual void rect(int x, int y, int w, int h, Rgb fill, FillStyle style, Rgb stroke, int stroke_width) = 0;

    virtual void line(int x1, int y1, int x2, int y2, Rgb color, int width, bool dashed) = 0;

    virtual void polygon(const std::vector<std::pair<int, int>>& points, Rgb fill, bool filled, Rgb stroke, int stroke_width) = 0;

    virtual void circle(int cx, int cy, int radius, Rgb fill, bool filled, Rgb stroke, int stroke_width) = 0;

    // y indica o centro vertical do texto; o texto e utf-8
    virtual void text(int x, int y, const std::string& text, Rgb color, int size, TextAlign align, bool bold) = 0;
};

#endif
