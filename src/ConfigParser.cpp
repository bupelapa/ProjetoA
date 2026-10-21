#include "ConfigParser.h"

#include "Scheduler.h"
#include "Util.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <sstream>

namespace {

std::string line_prefix(int line_number) {
    return "linha " + std::to_string(line_number) + ": ";
}

// le um campo numerico opcional
//
// campo ausente ou vazio: aplica o valor padrao e avisa o usuario (requisito 3.2)
// campo presente mas invalido: e um erro do arquivo e deve ser reportado
// devolve false somente no caso de erro
bool read_number(
    const std::vector<std::string>& fields,
    std::size_t index,
    const std::string& field_name,
    int default_value,
    int line_number,
    int& value,
    ParseReport& report
) {

    if (index >= fields.size() || fields[index].empty()) {

        value = default_value;

        report.warnings.push_back(
            line_prefix(line_number) + "campo '" + field_name +
            "' ausente; usado o valor padrão " + std::to_string(default_value)
        );

        return true;
    }

    if (!parse_int(fields[index], value)) {

        report.errors.push_back(
            line_prefix(line_number) + "o campo '" + field_name +
            "' deve ser um número inteiro (valor lido: \"" + fields[index] + "\")"
        );

        return false;
    }

    return true;
}

}

// carrega o arquivo de configuracao e transforma seus dados
// em uma configuracao do sistema e uma lista de tarefas
bool ConfigParser::load(
    const std::string& filename,
    Config& config,
    std::vector<Task>& tasks,
    ParseReport& report
) const {

    // no windows o caminho e convertido para utf-16, pois somente assim
    // e possivel abrir arquivos cujo caminho contenha acentos ou que estejam
    // em qualquer unidade/pendrive (requisito 3.3.4)
#ifdef _WIN32
    FILE* file = _wfopen(utf8_to_wide(filename).c_str(), L"rb");
#else
    FILE* file = std::fopen(filename.c_str(), "rb");
#endif

    if (file == nullptr) {

        report.errors.push_back(
            "não foi possível abrir o arquivo de configuração: " + filename +
            " (verifique se o arquivo existe e se você tem permissão de leitura)"
        );

        return false;
    }

    std::string content;

    char buffer[4096];

    std::size_t count = 0;

    while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        content.append(buffer, count);
    }

    std::fclose(file);

    return parse(content, config, tasks, report);
}

bool ConfigParser::parse(
    const std::string& content,
    Config& config,
    std::vector<Task>& tasks,
    ParseReport& report
) const {

    config = Config();

    tasks.clear();

    std::string text = content;

    // editores como o bloco de notas podem gravar a marca bom do utf-8
    // no inicio do arquivo; ela nao faz parte do conteudo e e removida
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text = text.substr(3);
    }

    std::istringstream stream(text);

    std::string line;

    int line_number = 0;

    bool system_line_read = false;

    int task_lines = 0;

    while (std::getline(stream, line)) {

        ++line_number;

        // trim tambem remove o '\r' de arquivos com quebra de linha do windows
        line = trim(line);

        // linhas vazias nao possuem informacoes uteis
        // portanto devem ser ignoradas durante a leitura (requisito 3.3.6)
        if (line.empty()) {
            continue;
        }

        // o ponto e virgula no final da linha e opcional (requisito 3.3.3)
        // por isso ele deve ser removido antes da interpretacao dos campos
        if (line.back() == ';') {
            line.pop_back();
            line = trim(line);
        }

        auto fields = split(line, ';');

        // a primeira linha com conteudo descreve o sistema; as demais, as tarefas
        // (usar a primeira linha nao vazia, e nao a linha 1 do arquivo, permite
        // aceitar arquivos que comecem com linhas em branco)
        if (!system_line_read) {
            parse_system_line(fields, line_number, config, report);
            system_line_read = true;
            continue;
        }

        ++task_lines;

        parse_task_line(fields, line_number, tasks, report);
    }

    if (!system_line_read) {
        report.errors.push_back("o arquivo está vazio: a primeira linha deve conter algoritmo;quantum;qtde_cpus");
    }
    else if (task_lines == 0) {
        report.errors.push_back("o arquivo deve possuir pelo menos uma tarefa a partir da segunda linha (id;cor;ingresso;duracao;periodo;prazo;lista_eventos)");
    }

    if (!report.ok()) {
        tasks.clear();
        return false;
    }

    // avisos sobre o conjunto de tarefas carregado
    std::set<std::string> colors;

    double utilization = 0.0;

    int periodic = 0;

    for (const Task& task : tasks) {

        if (task.ignored) {
            continue;
        }

        ++periodic;

        utilization += static_cast<double>(task.duration) / task.period;

        // o enunciado pede uma cor diferente para cada tarefa; cores repetidas
        // nao impedem a simulacao, mas dificultam a leitura do grafico
        if (!colors.insert(task.color).second) {
            report.warnings.push_back("a cor " + task.color + " é usada por mais de uma tarefa; o gráfico ficará ambíguo");
        }
    }

    if (periodic == 0) {
        report.warnings.push_back("nenhuma tarefa periódica foi encontrada: não há o que simular no projeto A");
    }
    else {

        char buffer[256];

        std::snprintf(
            buffer, sizeof(buffer),
            "utilização total U = %.3f para %d CPU(s) (U/m = %.3f); ",
            utilization, config.cpu_count, utilization / config.cpu_count
        );

        std::string message = buffer;

        if (utilization > config.cpu_count) {
            message += "U maior que o número de CPUs: perdas de prazo são inevitáveis";
        }
        else if (config.cpu_count == 1) {

            double bound = periodic * (std::pow(2.0, 1.0 / periodic) - 1.0);

            std::snprintf(buffer, sizeof(buffer), "limite de Liu & Layland para RM com %d tarefas = %.3f", periodic, bound);

            message += buffer;
        }
        else {
            message += "U não excede o número de CPUs";
        }

        report.info.push_back(message);
    }

    return true;
}

void ConfigParser::parse_system_line(
    const std::vector<std::string>& fields,
    int line_number,
    Config& config,
    ParseReport& report
) const {

    if (fields.size() > 3) {
        report.errors.push_back(
            line_prefix(line_number) + "a primeira linha deve ter no máximo 3 campos: algoritmo_escalonamento;quantum;qtde_cpus"
        );
        return;
    }

    // algoritmo de escalonamento (sem diferenciar maiusculas/minusculas, requisito 3.3.2)
    std::string algorithm = fields.empty() ? "" : to_upper(fields[0]);

    int dummy = 0;

    if (algorithm.empty()) {

        algorithm = DEFAULT_ALGORITHM;

        report.warnings.push_back(line_prefix(line_number) + "algoritmo não informado; usado o padrão " + algorithm);
    }
    else if (parse_int(algorithm, dummy)) {

        // um numero no lugar do algoritmo quase sempre indica que a linha de
        // sistema foi esquecida e o arquivo comeca direto pelas tarefas
        report.errors.push_back(
            line_prefix(line_number) + "esperado o nome do algoritmo de escalonamento, mas foi lido o número \"" +
            fields[0] + "\" (a primeira linha deve ser algoritmo;quantum;qtde_cpus)"
        );
        return;
    }

    if (SchedulerRegistry::instance().find(algorithm) == nullptr) {

        std::string available;

        for (const auto& name : SchedulerRegistry::instance().names()) {
            available += (available.empty() ? "" : ", ") + name;
        }

        report.errors.push_back(
            line_prefix(line_number) + "algoritmo de escalonamento \"" + algorithm +
            "\" desconhecido; algoritmos disponíveis: " + available
        );
    }

    config.scheduling_algorithm = algorithm;

    if (read_number(fields, 1, "quantum", DEFAULT_QUANTUM, line_number, config.quantum, report)) {

        if (config.quantum < 0) {
            report.errors.push_back(line_prefix(line_number) + "o quantum não pode ser negativo");
        }
        else if (config.quantum == 0) {
            report.warnings.push_back(line_prefix(line_number) + "quantum 0: as tarefas não terão limite de fatia de tempo");
        }
    }

    if (read_number(fields, 2, "qtde_cpus", DEFAULT_CPU_COUNT, line_number, config.cpu_count, report)) {

        if (config.cpu_count < 1 || config.cpu_count > MAX_CPUS) {
            report.errors.push_back(
                line_prefix(line_number) + "a quantidade de CPUs deve estar entre 1 e " +
                std::to_string(MAX_CPUS) + " (valor lido: " + std::to_string(config.cpu_count) + ")"
            );
        }
    }
}

void ConfigParser::parse_task_line(
    const std::vector<std::string>& fields,
    int line_number,
    std::vector<Task>& tasks,
    ParseReport& report
) const {

    std::size_t errors_before = report.errors.size();

    // id: quando ausente, o proximo id livre e sugerido
    int next_id = 1;

    for (const Task& task : tasks) {
        if (task.id >= next_id) {
            next_id = task.id + 1;
        }
    }

    int id = 0;

    if (!read_number(fields, 0, "id", next_id, line_number, id, report)) {
        return;
    }

    if (id < 0) {
        report.errors.push_back(line_prefix(line_number) + "o id da tarefa não pode ser negativo");
        return;
    }

    for (const Task& task : tasks) {
        if (task.id == id) {
            report.errors.push_back(line_prefix(line_number) + "o id " + std::to_string(id) + " já foi usado por outra tarefa (o id deve ser único)");
            return;
        }
    }

    std::string label = line_prefix(line_number) + "tarefa " + std::to_string(id) + ": ";

    // cor
    std::string color;

    if (fields.size() < 2 || fields[1].empty()) {

        color = default_color(static_cast<int>(tasks.size()));

        report.warnings.push_back(label + "cor não informada; usada a cor padrão " + color);
    }
    else {

        color = normalize_color(fields[1]);

        if (color.empty()) {
            report.errors.push_back(label + "cor \"" + fields[1] + "\" inválida; use 6 dígitos hexadecimais RRGGBB (ex.: F0E0D0)");
        }
    }

    int arrival = 0;
    int duration = 0;
    int period = 0;
    int deadline = 0;

    read_number(fields, 2, "ingresso", DEFAULT_ARRIVAL, line_number, arrival, report);
    read_number(fields, 3, "duracao", DEFAULT_DURATION, line_number, duration, report);
    bool period_ok = read_number(fields, 4, "periodo", DEFAULT_PERIOD, line_number, period, report);

    // o prazo padrao e o proprio periodo (prazo implicito), que e a
    // suposicao classica dos algoritmos rm e edf
    int default_deadline = (period_ok && period > 0) ? period : (duration > 0 ? duration : DEFAULT_DURATION);

    read_number(fields, 5, "prazo", default_deadline, line_number, deadline, report);

    if (report.errors.size() != errors_before) {
        return;
    }

    if (arrival < 0) {
        report.errors.push_back(label + "o instante de ingresso não pode ser negativo");
    }

    if (duration <= 0) {
        report.errors.push_back(label + "a duração deve ser maior que zero");
    }

    if (period < 0) {
        report.errors.push_back(label + "período negativo é inválido (use 0 para tarefa aperiódica ou um valor maior que zero)");
    }

    if (deadline <= 0) {
        report.errors.push_back(label + "o prazo deve ser maior que zero");
    }

    if (report.errors.size() != errors_before) {
        return;
    }

    Task task(id, color, arrival, duration, period, deadline);

    // todos os campos apos o prazo pertencem a lista de eventos, que pode
    // conter muitos itens (requisito 3.3.5); ela e guardada sem alteracoes
    for (std::size_t index = 6; index < fields.size(); ++index) {

        if (!task.event_list.empty()) {
            task.event_list += ";";
        }

        task.event_list += fields[index];
    }

    task.event_list = trim(task.event_list);

    if (deadline < duration) {
        report.warnings.push_back(label + "o prazo (" + std::to_string(deadline) + ") é menor que a duração (" + std::to_string(duration) + "): todas as ativações perderão o prazo");
    }

    // no projeto a somente tarefas periodicas sao simuladas
    // tarefas aperiodicas serao tratadas no projeto b (requisito 4.4)
    if (task.is_aperiodic()) {

        task.ignored = true;

        task.reset_runtime();

        report.warnings.push_back(label + "é aperiódica (período 0) e será IGNORADA: o projeto A simula apenas tarefas periódicas");
    }

    tasks.push_back(task);
}
