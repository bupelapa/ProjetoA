#ifndef GANTT_RENDERER_H
#define GANTT_RENDERER_H

#include "Canvas.h"
#include "Simulator.h"

#include <map>
#include <string>
#include <vector>

// desenha o grafico de gantt da simulacao sobre qualquer Canvas
//
// layout (de cima para baixo):
//   cabecalho com os parametros do sistema
//   uma linha por tarefa, com o MAIOR id no topo e o MENOR id junto ao
//   eixo x (requisito 2.5, no estilo das figuras do livro do prof. maziero)
//   eixo do tempo
//   uma faixa por processador mostrando qual tarefa executou ou se a cpu
//   estava desligada (requisitos 1.2 e 2.1)
//   legenda de todos os elementos graficos (requisito geral 3)
class GanttRenderer {

public:

    // cell_width: largura de um tick em pixels (zoom)
    // scale: fator de escala para telas com dpi alto (1.0 = 96 dpi)
    GanttRenderer(const Simulator& simulator, int cell_width, double scale);

    int content_width() const;

    // a altura depende da largura disponivel, pois a legenda quebra em linhas
    int content_height(int available_width) const;

    // desenha a regiao visivel; para exportar, scroll = 0 e view = conteudo inteiro
    void draw(Canvas& canvas, int scroll_x, int scroll_y, int view_width, int view_height) const;

    // texto descritivo do ponto sob o mouse (usado como "depurador" visual)
    std::string describe_point(int x, int y, int scroll_x, int scroll_y) const;

    // posicao x (no conteudo) do instante informado
    int time_to_x(int time) const;

private:

    int px(int value) const;

    int rows_top() const;
    int axis_y() const;
    int cpu_top() const;
    int legend_top() const;
    int legend_columns(int available_width) const;
    int legend_rows(int available_width) const;

    Rgb task_color(int id) const;

    void draw_glyph(Canvas& canvas, EventType type, int x, int row_top, int cpu) const;
    void draw_legend(Canvas& canvas, int top, int width) const;

    const Simulator& simulator_;
    int cell_width_;
    double scale_;

    // ids das tarefas na ordem de desenho (de cima para baixo)
    std::vector<int> rows_;
    std::map<int, int> row_of_task_;
    std::map<int, Rgb> colors_;

    int cpu_rows_;
    int end_time_;
    int now_;
    std::size_t visible_events_;
};

#endif
