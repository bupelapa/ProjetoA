#include "GanttRenderer.h"

#include "Scheduler.h"
#include "Util.h"

#include <algorithm>

namespace {

// medidas do layout em pixels para 96 dpi (multiplicadas pela escala)
const int HEADER_H = 32;
const int LABEL_W = 84;

// espaco entre o eixo y e o instante 0, para que as setas de chegada em
// t = 0 nao fiquem escondidas atras do eixo
const int TIME_PAD = 14;
const int ROW_H = 50;
const int BAR_TOP = 12;
const int BAR_H = 18;
const int AXIS_H = 30;
const int CPU_TITLE_H = 20;
const int CPU_ROW_H = 24;
const int LEGEND_GAP = 18;
const int LEGEND_TITLE_H = 22;
const int LEGEND_ITEM_W = 390;
const int LEGEND_ITEM_H = 50;

const Rgb WHITE = {255, 255, 255};
const Rgb BLACK = {0, 0, 0};
const Rgb GRID = {225, 225, 225};
const Rgb AXIS = {60, 60, 60};
const Rgb READY_BORDER = {150, 150, 150};
const Rgb ARRIVAL = {20, 90, 200};
const Rgb ACTIVATION = {70, 80, 95};
const Rgb DEADLINE = {200, 30, 30};
const Rgb PREEMPTION = {245, 130, 0};
const Rgb QUANTUM = {130, 50, 180};
const Rgb RANDOM = {230, 180, 0};
const Rgb MANUAL = {220, 0, 160};
const Rgb NOW_LINE = {220, 0, 0};
const Rgb CPU_OFF = {120, 120, 120};

// codigos dos itens da legenda que nao sao eventos
const int LEGEND_RUNNING = -1;
const int LEGEND_READY = -2;
const int LEGEND_SUSPENDED = -3;
const int LEGEND_CPU_OFF = -4;
const int LEGEND_NOW = -5;

struct LegendItem {
    int code;  // valor de EventType ou um dos codigos acima
    const char* text;
};

const LegendItem LEGEND[] = {
    {LEGEND_RUNNING, "Executando (cor da tarefa; rótulo = CPU usada)"},
    {LEGEND_READY, "Pronta na fila, sem executar (ausência de cor)"},
    {LEGEND_SUSPENDED, "Suspensa (preto pontilhado)"},
    {LEGEND_CPU_OFF, "CPU desligada (faixa das CPUs)"},
    {static_cast<int>(EventType::Arrival), "Chegada da tarefa (ingresso)"},
    {static_cast<int>(EventType::Activation), "Nova ativação periódica"},
    {static_cast<int>(EventType::ActivationQueued), "Ativação enfileirada (anterior não terminou)"},
    {static_cast<int>(EventType::Deadline), "Prazo (deadline) da ativação"},
    {static_cast<int>(EventType::DeadlineMiss), "Perda de prazo"},
    {static_cast<int>(EventType::JobEnd), "Fim de uma ativação"},
    {static_cast<int>(EventType::TaskEnd), "Término da tarefa"},
    {static_cast<int>(EventType::Preemption), "Preempção (perdeu a CPU)"},
    {static_cast<int>(EventType::QuantumExpired), "Quantum esgotado"},
    {static_cast<int>(EventType::RandomTie), "Escolhida por sorteio (empate)"},
    {static_cast<int>(EventType::Manual), "Alteração manual do usuário"},
    {LEGEND_NOW, "Instante atual da simulação"},
};

const int LEGEND_COUNT = static_cast<int>(sizeof(LEGEND) / sizeof(LEGEND[0]));

// escolhe preto ou branco para o texto conforme a luminosidade do fundo
Rgb contrast(Rgb background) {

    int luminance = (background.r * 299 + background.g * 587 + background.b * 114) / 1000;

    return luminance > 140 ? BLACK : WHITE;
}

}

GanttRenderer::GanttRenderer(const Simulator& simulator, int cell_width, double scale)
    : simulator_(simulator),
      cell_width_(std::max(2, cell_width)),
      scale_(scale),
      cpu_rows_(1),
      end_time_(0),
      now_(0),
      visible_events_(0)
{
    const SimState& state = simulator.state();

    // menor id junto ao eixo x: como o desenho e de cima para baixo, as
    // linhas sao montadas em ordem decrescente de id (requisito 2.5)
    for (const Task& task : state.tasks) {

        if (task.state == TaskState::Ignored) {
            continue;
        }

        rows_.push_back(task.id);

        Rgb color;

        color_to_rgb(task.color, color.r, color.g, color.b);

        colors_[task.id] = color;
    }

    std::sort(rows_.begin(), rows_.end(), [](int a, int b) {
        return a > b;
    });

    for (std::size_t index = 0; index < rows_.size(); ++index) {
        row_of_task_[rows_[index]] = static_cast<int>(index);
    }

    cpu_rows_ = simulator.max_cpu_count();
    now_ = simulator.position();
    visible_events_ = simulator.visible_event_count();

    // o grafico vai ate o instante atual ou ate o ultimo prazo ja conhecido
    end_time_ = now_;

    const auto& events = simulator.all_events();

    for (std::size_t index = 0; index < visible_events_; ++index) {
        end_time_ = std::max(end_time_, events[index].time);
    }
}

int GanttRenderer::px(int value) const {
    return static_cast<int>(value * scale_ + 0.5);
}

int GanttRenderer::rows_top() const {
    return px(HEADER_H);
}

int GanttRenderer::axis_y() const {
    return rows_top() + static_cast<int>(rows_.size()) * px(ROW_H);
}

int GanttRenderer::cpu_top() const {
    return axis_y() + px(AXIS_H) + px(CPU_TITLE_H);
}

int GanttRenderer::legend_top() const {
    return cpu_top() + cpu_rows_ * px(CPU_ROW_H) + px(LEGEND_GAP);
}

int GanttRenderer::legend_columns(int available_width) const {
    return std::max(1, (available_width - px(20)) / px(LEGEND_ITEM_W));
}

int GanttRenderer::legend_rows(int available_width) const {
    int columns = legend_columns(available_width);
    return (LEGEND_COUNT + columns - 1) / columns;
}

int GanttRenderer::time_to_x(int time) const {
    return px(LABEL_W) + px(TIME_PAD) + time * cell_width_;
}

int GanttRenderer::content_width() const {
    return time_to_x(end_time_ + 2) + px(30);
}

int GanttRenderer::content_height(int available_width) const {
    return legend_top() + px(LEGEND_TITLE_H) + legend_rows(available_width) * px(LEGEND_ITEM_H) + px(10);
}

Rgb GanttRenderer::task_color(int id) const {

    auto found = colors_.find(id);

    return found != colors_.end() ? found->second : Rgb{128, 128, 128};
}

// desenha o simbolo de um evento; row_top e o topo da linha da tarefa
// e x e a posicao exata do instante em que o evento acontece (requisito 2.2)
void GanttRenderer::draw_glyph(Canvas& canvas, EventType type, int x, int row_top, int cpu) const {

    auto y = [&](int offset) {
        return row_top + px(offset);
    };

    switch (type) {

        case EventType::Arrival: {

            // seta grossa azul para cima, atravessando a barra
            canvas.line(x, y(47), x, y(16), ARRIVAL, px(3), false);
            canvas.polygon({{x - px(6), y(20)}, {x + px(6), y(20)}, {x, y(10)}}, ARRIVAL, true, ARRIVAL, 1);
            break;
        }

        case EventType::Activation: {

            canvas.line(x, y(47), x, y(17), ACTIVATION, px(1), false);
            canvas.polygon({{x - px(4), y(20)}, {x + px(4), y(20)}, {x, y(13)}}, ACTIVATION, true, ACTIVATION, 1);
            break;
        }

        case EventType::ActivationQueued: {

            canvas.line(x, y(47), x, y(17), ACTIVATION, 1, true);
            canvas.polygon({{x - px(4), y(20)}, {x + px(4), y(20)}, {x, y(13)}}, WHITE, true, ACTIVATION, 1);
            break;
        }

        case EventType::Deadline: {

            // triangulo vazado vermelho apontando para baixo, acima da barra
            canvas.polygon({{x - px(5), y(1)}, {x + px(5), y(1)}, {x, y(10)}}, WHITE, true, DEADLINE, px(2));
            break;
        }

        case EventType::DeadlineMiss: {

            // x vermelho sobre a barra
            int size = px(7);
            int center = y(BAR_TOP + BAR_H / 2);

            canvas.line(x - size, center - size, x + size, center + size, DEADLINE, px(3), false);
            canvas.line(x - size, center + size, x + size, center - size, DEADLINE, px(3), false);
            break;
        }

        case EventType::JobEnd: {

            // traco preto vertical marcando o fim da ativacao
            canvas.line(x, y(BAR_TOP - 3), x, y(BAR_TOP + BAR_H + 3), BLACK, px(3), false);
            break;
        }

        case EventType::TaskEnd: {

            // seta grossa preta para baixo terminando em uma base
            canvas.line(x, y(0), x, y(24), BLACK, px(3), false);
            canvas.polygon({{x - px(7), y(22)}, {x + px(7), y(22)}, {x, y(33)}}, BLACK, true, BLACK, 1);
            canvas.line(x - px(8), y(34), x + px(8), y(34), BLACK, px(3), false);
            break;
        }

        case EventType::Preemption: {

            int center = y(39);
            int size = px(5);

            canvas.polygon({{x, center - size}, {x + size, center}, {x, center + size}, {x - size, center}}, PREEMPTION, true, BLACK, 1);
            break;
        }

        case EventType::QuantumExpired: {
            canvas.circle(x, y(45), px(4), WHITE, true, QUANTUM, px(2));
            break;
        }

        case EventType::RandomTie: {
            canvas.circle(x, y(39), px(7), RANDOM, true, BLACK, 1);
            canvas.text(x, y(39), "?", BLACK, px(11), TextAlign::Center, true);
            break;
        }

        case EventType::Manual: {
            canvas.rect(x - px(4), y(41), px(8), px(8), MANUAL, FillStyle::Solid, BLACK, 1);
            break;
        }

        case EventType::CpuOff:
        case EventType::CpuOn:
            // ligar/desligar aparece pelas listras na faixa da cpu
            (void)cpu;
            break;
    }
}

void GanttRenderer::draw(Canvas& canvas, int scroll_x, int scroll_y, int view_width, int view_height) const {

    const SimState& state = simulator_.state();

    const int label_w = px(LABEL_W);

    auto x_of = [&](int time) {
        return time_to_x(time) - scroll_x;
    };

    auto row_y = [&](int row) {
        return rows_top() + row * px(ROW_H) - scroll_y;
    };

    auto cpu_y = [&](int cpu) {
        return cpu_top() + cpu * px(CPU_ROW_H) - scroll_y;
    };

    const int axis = axis_y() - scroll_y;
    const int cpu_bottom = cpu_y(cpu_rows_);

    // somente os ticks visiveis sao desenhados, o que mantem o desenho
    // rapido mesmo em simulacoes muito longas
    int first_tick = std::max(0, scroll_x / cell_width_ - 1);
    int last_tick = (scroll_x + view_width) / cell_width_ + 1;

    // ---------------------------------------------------------------
    // grade e linhas de base
    // ---------------------------------------------------------------
    int grid_step = 1;

    while (grid_step * cell_width_ < px(8)) {
        grid_step *= 2;
    }

    for (int time = first_tick - first_tick % grid_step; time <= std::min(last_tick, end_time_ + 2); time += grid_step) {

        int x = x_of(time);

        if (x < label_w) {
            continue;
        }

        canvas.line(x, rows_top() - scroll_y, x, axis, GRID, 1, false);
        canvas.line(x, cpu_y(0), x, cpu_bottom, GRID, 1, false);
    }

    for (std::size_t row = 0; row < rows_.size(); ++row) {

        int base = row_y(static_cast<int>(row)) + px(BAR_TOP + BAR_H);

        canvas.line(label_w, base, view_width, base, GRID, 1, false);
    }

    // ---------------------------------------------------------------
    // barras de execucao das tarefas e faixas das cpus
    // ---------------------------------------------------------------
    const auto& ticks = simulator_.ticks();

    int tick_end = std::min(std::min(last_tick, now_ - 1), static_cast<int>(ticks.size()) - 1);

    for (int time = first_tick; time <= tick_end; ++time) {

        const TickRecord& record = ticks[time];

        int x = x_of(time);

        for (const TickCell& cell : record.cells) {

            auto found = row_of_task_.find(cell.task_id);

            if (found == row_of_task_.end()) {
                continue;
            }

            int top = row_y(found->second) + px(BAR_TOP);

            if (cell.state == TaskState::Running) {

                Rgb color = task_color(cell.task_id);

                canvas.rect(x, top, cell_width_, px(BAR_H), color, FillStyle::Solid, AXIS, 1);

                // rotulo com a cpu usada neste tick (requisito 2.1)
                if (cell_width_ >= px(20)) {
                    canvas.text(x + cell_width_ / 2, top + px(BAR_H) / 2, std::to_string(cell.cpu), contrast(color), px(10), TextAlign::Center, true);
                }
            }
            else if (cell.state == TaskState::Ready) {
                canvas.rect(x, top + px(4), cell_width_, px(BAR_H - 8), WHITE, FillStyle::None, READY_BORDER, 1);
            }
            else if (cell.state == TaskState::Suspended) {
                canvas.rect(x, top, cell_width_, px(BAR_H), BLACK, FillStyle::Dots, BLACK, 1);
            }
        }

        for (int cpu = 0; cpu < static_cast<int>(record.cpu_task.size()); ++cpu) {

            int top = cpu_y(cpu) + px(3);
            int height = px(CPU_ROW_H - 6);

            int id = record.cpu_task[cpu];

            if (id < 0) {
                canvas.rect(x, top, cell_width_, height, CPU_OFF, FillStyle::Hatch, CPU_OFF, 0);
                continue;
            }

            Rgb color = task_color(id);

            canvas.rect(x, top, cell_width_, height, color, FillStyle::Solid, AXIS, 1);

            if (cell_width_ >= px(22)) {
                canvas.text(x + cell_width_ / 2, top + height / 2, "T" + std::to_string(id), contrast(color), px(10), TextAlign::Center, false);
            }
        }
    }

    // ---------------------------------------------------------------
    // simbolos dos eventos
    // ---------------------------------------------------------------
    const auto& events = simulator_.all_events();

    for (std::size_t index = 0; index < visible_events_; ++index) {

        const SimEvent& event = events[index];

        if (event.time < first_tick - 1 || event.time > last_tick + 1) {
            continue;
        }

        int x = x_of(event.time);

        if (x < label_w - px(2)) {
            continue;
        }

        if (event.task_id < 0) {

            // alteracoes do sistema (quantum, cpus) aparecem sobre o eixo
            if (event.type == EventType::Manual) {
                draw_glyph(canvas, event.type, x, axis - px(45), event.cpu);
            }

            continue;
        }

        auto found = row_of_task_.find(event.task_id);

        if (found != row_of_task_.end()) {
            draw_glyph(canvas, event.type, x, row_y(found->second), event.cpu);
        }
    }

    // ---------------------------------------------------------------
    // eixo do tempo e instante atual
    // ---------------------------------------------------------------
    canvas.line(label_w, axis, std::max(label_w, x_of(end_time_ + 2)), axis, AXIS, px(2), false);

    int label_step = 1;

    const int steps[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000};

    for (int step : steps) {
        label_step = step;
        if (step * cell_width_ >= px(28)) {
            break;
        }
    }

    for (int time = first_tick - first_tick % label_step; time <= std::min(last_tick, end_time_ + 2); time += label_step) {

        int x = x_of(time);

        if (x < label_w) {
            continue;
        }

        canvas.line(x, axis, x, axis + px(5), AXIS, 1, false);
        canvas.text(x, axis + px(14), std::to_string(time), AXIS, px(11), TextAlign::Center, false);
    }

    canvas.text(x_of(end_time_ + 2) + px(4), axis, "t", AXIS, px(12), TextAlign::Left, true);

    int now_x = x_of(now_);

    if (now_x >= label_w) {
        canvas.line(now_x, rows_top() - scroll_y, now_x, cpu_bottom, NOW_LINE, 1, true);
    }

    canvas.text(label_w + px(4), axis + px(AXIS_H) + px(CPU_TITLE_H) / 2 - px(4), "Processadores (CPU usada em cada tick):", AXIS, px(11), TextAlign::Left, true);

    // ---------------------------------------------------------------
    // coluna de rotulos fixa (nao rola na horizontal) e cabecalho
    // ---------------------------------------------------------------
    // a faixa dos numeros do eixo fica de fora da mascara para que o
    // rotulo do instante 0 (exatamente na borda) nao seja cortado
    canvas.rect(0, 0, label_w, axis + px(2), WHITE, FillStyle::Solid, WHITE, 0);
    canvas.rect(0, axis + px(AXIS_H), label_w, cpu_bottom - axis - px(AXIS_H) + px(4), WHITE, FillStyle::Solid, WHITE, 0);

    for (std::size_t row = 0; row < rows_.size(); ++row) {

        int id = rows_[row];

        int center = row_y(static_cast<int>(row)) + px(BAR_TOP + BAR_H / 2);

        canvas.rect(px(8), center - px(6), px(12), px(12), task_color(id), FillStyle::Solid, AXIS, 1);
        canvas.text(px(26), center, "T" + std::to_string(id), BLACK, px(13), TextAlign::Left, true);
    }

    for (int cpu = 0; cpu < cpu_rows_; ++cpu) {
        canvas.text(px(8), cpu_y(cpu) + px(CPU_ROW_H) / 2, "CPU " + std::to_string(cpu), AXIS, px(12), TextAlign::Left, false);
    }

    canvas.line(label_w, rows_top() - scroll_y, label_w, axis, AXIS, px(2), false);

    canvas.rect(0, 0, std::max(view_width, content_width()), rows_top() - scroll_y, WHITE, FillStyle::Solid, WHITE, 0);

    const Scheduler* scheduler = SchedulerRegistry::instance().find(state.algorithm);

    std::string header =
        "Algoritmo: " + state.algorithm + (scheduler ? " - " + scheduler->description() : "") +
        "   |   Quantum: " + (state.quantum > 0 ? std::to_string(state.quantum) : std::string("sem limite")) +
        "   |   CPUs: " + std::to_string(state.cpu_count) +
        "   |   Relógio global: t = " + std::to_string(now_) +
        "   |   Perdas de prazo: " + std::to_string(simulator_.total_deadline_misses()) +
        (simulator_.finished() && !state.tasks.empty() ? "   |   SIMULAÇÃO CONCLUÍDA" : "");

    canvas.text(px(8), px(HEADER_H) / 2 - scroll_y, header, BLACK, px(13), TextAlign::Left, true);

    // ---------------------------------------------------------------
    // legenda (sempre visivel no final do grafico)
    // ---------------------------------------------------------------
    (void)view_height;

    draw_legend(canvas, legend_top() - scroll_y, std::max(view_width, px(LEGEND_ITEM_W) + px(20)));
}

void GanttRenderer::draw_legend(Canvas& canvas, int top, int width) const {

    int columns = legend_columns(width);

    canvas.line(px(8), top - px(6), width - px(8), top - px(6), GRID, 1, false);
    canvas.text(px(8), top + px(LEGEND_TITLE_H) / 2, "Legenda", BLACK, px(13), TextAlign::Left, true);

    for (int index = 0; index < LEGEND_COUNT; ++index) {

        int column = index % columns;
        int row = index / columns;

        int left = px(10) + column * px(LEGEND_ITEM_W);
        int item_top = top + px(LEGEND_TITLE_H) + row * px(LEGEND_ITEM_H);

        int x = left + px(24);
        int bar_top = item_top + px(BAR_TOP);

        const LegendItem& item = LEGEND[index];

        switch (item.code) {

            case LEGEND_RUNNING: {
                Rgb sample = {66, 99, 216};
                canvas.rect(left + px(6), bar_top, px(36), px(BAR_H), sample, FillStyle::Solid, AXIS, 1);
                canvas.text(left + px(24), bar_top + px(BAR_H) / 2, "0", contrast(sample), px(10), TextAlign::Center, true);
                break;
            }

            case LEGEND_READY:
                canvas.rect(left + px(6), bar_top + px(4), px(36), px(BAR_H - 8), WHITE, FillStyle::None, READY_BORDER, 1);
                break;

            case LEGEND_SUSPENDED:
                canvas.rect(left + px(6), bar_top, px(36), px(BAR_H), BLACK, FillStyle::Dots, BLACK, 1);
                break;

            case LEGEND_CPU_OFF:
                canvas.rect(left + px(6), bar_top, px(36), px(BAR_H), CPU_OFF, FillStyle::Hatch, CPU_OFF, 0);
                break;

            case LEGEND_NOW:
                canvas.line(x, item_top + px(4), x, item_top + px(44), NOW_LINE, 1, true);
                break;

            default: {

                // os simbolos que ficam abaixo ou acima da barra no grafico sao
                // deslocados na legenda para ficarem alinhados com o texto
                EventType type = static_cast<EventType>(item.code);

                int anchor = BAR_TOP + BAR_H / 2;

                if (type == EventType::Preemption || type == EventType::RandomTie) {
                    anchor = 39;
                }
                else if (type == EventType::QuantumExpired || type == EventType::Manual) {
                    anchor = 45;
                }
                else if (type == EventType::Deadline) {
                    anchor = 5;
                }

                int shift = px(BAR_TOP + BAR_H / 2) - px(anchor);

                if (anchor == BAR_TOP + BAR_H / 2) {
                    // linha de base cinza para dar contexto aos simbolos da barra
                    canvas.line(left + px(4), bar_top + px(BAR_H), left + px(44), bar_top + px(BAR_H), GRID, 1, false);
                }

                draw_glyph(canvas, type, x, item_top + shift, -1);
                break;
            }
        }

        canvas.text(left + px(52), bar_top + px(BAR_H) / 2, item.text, BLACK, px(12), TextAlign::Left, false);
    }
}

std::string GanttRenderer::describe_point(int x, int y, int scroll_x, int scroll_y) const {

    int content_x = x + scroll_x;
    int content_y = y + scroll_y;

    if (content_x < px(LABEL_W) + px(TIME_PAD) || x < px(LABEL_W)) {
        return "";
    }

    int time = (content_x - px(LABEL_W) - px(TIME_PAD)) / cell_width_;

    std::string prefix = "Gantt em t=" + std::to_string(time) + " → ";

    const auto& ticks = simulator_.ticks();

    bool has_record = time >= 0 && time < now_ && time < static_cast<int>(ticks.size());

    // linha de uma tarefa
    if (content_y >= rows_top() && content_y < axis_y()) {

        int row = (content_y - rows_top()) / px(ROW_H);

        if (row < 0 || row >= static_cast<int>(rows_.size())) {
            return "";
        }

        int id = rows_[row];

        std::string text = prefix + "T" + std::to_string(id) + ": ";

        std::string situation = "sem ativação/inativa";

        if (!has_record) {
            situation = "ainda não simulado";
        }
        else {
            for (const TickCell& cell : ticks[time].cells) {
                if (cell.task_id == id) {
                    situation = task_state_name(cell.state);
                    if (cell.state == TaskState::Running) {
                        situation += " na CPU " + std::to_string(cell.cpu);
                    }
                }
            }
        }

        text += situation;

        const auto& events = simulator_.all_events();

        for (std::size_t index = 0; index < visible_events_; ++index) {
            if (events[index].task_id == id && events[index].time == time) {
                text += " | " + std::string(event_type_name(events[index].type));
            }
        }

        return text;
    }

    // faixa de uma cpu
    if (content_y >= cpu_top() && content_y < cpu_top() + cpu_rows_ * px(CPU_ROW_H)) {

        int cpu = (content_y - cpu_top()) / px(CPU_ROW_H);

        std::string text = prefix + "CPU " + std::to_string(cpu) + ": ";

        if (!has_record) {
            return text + "ainda não simulado";
        }

        const TickRecord& record = ticks[time];

        if (cpu >= static_cast<int>(record.cpu_task.size())) {
            return text + "não existia neste instante";
        }

        int id = record.cpu_task[cpu];

        return text + (id < 0 ? std::string("desligada") : "executando T" + std::to_string(id));
    }

    return "";
}
