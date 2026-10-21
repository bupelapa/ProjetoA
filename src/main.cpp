#include "ConfigParser.h"
#include "GanttRenderer.h"
#include "MainWindow.h"
#include "Scheduler.h"
#include "Simulator.h"
#include "SvgCanvas.h"
#include "Util.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

// pasta onde fica o executavel; os escalonadores externos sao procurados
// em "<pasta do executavel>\escalonadores", e nao na pasta atual, para que
// o simulador funcione igual quando aberto por duplo clique ou pelo terminal
std::wstring executable_directory() {

    wchar_t buffer[32768];

    DWORD length = GetModuleFileNameW(nullptr, buffer, 32768);

    std::wstring path(buffer, length);

    std::size_t slash = path.find_last_of(L"\\/");

    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

void print(const std::string& text) {
    std::fputs(text.c_str(), stdout);
    std::fflush(stdout);
}

// modo de linha de comando (execucao completa sem intervencao humana)
//
//   simulador.exe --executar <arquivo_config> [<imagem.svg>]
//
// carrega o arquivo, executa a simulacao inteira e grava o gantt final;
// util para testes automatizados e para gerar varias imagens de uma vez
int run_headless(const std::vector<std::wstring>& arguments) {

    // o executavel e de interface grafica; se a saida nao foi redirecionada
    // para um arquivo/pipe, e preciso conectar-se ao console de quem o chamou
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

    bool redirected = output != nullptr && output != INVALID_HANDLE_VALUE && GetFileType(output) != FILE_TYPE_UNKNOWN;

    if (!redirected && (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole())) {
        std::freopen("CONOUT$", "w", stdout);
        std::freopen("CONOUT$", "w", stderr);
    }

    SetConsoleOutputCP(CP_UTF8);

    if (arguments.size() < 3) {
        print("\nuso: simulador --executar <arquivo_config> [<imagem.svg>]\n");
        return 1;
    }

    std::string config_path = wide_to_utf8(arguments[2]);

    std::string svg_path;

    if (arguments.size() >= 4) {
        svg_path = wide_to_utf8(arguments[3]);
    }
    else {
        std::size_t dot = config_path.find_last_of('.');
        std::size_t slash = config_path.find_last_of("\\/");
        svg_path = (dot != std::string::npos && (slash == std::string::npos || dot > slash) ? config_path.substr(0, dot) : config_path) + "_gantt.svg";
    }

    print("\n=== simulador de SO multitarefa de tempo compartilhado - projeto A ===\n");

    ConfigParser parser;
    Config config;
    std::vector<Task> tasks;
    ParseReport report;

    bool loaded = parser.load(config_path, config, tasks, report);

    for (const auto& line : report.errors) {
        print("ERRO: " + line + "\n");
    }

    for (const auto& line : report.warnings) {
        print("aviso: " + line + "\n");
    }

    for (const auto& line : report.info) {
        print("info: " + line + "\n");
    }

    if (!loaded) {
        print("falha ao carregar a configuracao\n");
        return 1;
    }

    Simulator simulator;

    simulator.load(config, tasks);

    std::string message;

    bool completed = simulator.run_to_end(message);

    if (!completed) {
        print("a simulacao foi interrompida: " + message + "\n");
    }

    const auto& events = simulator.all_events();

    print("\n--- eventos ---\n");

    for (std::size_t index = 0; index < simulator.visible_event_count(); ++index) {

        if (events[index].type == EventType::Deadline) {
            continue;
        }

        print("t=" + std::to_string(events[index].time) + " [" + event_type_name(events[index].type) + "] " + events[index].text + "\n");
    }

    const SimState& state = simulator.state();

    print("\n--- resultado final (t = " + std::to_string(state.time) + ") ---\n");
    print("algoritmo " + state.algorithm + ", quantum " + std::to_string(state.quantum) + ", " + std::to_string(state.cpu_count) + " CPU(s)\n\n");

    for (const Task& task : state.tasks) {
        print(task.describe() + "\n");
    }

    for (int cpu = 0; cpu < state.cpu_count; ++cpu) {
        print("CPU " + std::to_string(cpu) + " desligada por " + std::to_string(state.cpu_off_time(cpu)) + " tick(s)\n");
    }

    print("perdas de prazo: " + std::to_string(simulator.total_deadline_misses()) + "\n");

    GanttRenderer renderer(simulator, 26, 1.0);

    int width = std::max(renderer.content_width(), 1250);
    int height = renderer.content_height(width);

    SvgCanvas canvas(width, height);

    renderer.draw(canvas, 0, 0, width, height);

    FILE* file = _wfopen(utf8_to_wide(svg_path).c_str(), L"wb");

    if (file == nullptr) {
        print("ERRO: nao foi possivel gravar " + svg_path + "\n");
        return 1;
    }

    std::string document = canvas.document();

    std::fwrite(document.data(), 1, document.size(), file);
    std::fclose(file);

    print("\ngrafico de gantt gravado em " + svg_path + "\n");

    return completed ? 0 : 2;
}

}

// ponto de entrada de um programa grafico do windows
//
// a linha de comando e lida com GetCommandLineW (em utf-16) para aceitar
// caminhos com acentos, em vez de usar o argv do WinMain (codificacao ansi)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    int count = 0;

    LPWSTR* raw = CommandLineToArgvW(GetCommandLineW(), &count);

    std::vector<std::wstring> arguments;

    for (int index = 0; index < count; ++index) {
        arguments.push_back(raw[index]);
    }

    LocalFree(raw);

    // escalonadores externos (dlls) disponiveis para os dois modos
    std::vector<std::string> plugin_messages;

    SchedulerRegistry::instance().load_plugins(executable_directory() + L"\\escalonadores", plugin_messages);

    if (arguments.size() >= 2 && (arguments[1] == L"--executar" || arguments[1] == L"-e")) {
        return run_headless(arguments);
    }

    // simulador.exe <arquivo> abre a interface ja com o arquivo carregado
    // (tambem permite arrastar um arquivo .txt sobre o executavel)
    std::wstring initial_file = arguments.size() >= 2 ? arguments[1] : L"";

    return run_main_window(initial_file, plugin_messages);
}
