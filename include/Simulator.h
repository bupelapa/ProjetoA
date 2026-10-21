#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "Config.h"
#include "Task.h"

#include <string>
#include <vector>

// tipos de evento relevantes que acontecem durante a simulacao;
// cada tipo possui um simbolo grafico proprio no grafico de gantt
enum class EventType {
    Arrival,          // a tarefa ingressou no sistema (primeira ativacao)
    Activation,       // nova ativacao periodica
    ActivationQueued, // ativacao liberada antes da anterior terminar
    Deadline,         // prazo absoluto de uma ativacao
    DeadlineMiss,     // a ativacao ultrapassou o prazo
    JobEnd,           // uma ativacao terminou de executar
    TaskEnd,          // a tarefa terminou (10 ativacoes ou encerramento manual)
    Preemption,       // a tarefa perdeu o processador
    QuantumExpired,   // a fatia de tempo da tarefa acabou
    RandomTie,        // o desempate precisou de sorteio (requisito 4.3 item 5)
    CpuOff,           // processador desligado por falta de tarefas prontas
    CpuOn,            // processador religado
    Manual            // alteracao feita pelo usuario durante a simulacao
};

const char* event_type_name(EventType type);

// registro de um evento
struct SimEvent {

    // passo da simulacao que gerou o evento (usado para descartar eventos
    // de um "futuro" que deixou de existir apos uma edicao manual)
    int step;

    // instante (tick) em que o evento acontece
    int time;

    EventType type;

    // tarefa e cpu envolvidas (-1 quando nao se aplica)
    int task_id;
    int cpu;

    std::string text;
};

// situacao de uma tarefa durante um tick, usada para desenhar o gantt
struct TickCell {
    int task_id;
    TaskState state;
    int cpu;
};

// tudo que aconteceu no intervalo [time, time + 1)
struct TickRecord {
    int time;
    std::vector<TickCell> cells;
    std::vector<int> cpu_task;  // tarefa em cada cpu (-1 = desligada)
};

// estado completo do sistema em um instante
//
// uma copia deste estado e guardada a cada passo; retroceder a simulacao
// e simplesmente voltar para a copia anterior (requisito 1.5.2)
struct SimState {

    // relogio global do sistema
    int time = 0;

    std::string algorithm;
    int quantum = DEFAULT_QUANTUM;
    int cpu_count = DEFAULT_CPU_COUNT;

    // tcb de todas as tarefas
    std::vector<Task> tasks;

    // tarefa em cada processador (-1 = processador desligado)
    std::vector<int> cpu_task;

    // instante em que cada cpu foi desligada (-1 = ligada) e o total
    // acumulado de ticks desligada (informado ao usuario, requisito 1.2)
    std::vector<int> cpu_off_since;
    std::vector<int> cpu_off_total;

    // estado do gerador de numeros aleatorios dos sorteios; fica no estado
    // para que avancar novamente depois de retroceder gere o mesmo resultado
    unsigned long long rng = 0x9E3779B97F4A7C15ULL;

    // eventos de edicoes manuais feitas neste instante; eles entram no
    // historico quando o proximo passo for executado
    std::vector<SimEvent> pending_events;

    Task* find(int id);
    const Task* find(int id) const;

    // todas as tarefas terminaram (ou foram ignoradas)
    bool finished() const;

    // ticks que a cpu passou desligada ate o instante atual
    int cpu_off_time(int cpu) const;
};

// alteracoes solicitadas pelo usuario para uma tarefa
struct TaskEdit {

    enum class Action { Keep, Suspend, Resume, Terminate };

    std::string color;
    int arrival_time = 0;
    int duration = 0;
    int period = 0;
    int deadline = 0;
    int remaining = 0;
    Action action = Action::Keep;
};

// motor da simulacao
//
// o simulador e independente da interface grafica: a janela apenas chama
// estes metodos e desenha o resultado. isso permite usar o mesmo motor no
// modo grafico (passo a passo) e no modo de linha de comando (execucao completa)
class Simulator {

public:

    // prepara uma nova simulacao em t = 0
    void load(const Config& config, const std::vector<Task>& tasks);

    // estado no instante atualmente exibido
    const SimState& state() const;

    // posicao no historico (coincide com o relogio global)
    int position() const;

    // quantidade de passos ja calculados e guardados
    int history_size() const;

    bool finished() const;

    // ---------------------------------------------------------------
    // controle da execucao
    // ---------------------------------------------------------------

    // avanca um tick; se o passo ja foi calculado antes, ele e apenas
    // restaurado do historico (o mesmo resultado e garantido)
    bool step_forward(std::string& message);

    // volta um tick (o relogio global tambem volta)
    bool step_back();

    // volta para t = 0
    void go_to_start();

    // vai para um instante qualquer, calculando os passos que faltarem
    bool go_to(int time, std::string& message);

    // executa ate o fim sem intervencao (modo de execucao completa)
    bool run_to_end(std::string& message);

    // ---------------------------------------------------------------
    // alteracoes manuais (requisitos 1.5.2, 3.1 e 3.4)
    // todas devolvem texto vazio em caso de sucesso ou o motivo da recusa
    // ---------------------------------------------------------------

    std::string edit_task(int id, const TaskEdit& edit);

    std::string add_task(const Task& task);

    std::string set_system(const std::string& algorithm, int quantum, int cpu_count);

    // ---------------------------------------------------------------
    // consultas para a interface
    // ---------------------------------------------------------------

    // eventos ja ocorridos ate o instante atual (prefixo de all_events)
    std::size_t visible_event_count() const;

    const std::vector<SimEvent>& all_events() const;

    // registros dos ticks; somente os de indice menor que position()
    // pertencem ao passado do instante exibido
    const std::vector<TickRecord>& ticks() const;

    // fila de prontas ordenada pela prioridade do algoritmo atual
    std::vector<int> ready_queue() const;

    // maior quantidade de cpus usada ate agora (define as faixas do gantt)
    int max_cpu_count() const;

    int total_deadline_misses() const;

private:

    // calcula o estado do proximo tick a partir do estado atual
    SimState compute_next(const SimState& current, std::vector<SimEvent>& events, TickRecord& record) const;

    // ordena as tarefas candidatas conforme o algoritmo e os desempates
    std::vector<int> rank_candidates(
        const SimState& state,
        const std::vector<char>& quantum_expired,
        unsigned long long& rng,
        std::vector<char>* random_tie,
        bool include_running
    ) const;

    // descarta os passos posteriores ao instante atual; chamado quando o
    // usuario altera algo, pois o futuro calculado deixa de ser valido
    void discard_future();

    // retorna se ainda existe alguma tarefa capaz de progredir
    bool can_progress(std::string& reason) const;

    std::vector<SimState> history_;
    int position_ = 0;

    std::vector<SimEvent> events_;
    std::vector<TickRecord> ticks_;
};

#endif
