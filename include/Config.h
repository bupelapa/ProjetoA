#ifndef CONFIG_H
#define CONFIG_H

#include <string>

// quantidade maxima de processadores aceita pelo simulador (o "n" do enunciado)
// o limite existe apenas para manter o grafico de gantt legivel, ja que
// cada cpu ganha uma faixa propria no grafico
constexpr int MAX_CPUS = 64;

// o enunciado define que, para a simulacao terminar, uma tarefa periodica
// e considerada encerrada apos ter executado 10 vezes (requisito 4.4)
constexpr int MAX_ACTIVATIONS = 10;

// valores padrao sugeridos ao usuario (requisito 3.2)
// eles sao usados quando um campo do arquivo esta vazio/ausente e
// tambem aparecem preenchidos nas telas de configuracao
constexpr const char* DEFAULT_ALGORITHM = "RM";
constexpr int DEFAULT_QUANTUM = 1;
constexpr int DEFAULT_CPU_COUNT = 1;
constexpr int DEFAULT_ARRIVAL = 0;
constexpr int DEFAULT_DURATION = 1;
constexpr int DEFAULT_PERIOD = 10;

// parametros gerais do sistema simulado (primeira linha do arquivo)
struct Config {

    // nome do algoritmo de escalonamento, sempre em maiusculas
    std::string scheduling_algorithm;

    // fatia maxima de tempo que uma tarefa executa antes do escalonador
    // ser acionado novamente; o valor 0 significa "sem limite"
    int quantum;

    // quantidade de processadores do sistema (1 ate MAX_CPUS)
    int cpu_count;

    Config()
        : scheduling_algorithm(DEFAULT_ALGORITHM),
          quantum(DEFAULT_QUANTUM),
          cpu_count(DEFAULT_CPU_COUNT)
    {
    }
};

#endif
