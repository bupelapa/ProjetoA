#include "MainWindow.h"

#include "ConfigParser.h"
#include "FormDialog.h"
#include "GanttRenderer.h"
#include "GdiCanvas.h"
#include "Scheduler.h"
#include "Simulator.h"
#include "SvgCanvas.h"
#include "Util.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <cstdio>

namespace {

// ---------------------------------------------------------------------------
// identificadores dos controles
// ---------------------------------------------------------------------------

enum ControlId {
    ID_OPEN = 100,
    ID_RELOAD,
    ID_START,
    ID_BACK,
    ID_FORWARD,
    ID_GOTO,
    ID_RUN,
    ID_SYSTEM,
    ID_NEW_TASK,
    ID_EDIT_TASK,
    ID_EXPORT,
    ID_ZOOM_OUT,
    ID_ZOOM_IN,
    ID_HELP,
    ID_TASK_LIST = 200,
    ID_EVENT_LIST
};

struct ButtonInfo {
    int id;
    const wchar_t* text;
    int width;
};

// barra de botoes: cada acao do simulador fica a um clique de distancia,
// e as teclas de atalho aparecem no proprio texto do botao
const ButtonInfo BUTTONS[] = {
    {ID_OPEN, L"Abrir arquivo... (Ctrl+O)", 170},
    {ID_RELOAD, L"Recarregar", 86},
    {ID_START, L"|◀ Início (t=0)", 110},
    {ID_BACK, L"◀ Voltar (F7)", 100},
    {ID_FORWARD, L"Avançar ▶ (F8)", 112},
    {ID_GOTO, L"Ir para t...", 86},
    {ID_RUN, L"Executar tudo ▶▶ (F9)", 160},
    {ID_SYSTEM, L"Sistema...", 84},
    {ID_NEW_TASK, L"Nova tarefa...", 104},
    {ID_EDIT_TASK, L"Editar tarefa... (F2)", 148},
    {ID_EXPORT, L"Exportar imagem SVG... (Ctrl+S)", 220},
    {ID_ZOOM_OUT, L"Zoom −", 66},
    {ID_ZOOM_IN, L"Zoom +", 66},
    {ID_HELP, L"Ajuda", 60},
};

struct Column {
    const wchar_t* title;
    int width;
};

// colunas da tabela de tcbs (modo depurador: todas as informacoes de cada
// tarefa ficam visiveis a cada passo, requisito 1.5.1)
const Column COLUMNS[] = {
    {L"ID", 44}, {L"Cor", 70}, {L"Estado", 140}, {L"CPU", 42}, {L"Ingresso", 64},
    {L"Duração", 64}, {L"Período", 60}, {L"Prazo", 52}, {L"Ativações", 72}, {L"Concluídas", 78},
    {L"Restante", 64}, {L"Liberação", 70}, {L"Prazo abs.", 74}, {L"Próx. ativação", 96},
    {L"Quantum usado", 98}, {L"Executado", 72}, {L"Tempo pronta", 92}, {L"Perdas de prazo", 108},
    {L"Fim", 46}, {L"Eventos (proj. B)", 140},
};

const wchar_t* MAIN_CLASS = L"SimuladorJanelaPrincipal";
const wchar_t* GANTT_CLASS = L"SimuladorGantt";

// ---------------------------------------------------------------------------
// estado da aplicacao
// ---------------------------------------------------------------------------

struct App {

    HINSTANCE instance = nullptr;

    HWND window = nullptr;
    HWND gantt = nullptr;
    HWND status = nullptr;
    HWND hover = nullptr;
    HWND task_list = nullptr;
    HWND event_list = nullptr;
    HWND task_label = nullptr;
    HWND event_label = nullptr;

    std::vector<HWND> buttons;

    HFONT font = nullptr;
    HFONT bold_font = nullptr;

    // fator de escala para monitores com dpi alto
    double scale = 1.0;

    Simulator simulator;

    // caminho do arquivo carregado (vazio = sistema criado manualmente)
    std::wstring file_path;

    // largura de um tick no gantt (zoom), em pixels a 96 dpi
    int cell_width = 26;

    int scroll_x = 0;
    int scroll_y = 0;

    // evita perguntar varias vezes se o usuario quer salvar a imagem final
    bool offered_export = false;

    std::vector<std::string> plugin_messages;
};

App app;

int S(int value) {
    return static_cast<int>(value * app.scale + 0.5);
}

int cell_pixels() {
    return S(app.cell_width);
}

// ---------------------------------------------------------------------------
// mensagens ao usuario (requisito 5: todo erro e explicado)
// ---------------------------------------------------------------------------

void show_error(const std::string& text, const std::wstring& title = L"Não foi possível concluir a operação") {
    MessageBoxW(app.window, utf8_to_wide(text).c_str(), title.c_str(), MB_OK | MB_ICONERROR);
}

void show_info(const std::string& text, const std::wstring& title = L"Simulador") {
    MessageBoxW(app.window, utf8_to_wide(text).c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

bool ask(const std::string& text, const std::wstring& title) {
    return MessageBoxW(app.window, utf8_to_wide(text).c_str(), title.c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES;
}

// ---------------------------------------------------------------------------
// atualizacao da tela
// ---------------------------------------------------------------------------

void update_gantt_scroll();

std::wstring number(int value) {
    return std::to_wstring(value);
}

void update_status() {

    const SimState& state = app.simulator.state();

    const Scheduler* scheduler = SchedulerRegistry::instance().find(state.algorithm);

    std::string file = app.file_path.empty() ? "(nenhum - sistema criado na interface)" : wide_to_utf8(app.file_path);

    std::string line1 =
        "Arquivo: " + file +
        "     |     Algoritmo: " + state.algorithm + (scheduler ? " - " + scheduler->description() : "") +
        "     |     Quantum: " + (state.quantum > 0 ? std::to_string(state.quantum) : std::string("sem limite")) +
        "     |     CPUs: " + std::to_string(state.cpu_count);

    std::string line2 =
        "Relógio global: t = " + std::to_string(state.time) +
        "     |     Histórico: passo " + std::to_string(app.simulator.position()) +
        " de " + std::to_string(app.simulator.history_size() - 1) + " já calculados" +
        "     |     Situação: " + (state.tasks.empty() ? "sem tarefas (abra um arquivo ou use Nova tarefa)" : (app.simulator.finished() ? "SIMULAÇÃO CONCLUÍDA" : "em andamento")) +
        "     |     Perdas de prazo: " + std::to_string(app.simulator.total_deadline_misses());

    std::string line3 = "Processadores:";

    for (int cpu = 0; cpu < state.cpu_count; ++cpu) {

        int id = cpu < static_cast<int>(state.cpu_task.size()) ? state.cpu_task[cpu] : -1;

        line3 += "   CPU " + std::to_string(cpu) + " → ";

        if (id >= 0) {

            const Task* task = state.find(id);

            line3 += "T" + std::to_string(id);

            if (task != nullptr && state.quantum > 0) {
                line3 += " (quantum " + std::to_string(task->quantum_used) + "/" + std::to_string(state.quantum) + ")";
            }
        }
        else if (state.cpu_off_since[cpu] >= 0) {
            line3 += "desligada desde t=" + std::to_string(state.cpu_off_since[cpu]);
        }
        else {
            line3 += "livre";
        }

        line3 += " [desligada " + std::to_string(state.cpu_off_time(cpu)) + " tick(s) no total]";
    }

    std::string line4 = "Fila global de prontas (ordem de prioridade): ";

    std::vector<int> queue = app.simulator.ready_queue();

    if (queue.empty()) {
        line4 += "vazia";
    }

    for (std::size_t index = 0; index < queue.size(); ++index) {
        line4 += (index > 0 ? "  →  " : "") + ("T" + std::to_string(queue[index]));
    }

    SetWindowTextW(app.status, utf8_to_wide(line1 + "\r\n" + line2 + "\r\n" + line3 + "\r\n" + line4).c_str());
}

void set_cell(int row, int column, const std::wstring& text) {

    LVITEMW item = {};

    item.iItem = row;
    item.iSubItem = column;
    item.pszText = const_cast<wchar_t*>(text.c_str());

    SendMessageW(app.task_list, LVM_SETITEMTEXTW, static_cast<WPARAM>(row), reinterpret_cast<LPARAM>(&item));
}

int selected_task_id() {

    int index = ListView_GetNextItem(app.task_list, -1, LVNI_SELECTED);

    if (index < 0) {
        return -1;
    }

    LVITEMW item = {};

    item.mask = LVIF_PARAM;
    item.iItem = index;

    SendMessageW(app.task_list, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&item));

    return static_cast<int>(item.lParam);
}

void update_task_list() {

    int selected = selected_task_id();

    SendMessageW(app.task_list, WM_SETREDRAW, FALSE, 0);

    ListView_DeleteAllItems(app.task_list);

    const SimState& state = app.simulator.state();

    int row = 0;

    for (const Task& task : state.tasks) {

        std::wstring id = number(task.id);

        LVITEMW item = {};

        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = row;
        item.pszText = const_cast<wchar_t*>(id.c_str());
        item.lParam = task.id;

        SendMessageW(app.task_list, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));

        bool active = task.state != TaskState::Terminated && task.state != TaskState::Ignored;

        set_cell(row, 1, utf8_to_wide(task.color));
        set_cell(row, 2, utf8_to_wide(task_state_name(task.state)));
        set_cell(row, 3, task.cpu >= 0 ? number(task.cpu) : L"-");
        set_cell(row, 4, number(task.arrival_time));
        set_cell(row, 5, number(task.duration));
        set_cell(row, 6, number(task.period));
        set_cell(row, 7, number(task.deadline));
        set_cell(row, 8, number(task.activations) + L"/" + number(MAX_ACTIVATIONS));
        set_cell(row, 9, number(task.executions_completed) + L"/" + number(MAX_ACTIVATIONS));
        set_cell(row, 10, task.has_job() ? number(task.remaining) : L"-");
        set_cell(row, 11, task.has_job() ? number(task.job_release) : L"-");
        set_cell(row, 12, task.has_job() ? number(task.job_deadline) + (task.job_missed ? L" (perdido)" : L"") : L"-");
        set_cell(row, 13, active && task.activations < MAX_ACTIVATIONS ? number(task.next_release) : L"-");
        set_cell(row, 14, number(task.quantum_used));
        set_cell(row, 15, number(task.total_executed));
        set_cell(row, 16, number(task.total_ready_time));
        set_cell(row, 17, number(task.deadline_misses));
        set_cell(row, 18, task.finish_time >= 0 ? number(task.finish_time) : L"-");
        set_cell(row, 19, utf8_to_wide(task.event_list));

        if (task.id == selected) {
            ListView_SetItemState(app.task_list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }

        ++row;
    }

    SendMessageW(app.task_list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(app.task_list, nullptr, TRUE);
}

void update_event_list() {

    SendMessageW(app.event_list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(app.event_list, LB_RESETCONTENT, 0, 0);

    const auto& events = app.simulator.all_events();

    std::size_t count = app.simulator.visible_event_count();

    for (std::size_t index = 0; index < count; ++index) {

        const SimEvent& event = events[index];

        // o prazo de cada ativacao ja aparece no grafico; na lista ele so
        // poluiria a leitura, entao e omitido
        if (event.type == EventType::Deadline) {
            continue;
        }

        std::string line = "t=" + std::to_string(event.time) + "   [" + event_type_name(event.type) + "]   " + event.text;

        SendMessageW(app.event_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(utf8_to_wide(line).c_str()));
    }

    // edicoes feitas no instante atual aparecem como pendentes
    for (const SimEvent& event : app.simulator.state().pending_events) {

        std::string line = "t=" + std::to_string(event.time) + "   [" + event_type_name(event.type) + " - aplicada neste instante]   " + event.text;

        SendMessageW(app.event_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(utf8_to_wide(line).c_str()));
    }

    LRESULT total = SendMessageW(app.event_list, LB_GETCOUNT, 0, 0);

    if (total > 0) {
        SendMessageW(app.event_list, LB_SETTOPINDEX, static_cast<WPARAM>(total - 1), 0);
    }

    SendMessageW(app.event_list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(app.event_list, nullptr, TRUE);
}

void update_buttons() {

    bool at_start = app.simulator.position() == 0;
    bool finished = app.simulator.finished();

    for (HWND button : app.buttons) {

        int id = GetDlgCtrlID(button);

        bool enabled = true;

        if (id == ID_START || id == ID_BACK) {
            enabled = !at_start;
        }
        else if (id == ID_FORWARD || id == ID_RUN) {
            enabled = !finished;
        }
        else if (id == ID_RELOAD) {
            enabled = !app.file_path.empty();
        }

        EnableWindow(button, enabled ? TRUE : FALSE);
    }
}

// rola o gantt para que o instante atual fique visivel
void ensure_now_visible() {

    RECT client;

    GetClientRect(app.gantt, &client);

    GanttRenderer renderer(app.simulator, cell_pixels(), app.scale);

    int x = renderer.time_to_x(app.simulator.position());

    int width = client.right;

    if (x < app.scroll_x + S(90) || x > app.scroll_x + width - S(40)) {
        app.scroll_x = std::max(0, x - width * 2 / 3);
    }
}

void refresh(bool follow_now) {

    update_status();
    update_task_list();
    update_event_list();
    update_buttons();

    if (follow_now) {
        ensure_now_visible();
    }

    update_gantt_scroll();

    InvalidateRect(app.gantt, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// gantt: rolagem e desenho
// ---------------------------------------------------------------------------

void update_gantt_scroll() {

    // SetScrollInfo pode mostrar/esconder barras e gerar um novo WM_SIZE;
    // a trava evita recursao infinita nesse caso
    static bool busy = false;

    if (busy || app.gantt == nullptr) {
        return;
    }

    busy = true;

    RECT client;

    GetClientRect(app.gantt, &client);

    GanttRenderer renderer(app.simulator, cell_pixels(), app.scale);

    int content_width = renderer.content_width();
    int content_height = renderer.content_height(client.right);

    app.scroll_x = std::max(0, std::min(app.scroll_x, content_width - static_cast<int>(client.right)));
    app.scroll_y = std::max(0, std::min(app.scroll_y, content_height - static_cast<int>(client.bottom)));

    SCROLLINFO info = {};

    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

    info.nMin = 0;
    info.nMax = content_width - 1;
    info.nPage = static_cast<UINT>(client.right);
    info.nPos = app.scroll_x;

    SetScrollInfo(app.gantt, SB_HORZ, &info, TRUE);

    info.nMax = content_height - 1;
    info.nPage = static_cast<UINT>(client.bottom);
    info.nPos = app.scroll_y;

    SetScrollInfo(app.gantt, SB_VERT, &info, TRUE);

    busy = false;
}

void scroll_gantt(int bar, int request, int amount_line) {

    SCROLLINFO info = {};

    info.cbSize = sizeof(info);
    info.fMask = SIF_ALL;

    GetScrollInfo(app.gantt, bar, &info);

    int position = info.nPos;

    switch (request) {
        case SB_LINELEFT: position -= amount_line; break;
        case SB_LINERIGHT: position += amount_line; break;
        case SB_PAGELEFT: position -= static_cast<int>(info.nPage); break;
        case SB_PAGERIGHT: position += static_cast<int>(info.nPage); break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: position = info.nTrackPos; break;
        case SB_LEFT: position = 0; break;
        case SB_RIGHT: position = info.nMax; break;
    }

    position = std::max(0, std::min(position, info.nMax - static_cast<int>(info.nPage) + 1));

    if (bar == SB_HORZ) {
        app.scroll_x = position;
    }
    else {
        app.scroll_y = position;
    }

    update_gantt_scroll();

    InvalidateRect(app.gantt, nullptr, FALSE);
}

void change_zoom(int delta) {

    // mantem o instante no centro da tela fixo durante o zoom
    RECT client;

    GetClientRect(app.gantt, &client);

    double center_time = static_cast<double>(app.scroll_x + client.right / 2 - S(84)) / cell_pixels();

    int widths[] = {3, 5, 8, 12, 18, 26, 36, 50, 70};

    int count = static_cast<int>(sizeof(widths) / sizeof(widths[0]));

    int index = 0;

    while (index < count - 1 && widths[index] < app.cell_width) {
        ++index;
    }

    index = std::max(0, std::min(count - 1, index + delta));

    app.cell_width = widths[index];

    app.scroll_x = std::max(0, static_cast<int>(center_time * cell_pixels()) + S(84) - static_cast<int>(client.right) / 2);

    update_gantt_scroll();

    InvalidateRect(app.gantt, nullptr, FALSE);
}

LRESULT CALLBACK gantt_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {

    switch (message) {

        case WM_ERASEBKGND:
            // o fundo e pintado junto com o grafico (evita piscar)
            return 1;

        case WM_PAINT: {

            PAINTSTRUCT paint;

            HDC dc = BeginPaint(window, &paint);

            RECT client;

            GetClientRect(window, &client);

            // desenho em memoria e copia unica para a tela (double buffering)
            HDC memory = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, std::max<LONG>(1, client.right), std::max<LONG>(1, client.bottom));
            HGDIOBJ old_bitmap = SelectObject(memory, bitmap);

            FillRect(memory, &client, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

            {
                GdiCanvas canvas(memory);

                GanttRenderer renderer(app.simulator, cell_pixels(), app.scale);

                renderer.draw(canvas, app.scroll_x, app.scroll_y, client.right, client.bottom);
            }

            BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);

            SelectObject(memory, old_bitmap);
            DeleteObject(bitmap);
            DeleteDC(memory);

            EndPaint(window, &paint);

            return 0;
        }

        case WM_SIZE:
            update_gantt_scroll();
            InvalidateRect(window, nullptr, FALSE);
            return 0;

        case WM_HSCROLL:
            scroll_gantt(SB_HORZ, LOWORD(wparam), cell_pixels());
            return 0;

        case WM_VSCROLL:
            scroll_gantt(SB_VERT, LOWORD(wparam), S(25));
            return 0;

        case WM_MOUSEWHEEL: {

            int delta = GET_WHEEL_DELTA_WPARAM(wparam);

            if (GET_KEYSTATE_WPARAM(wparam) & MK_CONTROL) {
                change_zoom(delta > 0 ? 1 : -1);
                return 0;
            }

            // shift (ou grafico que cabe na vertical) rola na horizontal,
            // que e o sentido em que o tempo cresce
            SCROLLINFO info = {};

            info.cbSize = sizeof(info);
            info.fMask = SIF_ALL;

            GetScrollInfo(window, SB_VERT, &info);

            bool vertical_needed = info.nMax >= static_cast<int>(info.nPage);

            if ((GET_KEYSTATE_WPARAM(wparam) & MK_SHIFT) || !vertical_needed) {
                app.scroll_x = std::max(0, app.scroll_x - delta * cell_pixels() * 3 / WHEEL_DELTA);
            }
            else {
                app.scroll_y = std::max(0, app.scroll_y - delta * S(40) / WHEEL_DELTA);
            }

            update_gantt_scroll();

            InvalidateRect(window, nullptr, FALSE);

            return 0;
        }

        case WM_MOUSEMOVE: {

            int x = static_cast<short>(LOWORD(lparam));
            int y = static_cast<short>(HIWORD(lparam));

            GanttRenderer renderer(app.simulator, cell_pixels(), app.scale);

            std::string text = renderer.describe_point(x, y, app.scroll_x, app.scroll_y);

            if (text.empty()) {
                text = "Passe o mouse sobre o gráfico para inspecionar um instante.  Ctrl + roda do mouse = zoom;  Shift + roda = rolar no tempo.";
            }

            SetWindowTextW(app.hover, utf8_to_wide(text).c_str());

            return 0;
        }

        case WM_LBUTTONDOWN:
            SetFocus(window);
            return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

// ---------------------------------------------------------------------------
// arquivos
// ---------------------------------------------------------------------------

bool write_file(const std::wstring& path, const std::string& content) {

    FILE* file = _wfopen(path.c_str(), L"wb");

    if (file == nullptr) {
        return false;
    }

    bool ok = std::fwrite(content.data(), 1, content.size(), file) == content.size();

    ok = (std::fclose(file) == 0) && ok;

    return ok;
}

std::string join_lines(const std::vector<std::string>& lines) {

    std::string text;

    for (const auto& line : lines) {
        text += "• " + line + "\n";
    }

    return text;
}

// formulario dos parametros gerais do sistema
// devolve false se o usuario cancelar
bool ask_system_parameters(Config& config, const std::wstring& title, const std::string& extra_info) {

    std::vector<FormField> fields(3);

    fields[0].label = L"Algoritmo de escalonamento:";

    for (const auto& name : SchedulerRegistry::instance().names()) {
        fields[0].options.push_back(utf8_to_wide(name));
    }

    fields[0].value = utf8_to_wide(config.scheduling_algorithm);

    fields[1].label = L"Quantum (ticks; 0 = sem limite):";
    fields[1].value = number(config.quantum);

    fields[2].label = L"Quantidade de CPUs (1 a " + number(MAX_CPUS) + L"):";
    fields[2].value = number(config.cpu_count);

    std::string info = extra_info + "Algoritmos disponíveis:\n";

    for (const auto& name : SchedulerRegistry::instance().names()) {
        info += "  " + name + ": " + SchedulerRegistry::instance().find(name)->description() + "\n";
    }

    info += "Valores padrão sugeridos: " + std::string(DEFAULT_ALGORITHM) + ", quantum " + std::to_string(DEFAULT_QUANTUM) +
            ", " + std::to_string(DEFAULT_CPU_COUNT) + " CPU.";

    while (true) {

        if (!show_form(app.window, title, utf8_to_wide(info), fields, app.scale)) {
            return false;
        }

        int quantum = 0;
        int cpus = 0;

        if (!parse_int(wide_to_utf8(fields[1].value), quantum) || quantum < 0) {
            show_error("O quantum deve ser um número inteiro maior ou igual a zero.");
            continue;
        }

        if (!parse_int(wide_to_utf8(fields[2].value), cpus) || cpus < 1 || cpus > MAX_CPUS) {
            show_error("A quantidade de CPUs deve ser um número inteiro entre 1 e " + std::to_string(MAX_CPUS) + ".");
            continue;
        }

        config.scheduling_algorithm = wide_to_utf8(fields[0].value);
        config.quantum = quantum;
        config.cpu_count = cpus;

        return true;
    }
}

void load_file(const std::wstring& path) {

    ConfigParser parser;

    Config config;

    std::vector<Task> tasks;

    ParseReport report;

    if (!parser.load(wide_to_utf8(path), config, tasks, report)) {

        show_error(
            "O arquivo de configuração não pôde ser carregado:\n" + wide_to_utf8(path) + "\n\n" +
            join_lines(report.errors) +
            "\nFormato esperado:\n  algoritmo_escalonamento;quantum;qtde_cpus\n  id;cor;ingresso;duracao;periodo;prazo;lista_eventos",
            L"Erro no arquivo de configuração"
        );

        return;
    }

    std::string notes;

    if (!report.warnings.empty()) {
        notes += "Avisos:\n" + join_lines(report.warnings) + "\n";
    }

    if (!report.info.empty()) {
        notes += join_lines(report.info) + "\n";
    }

    // o usuario confirma (ou altera) os parametros lidos antes do inicio,
    // o que tambem cumpre o requisito 4.1 (algoritmo escolhido antes da simulacao)
    std::string info =
        "Arquivo: " + wide_to_utf8(path) + "\n" +
        std::to_string(tasks.size()) + " tarefa(s) lida(s).\n" + notes +
        "Os valores abaixo foram lidos do arquivo; altere-os se desejar.\n\n";

    if (!ask_system_parameters(config, L"Parâmetros da simulação", info)) {
        return;
    }

    app.simulator.load(config, tasks);
    app.file_path = path;
    app.scroll_x = 0;
    app.scroll_y = 0;
    app.offered_export = false;

    refresh(true);
}

void open_file() {

    wchar_t buffer[32768] = L"";

    OPENFILENAMEW dialog = {};

    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = L"Arquivos de texto (*.txt)\0*.txt\0Todos os arquivos (*.*)\0*.*\0";
    dialog.nFilterIndex = 2;
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    dialog.lpstrTitle = L"Abrir arquivo de configuração da simulação";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    // qualquer arquivo, de qualquer pasta, unidade ou pendrive (requisito 3.3.4)
    if (GetOpenFileNameW(&dialog)) {
        load_file(buffer);
    }
}

void export_svg() {

    if (!app.simulator.finished()) {
        if (!ask("A simulação ainda não terminou (t = " + std::to_string(app.simulator.position()) +
                 ").\nDeseja exportar o gráfico até o instante atual mesmo assim?", L"Exportar gráfico de Gantt")) {
            return;
        }
    }

    std::wstring suggestion = L"gantt.svg";
    std::wstring initial_folder;

    if (!app.file_path.empty()) {

        // a imagem e sugerida na mesma pasta do arquivo de configuracao,
        // para que cada configuracao fique junto do seu resultado
        std::size_t slash = app.file_path.find_last_of(L"\\/");

        std::wstring folder = slash == std::wstring::npos ? L"" : app.file_path.substr(0, slash + 1);
        std::wstring name = app.file_path.substr(slash == std::wstring::npos ? 0 : slash + 1);

        std::size_t dot = name.find_last_of(L'.');

        suggestion = (dot == std::wstring::npos ? name : name.substr(0, dot)) + L"_gantt.svg";

        // sem a pasta inicial explicita, o windows abriria a ultima pasta usada
        initial_folder = folder;
    }

    wchar_t buffer[32768];

    lstrcpynW(buffer, suggestion.c_str(), 32768);

    OPENFILENAMEW dialog = {};

    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = L"Imagem SVG (*.svg)\0*.svg\0";
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    dialog.lpstrDefExt = L"svg";
    dialog.lpstrInitialDir = initial_folder.empty() ? nullptr : initial_folder.c_str();
    dialog.lpstrTitle = L"Salvar gráfico de Gantt";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetSaveFileNameW(&dialog)) {
        return;
    }

    // a imagem e desenhada do zero em escala 1, com toda a simulacao,
    // independentemente do zoom ou da rolagem da tela (requisito 2.4)
    GanttRenderer renderer(app.simulator, 26, 1.0);

    int width = std::max(renderer.content_width(), 1250);
    int height = renderer.content_height(width);

    SvgCanvas canvas(width, height);

    renderer.draw(canvas, 0, 0, width, height);

    if (!write_file(buffer, canvas.document())) {
        show_error("Não foi possível gravar o arquivo:\n" + wide_to_utf8(buffer) + "\n\nVerifique se a pasta existe e se você tem permissão de escrita.");
        return;
    }

    show_info("Gráfico de Gantt salvo em:\n" + wide_to_utf8(buffer) + "\n\nO arquivo SVG pode ser aberto em qualquer navegador.", L"Imagem exportada");
}

// ao chegar no fim da simulacao, oferece salvar a imagem final (requisito 2.4)
void offer_export_if_finished() {

    if (!app.simulator.finished() || app.offered_export) {
        return;
    }

    app.offered_export = true;

    if (ask("Simulação concluída em t = " + std::to_string(app.simulator.position()) + " com " +
            std::to_string(app.simulator.total_deadline_misses()) + " perda(s) de prazo.\n\n"
            "Deseja salvar agora a imagem (SVG) do gráfico de Gantt completo?", L"Simulação concluída")) {
        export_svg();
    }
}

// ---------------------------------------------------------------------------
// acoes dos botoes
// ---------------------------------------------------------------------------

void step_forward() {

    std::string message;

    if (!app.simulator.step_forward(message)) {
        show_info(message);
    }

    refresh(true);

    offer_export_if_finished();
}

void step_back() {

    if (app.simulator.step_back()) {
        refresh(true);
    }
}

void run_all() {

    std::string message;

    HCURSOR old_cursor = SetCursor(LoadCursor(nullptr, IDC_WAIT));

    bool ok = app.simulator.run_to_end(message);

    SetCursor(old_cursor);

    refresh(true);

    if (!ok) {
        show_error(message, L"A execução completa foi interrompida");
        return;
    }

    offer_export_if_finished();
}

void go_to_time() {

    std::vector<FormField> fields(1);

    fields[0].label = L"Ir para o instante (tick):";
    fields[0].value = number(app.simulator.position());

    std::string info = "Instantes já calculados: 0 a " + std::to_string(app.simulator.history_size() - 1) +
                       ".\nUm instante posterior é calculado automaticamente (se a simulação não terminar antes).";

    if (!show_form(app.window, L"Ir para um instante", utf8_to_wide(info), fields, app.scale)) {
        return;
    }

    int time = 0;

    if (!parse_int(wide_to_utf8(fields[0].value), time)) {
        show_error("O instante deve ser um número inteiro.");
        return;
    }

    std::string message;

    if (!app.simulator.go_to(time, message)) {
        show_info(message);
    }

    refresh(true);

    offer_export_if_finished();
}

void edit_system() {

    Config config;

    const SimState& state = app.simulator.state();

    config.scheduling_algorithm = state.algorithm;
    config.quantum = state.quantum;
    config.cpu_count = state.cpu_count;

    std::string info =
        "Instante atual: t = " + std::to_string(state.time) + ".\n"
        "O quantum e a quantidade de CPUs podem ser alterados em qualquer instante; o algoritmo, apenas em t = 0 "
        "(ele deve ser escolhido antes da simulação). Alterar algo descarta os passos já calculados após este instante.\n\n";

    if (!ask_system_parameters(config, L"Parâmetros do sistema", info)) {
        return;
    }

    std::string error = app.simulator.set_system(config.scheduling_algorithm, config.quantum, config.cpu_count);

    if (!error.empty()) {
        show_error(error, L"Alteração não aplicada");
        return;
    }

    app.offered_export = false;

    refresh(false);
}

void new_task() {

    const SimState& state = app.simulator.state();

    int next_id = 1;

    for (const Task& task : state.tasks) {
        next_id = std::max(next_id, task.id + 1);
    }

    std::vector<FormField> fields(6);

    fields[0].label = L"ID (único):";
    fields[0].value = number(next_id);
    fields[1].label = L"Cor (RRGGBB hexadecimal):";
    fields[1].value = utf8_to_wide(default_color(static_cast<int>(state.tasks.size())));
    fields[2].label = L"Ingresso (tick, ≥ t atual):";
    fields[2].value = number(state.time);
    fields[3].label = L"Duração (ticks):";
    fields[3].value = number(DEFAULT_DURATION);
    fields[4].label = L"Período (ticks, > 0):";
    fields[4].value = number(DEFAULT_PERIOD);
    fields[5].label = L"Prazo relativo (vazio = período):";
    fields[5].value = L"";

    std::string info = "Nova tarefa periódica inserida no instante t = " + std::to_string(state.time) +
                       ".\nOs valores sugeridos podem ser alterados.";

    while (true) {

        if (!show_form(app.window, L"Nova tarefa", utf8_to_wide(info), fields, app.scale)) {
            return;
        }

        int values[5] = {0, 0, 0, 0, 0};

        const int numeric_fields[] = {0, 2, 3, 4};

        bool ok = true;

        for (int index : numeric_fields) {
            if (!parse_int(wide_to_utf8(fields[index].value), values[index == 0 ? 0 : index - 1])) {
                show_error("O campo \"" + wide_to_utf8(fields[index].label) + "\" deve ser um número inteiro.");
                ok = false;
                break;
            }
        }

        if (!ok) {
            continue;
        }

        int deadline = values[3];

        if (!trim(wide_to_utf8(fields[5].value)).empty() && !parse_int(wide_to_utf8(fields[5].value), deadline)) {
            show_error("O prazo deve ser um número inteiro (ou vazio para usar o período).");
            continue;
        }

        Task task(values[0], wide_to_utf8(fields[1].value), values[1], values[2], values[3], deadline);

        std::string error = app.simulator.add_task(task);

        if (!error.empty()) {
            show_error(error, L"Tarefa não adicionada");
            continue;
        }

        break;
    }

    app.offered_export = false;

    refresh(false);
}

void edit_task(int id) {

    const SimState& state = app.simulator.state();

    // sem linha selecionada: o usuario escolhe a tarefa em uma lista
    if (id < 0) {

        if (state.tasks.empty()) {
            show_info("Não há tarefas no sistema. Abra um arquivo de configuração ou use \"Nova tarefa\".");
            return;
        }

        std::vector<FormField> choose(1);

        choose[0].label = L"Tarefa:";

        for (const Task& task : state.tasks) {
            choose[0].options.push_back(L"T" + number(task.id));
        }

        choose[0].value = choose[0].options.front();

        if (!show_form(app.window, L"Editar tarefa", L"Escolha a tarefa a examinar/alterar (ou dê um duplo clique na tabela).", choose, app.scale)) {
            return;
        }

        parse_int(wide_to_utf8(choose[0].value.substr(1)), id);
    }

    const Task* current = state.find(id);

    if (current == nullptr) {
        return;
    }

    const wchar_t* actions[] = {
        L"Manter o estado atual",
        L"Suspender a tarefa",
        L"Retomar (voltar para a fila de prontas)",
        L"Encerrar a tarefa"
    };

    std::vector<FormField> fields(8);

    fields[0].label = L"Cor (RRGGBB hexadecimal):";
    fields[0].value = utf8_to_wide(current->color);
    fields[1].label = L"Ingresso (tick):";
    fields[1].value = number(current->arrival_time);
    fields[2].label = L"Duração (ticks):";
    fields[2].value = number(current->duration);
    fields[3].label = L"Período (ticks):";
    fields[3].value = number(current->period);
    fields[4].label = L"Prazo relativo (ticks):";
    fields[4].value = number(current->deadline);
    fields[5].label = L"Tempo restante da ativação atual:";
    fields[5].value = number(current->remaining);
    fields[6].label = L"Mudança de estado:";
    fields[6].options.assign(std::begin(actions), std::end(actions));
    fields[6].value = actions[0];
    fields[7].label = L"Lista de eventos (projeto B):";
    fields[7].value = utf8_to_wide(current->event_list);
    fields[7].read_only = true;

    std::string info =
        current->describe() +
        "\nInstante atual: t = " + std::to_string(state.time) +
        ". As alterações valem a partir deste instante e descartam os passos já calculados depois dele.";

    while (true) {

        if (!show_form(app.window, L"Examinar / alterar tarefa T" + number(id), utf8_to_wide(info), fields, app.scale)) {
            return;
        }

        TaskEdit edit;

        edit.color = wide_to_utf8(fields[0].value);

        int* targets[] = {&edit.arrival_time, &edit.duration, &edit.period, &edit.deadline, &edit.remaining};

        bool ok = true;

        for (int index = 0; index < 5; ++index) {
            if (!parse_int(wide_to_utf8(fields[index + 1].value), *targets[index])) {
                show_error("O campo \"" + wide_to_utf8(fields[index + 1].label) + "\" deve ser um número inteiro.");
                ok = false;
                break;
            }
        }

        if (!ok) {
            continue;
        }

        for (int index = 0; index < 4; ++index) {
            if (fields[6].value == actions[index]) {
                edit.action = static_cast<TaskEdit::Action>(index);
            }
        }

        std::string error = app.simulator.edit_task(id, edit);

        if (!error.empty()) {
            show_error(error, L"Alteração não aplicada");
            continue;
        }

        break;
    }

    app.offered_export = false;

    refresh(false);
}

void show_help() {

    std::string schedulers;

    for (const auto& name : SchedulerRegistry::instance().names()) {
        schedulers += "  " + name + ": " + SchedulerRegistry::instance().find(name)->description() + "\n";
    }

    std::string plugins = app.plugin_messages.empty()
        ? "  nenhuma DLL encontrada na pasta \"escalonadores\"\n"
        : join_lines(app.plugin_messages);

    show_info(
        "SIMULADOR DE SO MULTITAREFA DE TEMPO COMPARTILHADO (Projeto A)\n\n"
        "1. Abra um arquivo de configuração (qualquer nome/pasta) ou crie tarefas com \"Nova tarefa\".\n"
        "2. Modo passo a passo: \"Avançar\" (F8) e \"Voltar\" (F7) movem o relógio global um tick.\n"
        "    A tabela mostra o TCB de cada tarefa; passe o mouse no gráfico para inspecionar um instante.\n"
        "3. Modo completo: \"Executar tudo\" (F9) roda até o fim e mostra apenas o resultado final.\n"
        "4. Em qualquer instante: duplo clique (ou F2) em uma tarefa para examiná-la e alterar\n"
        "    parâmetros ou estado (suspender, retomar, encerrar); \"Sistema\" altera quantum/CPUs.\n"
        "5. \"Exportar imagem SVG\" (Ctrl+S) grava o gráfico de Gantt completo.\n\n"
        "Gráfico: tarefas com menor ID junto ao eixo do tempo; o número dentro da barra é a CPU usada.\n"
        "A legenda completa aparece no final do gráfico (role para baixo, se necessário).\n\n"
        "Algoritmos disponíveis:\n" + schedulers +
        "\nEscalonadores externos (DLLs):\n" + plugins +
        "\nDesempate (requisito 4.3): tarefa que já executava, menor prazo, ingresso mais cedo,\n"
        "menor duração e, por fim, sorteio (marcado com \"?\" no gráfico).",
        L"Ajuda"
    );
}

void handle_command(int id) {

    switch (id) {
        case ID_OPEN: open_file(); break;
        case ID_RELOAD: if (!app.file_path.empty()) { std::wstring path = app.file_path; load_file(path); } break;
        case ID_START: app.simulator.go_to_start(); refresh(true); break;
        case ID_BACK: step_back(); break;
        case ID_FORWARD: step_forward(); break;
        case ID_GOTO: go_to_time(); break;
        case ID_RUN: run_all(); break;
        case ID_SYSTEM: edit_system(); break;
        case ID_NEW_TASK: new_task(); break;
        case ID_EDIT_TASK: edit_task(selected_task_id()); break;
        case ID_EXPORT: export_svg(); break;
        case ID_ZOOM_IN: change_zoom(1); break;
        case ID_ZOOM_OUT: change_zoom(-1); break;
        case ID_HELP: show_help(); break;
    }
}

// atalhos de teclado validos em toda a janela principal
bool handle_shortcut(const MSG& message) {

    if (message.message != WM_KEYDOWN) {
        return false;
    }

    bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    switch (message.wParam) {
        case VK_F7: handle_command(ID_BACK); return true;
        case VK_F8: handle_command(ID_FORWARD); return true;
        case VK_F9: handle_command(ID_RUN); return true;
        case VK_F2: handle_command(ID_EDIT_TASK); return true;
        case 'O': if (control) { handle_command(ID_OPEN); return true; } break;
        case 'S': if (control) { handle_command(ID_EXPORT); return true; } break;
    }

    return false;
}

// ---------------------------------------------------------------------------
// janela principal
// ---------------------------------------------------------------------------

void layout() {

    RECT client;

    GetClientRect(app.window, &client);

    int width = client.right;
    int height = client.bottom;
    int margin = S(6);

    int x = margin;
    int y = margin;

    // os botoes quebram linha quando a janela e estreita
    for (std::size_t index = 0; index < app.buttons.size(); ++index) {

        int button_width = S(BUTTONS[index].width);

        if (x + button_width > width - margin && x > margin) {
            x = margin;
            y += S(30) + S(4);
        }

        MoveWindow(app.buttons[index], x, y, button_width, S(30), TRUE);

        x += button_width + S(4);
    }

    y += S(30) + S(6);

    MoveWindow(app.status, margin, y, width - margin * 2, S(70), TRUE);
    y += S(72);

    MoveWindow(app.hover, margin, y, width - margin * 2, S(20), TRUE);
    y += S(22);

    int remaining = height - y - margin;
    int gantt_height = std::max(S(160), remaining * 58 / 100);

    MoveWindow(app.gantt, margin, y, width - margin * 2, gantt_height, TRUE);
    y += gantt_height + S(6);

    int list_width = (width - margin * 3) * 62 / 100;
    int bottom_height = std::max(S(60), height - y - S(20) - margin);

    MoveWindow(app.task_label, margin, y, list_width, S(18), TRUE);
    MoveWindow(app.event_label, margin * 2 + list_width, y, width - list_width - margin * 3, S(18), TRUE);
    y += S(20);

    MoveWindow(app.task_list, margin, y, list_width, bottom_height, TRUE);
    MoveWindow(app.event_list, margin * 2 + list_width, y, width - list_width - margin * 3, bottom_height, TRUE);
}

HWND create_child(const wchar_t* class_name, const wchar_t* text, DWORD style, DWORD ex_style, int id) {

    HWND control = CreateWindowExW(ex_style, class_name, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10,
                                   app.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), app.instance, nullptr);

    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(app.font), TRUE);

    return control;
}

void create_controls() {

    for (const ButtonInfo& button : BUTTONS) {
        app.buttons.push_back(create_child(L"BUTTON", button.text, WS_TABSTOP | BS_PUSHBUTTON, 0, button.id));
    }

    app.status = create_child(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, 0, 0);
    app.hover = create_child(L"STATIC", L"Passe o mouse sobre o gráfico para inspecionar um instante.", SS_LEFT | SS_NOPREFIX, 0, 0);

    SendMessageW(app.status, WM_SETFONT, reinterpret_cast<WPARAM>(app.bold_font), TRUE);

    app.gantt = CreateWindowExW(WS_EX_CLIENTEDGE, GANTT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL,
                                0, 0, 10, 10, app.window, nullptr, app.instance, nullptr);

    app.task_label = create_child(L"STATIC", L"Tarefas (TCB) no instante atual — duplo clique para examinar/alterar:", SS_LEFT, 0, 0);
    app.event_label = create_child(L"STATIC", L"Eventos ocorridos até o instante atual:", SS_LEFT, 0, 0);

    app.task_list = create_child(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_TASK_LIST);

    ListView_SetExtendedListViewStyle(app.task_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    int index = 0;

    for (const Column& column : COLUMNS) {

        LVCOLUMNW info = {};

        info.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        info.pszText = const_cast<wchar_t*>(column.title);
        info.cx = S(column.width);
        info.iSubItem = index;

        SendMessageW(app.task_list, LVM_INSERTCOLUMNW, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(&info));

        ++index;
    }

    app.event_list = create_child(L"LISTBOX", L"", WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY | WS_TABSTOP, WS_EX_CLIENTEDGE, ID_EVENT_LIST);

    SendMessageW(app.event_list, LB_SETHORIZONTALEXTENT, static_cast<WPARAM>(S(1600)), 0);
}

// pinta a coluna "cor" da tabela com a cor da propria tarefa e destaca
// em vermelho as tarefas cuja ativacao atual ja perdeu o prazo
LRESULT handle_list_custom_draw(NMLVCUSTOMDRAW* draw) {

    switch (draw->nmcd.dwDrawStage) {

        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT:
            return CDRF_NOTIFYSUBITEMDRAW;

        case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {

            const Task* task = app.simulator.state().find(static_cast<int>(draw->nmcd.lItemlParam));

            draw->clrText = RGB(0, 0, 0);
            draw->clrTextBk = RGB(255, 255, 255);

            if (task != nullptr && draw->iSubItem == 1) {

                unsigned char r = 0;
                unsigned char g = 0;
                unsigned char b = 0;

                color_to_rgb(task->color, r, g, b);

                draw->clrTextBk = RGB(r, g, b);
                draw->clrText = (r * 299 + g * 587 + b * 114) / 1000 > 140 ? RGB(0, 0, 0) : RGB(255, 255, 255);
            }
            else if (task != nullptr && task->job_missed && task->has_job()) {
                draw->clrText = RGB(200, 0, 0);
            }
            else if (task != nullptr && task->state == TaskState::Running) {
                draw->clrTextBk = RGB(225, 240, 255);
            }

            return CDRF_NEWFONT;
        }
    }

    return CDRF_DODEFAULT;
}

LRESULT CALLBACK main_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {

    switch (message) {

        case WM_CREATE:
            app.window = window;
            create_controls();
            return 0;

        case WM_SIZE:
            layout();
            return 0;

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            info->ptMinTrackSize.x = S(800);
            info->ptMinTrackSize.y = S(600);
            return 0;
        }

        case WM_COMMAND:

            if (HIWORD(wparam) == BN_CLICKED && LOWORD(wparam) >= ID_OPEN && LOWORD(wparam) <= ID_HELP) {
                handle_command(LOWORD(wparam));
                return 0;
            }

            break;

        case WM_NOTIFY: {

            auto* header = reinterpret_cast<NMHDR*>(lparam);

            if (header->hwndFrom == app.task_list) {

                if (header->code == NM_CUSTOMDRAW) {
                    return handle_list_custom_draw(reinterpret_cast<NMLVCUSTOMDRAW*>(lparam));
                }

                if (header->code == NM_DBLCLK || header->code == NM_RETURN) {
                    int id = selected_task_id();
                    if (id >= 0) {
                        edit_task(id);
                    }
                    return 0;
                }
            }

            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

}

int run_main_window(const std::wstring& initial_file, const std::vector<std::string>& plugin_messages) {

    app.instance = GetModuleHandleW(nullptr);
    app.plugin_messages = plugin_messages;

    // com dpi aware o windows nao "estica" a janela (o que deixaria o texto
    // borrado); as medidas sao entao escaladas manualmente por S()
    SetProcessDPIAware();

    HDC screen = GetDC(nullptr);

    app.scale = GetDeviceCaps(screen, LOGPIXELSX) / 96.0;

    ReleaseDC(nullptr, screen);

    INITCOMMONCONTROLSEX controls = {sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES};

    InitCommonControlsEx(&controls);

    app.font = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    app.bold_font = CreateFontW(-S(12), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    WNDCLASSW gantt_class = {};

    gantt_class.lpfnWndProc = gantt_procedure;
    gantt_class.hInstance = app.instance;
    gantt_class.hCursor = LoadCursor(nullptr, IDC_CROSS);
    gantt_class.lpszClassName = GANTT_CLASS;

    RegisterClassW(&gantt_class);

    WNDCLASSW main_class = {};

    main_class.lpfnWndProc = main_procedure;
    main_class.hInstance = app.instance;
    main_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    main_class.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    main_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    main_class.lpszClassName = MAIN_CLASS;

    RegisterClassW(&main_class);

    // sistema inicial vazio com os valores padrao, para que o usuario possa
    // montar uma simulacao do zero mesmo sem arquivo
    app.simulator.load(Config(), {});

    CreateWindowExW(0, MAIN_CLASS, L"Simulador de SO Multitarefa de Tempo Compartilhado — Projeto A",
                    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, S(1360), S(860),
                    nullptr, nullptr, app.instance, nullptr);

    ShowWindow(app.window, SW_SHOWMAXIMIZED);
    UpdateWindow(app.window);

    refresh(true);

    if (!initial_file.empty()) {
        load_file(initial_file);
    }

    MSG message;

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {

        // atalhos so valem quando a janela principal (e nao um formulario) esta ativa
        if (GetAncestor(message.hwnd, GA_ROOT) == app.window && handle_shortcut(message)) {
            continue;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    DeleteObject(app.font);
    DeleteObject(app.bold_font);

    return static_cast<int>(message.wParam);
}
