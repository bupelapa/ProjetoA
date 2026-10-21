#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "Config.h"
#include "Task.h"

#include <string>
#include <vector>

// resultado da leitura do arquivo de configuracao
//
// em vez de imprimir mensagens diretamente (o que so funcionaria em modo
// console), o parser acumula todas as mensagens para que a interface
// grafica possa mostra-las ao usuario de uma so vez (requisito 3.3.6)
struct ParseReport {

    // problemas que impedem a simulacao (o arquivo nao e aceito)
    std::vector<std::string> errors;

    // situacoes aceitas, mas que o usuario precisa saber
    // (ex.: tarefa aperiodica ignorada, valor padrao aplicado)
    std::vector<std::string> warnings;

    // informacoes uteis sobre o conjunto de tarefas (ex.: utilizacao)
    std::vector<std::string> info;

    bool ok() const {
        return errors.empty();
    }
};

class ConfigParser {

public:

    // le o arquivo indicado (qualquer nome e qualquer local; caminho em utf-8)
    bool load(
        const std::string& filename,
        Config& config,
        std::vector<Task>& tasks,
        ParseReport& report
    ) const;

    // interpreta o conteudo ja lido; separado de load para facilitar testes
    bool parse(
        const std::string& content,
        Config& config,
        std::vector<Task>& tasks,
        ParseReport& report
    ) const;

private:

    void parse_system_line(
        const std::vector<std::string>& fields,
        int line_number,
        Config& config,
        ParseReport& report
    ) const;

    void parse_task_line(
        const std::vector<std::string>& fields,
        int line_number,
        std::vector<Task>& tasks,
        ParseReport& report
    ) const;
};

#endif
