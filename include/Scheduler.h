#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "SchedulerPlugin.h"

#include <memory>
#include <string>
#include <vector>

// interface de um algoritmo de escalonamento
//
// decisao de projeto: o algoritmo apenas informa a prioridade de cada
// tarefa (uma chave numerica, menor = mais prioritaria). todo o restante
// (fila global, varias cpus, preempcao, desempates do requisito 4.3,
// registro de eventos) fica no simulador. assim:
//   - novos algoritmos sao triviais de escrever (uma unica funcao);
//   - os desempates sao garantidamente iguais para todos os algoritmos;
//   - o codigo da simulacao nao muda quando um algoritmo e adicionado.
class Scheduler {

public:

    virtual ~Scheduler() = default;

    // nome usado no arquivo de configuracao (sempre em maiusculas)
    virtual std::string name() const = 0;

    // descricao curta exibida ao usuario
    virtual std::string description() const = 0;

    // chave de prioridade: quanto menor, maior a prioridade
    virtual long long priority(const SchedTaskInfo& task, int now) const = 0;
};

// catalogo de algoritmos disponiveis
//
// os algoritmos embutidos (rm e edf) sao registrados no construtor e os
// externos sao carregados de dlls; o simulador procura o algoritmo apenas
// pelo nome, sem saber se ele e embutido ou externo
class SchedulerRegistry {

public:

    static SchedulerRegistry& instance();

    // registra um algoritmo; falha se ja existir outro com o mesmo nome
    bool add(std::unique_ptr<Scheduler> scheduler);

    // procura pelo nome sem diferenciar maiusculas/minusculas
    const Scheduler* find(const std::string& name) const;

    // nomes de todos os algoritmos registrados
    std::vector<std::string> names() const;

    // carrega todas as dlls da pasta informada; mensagens de sucesso ou de
    // erro de cada arquivo sao devolvidas para exibicao ao usuario
    void load_plugins(const std::wstring& directory, std::vector<std::string>& messages);

private:

    SchedulerRegistry();

    std::vector<std::unique_ptr<Scheduler>> schedulers_;
};

#endif
