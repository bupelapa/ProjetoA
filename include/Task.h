#ifndef TASK_H
#define TASK_H

#include <string>
#include <vector>

// estados possiveis de uma tarefa durante a simulacao
enum class TaskState {

    // a tarefa ainda nao ingressou no sistema (instante de ingresso no futuro)
    NotArrived,

    // possui uma ativacao pendente e esta na fila global de prontas
    Ready,

    // esta ocupando um processador
    Running,

    // foi suspensa (no projeto a, apenas por intervencao manual do usuario;
    // no projeto b tambem por mutex, entrada/saida etc.)
    Suspended,

    // terminou a ativacao atual e aguarda o inicio do proximo periodo
    Waiting,

    // executou todas as ativacoes (ou foi encerrada pelo usuario)
    Terminated,

    // tarefa aperiodica: ignorada no projeto a (requisito 4.4)
    Ignored
};

// nome do estado em portugues (utf-8) para exibicao ao usuario
const char* task_state_name(TaskState state);

// task control block (tcb): todas as informacoes de uma tarefa, antes,
// durante e depois da simulacao, ficam nesta unica estrutura (requisito 1.3)
//
// como cada passo da simulacao guarda uma copia de todos os tcbs,
// a estrutura contem apenas valores simples, o que torna a copia barata
// e permite retroceder a simulacao restaurando a copia anterior
struct Task {

    // ---------------------------------------------------------------
    // parametros lidos do arquivo de configuracao
    // ---------------------------------------------------------------

    int id;

    // cor no formato rrggbb (hexadecimal)
    std::string color;

    // instante em que a tarefa e criada (primeira ativacao)
    int arrival_time;

    // tempo de execucao de cada ativacao (em ticks)
    int duration;

    // periodo de ativacao; 0 indica tarefa aperiodica
    int period;

    // prazo relativo: contado a partir de cada ativacao
    int deadline;

    // lista de eventos crua (sera interpretada no projeto b); e mantida
    // como texto para nao perder nenhuma informacao do arquivo
    std::string event_list;

    // ---------------------------------------------------------------
    // estado dinamico (atualizado a cada tick)
    // ---------------------------------------------------------------

    TaskState state;

    // processador em uso (-1 quando nao esta executando)
    int cpu;

    // ultimo processador usado; o escalonador tenta devolver a tarefa
    // para a mesma cpu, o que deixa o grafico mais facil de acompanhar
    int last_cpu;

    // quantidade de ativacoes ja liberadas (0 ate MAX_ACTIVATIONS)
    int activations;

    // quantidade de ativacoes concluidas
    int executions_completed;

    // tempo que falta para concluir a ativacao atual (0 = sem ativacao)
    int remaining;

    // instante de liberacao e prazo absoluto da ativacao atual
    int job_release;
    int job_deadline;

    // indica se a ativacao atual ja perdeu o prazo (evita contar duas vezes)
    bool job_missed;

    // instante da proxima ativacao periodica
    int next_release;

    // instante da ultima ativacao liberada (usado quando o periodo e editado)
    int last_release;

    // ativacoes liberadas enquanto a anterior ainda nao havia terminado;
    // elas aguardam em ordem e comecam assim que a atual terminar
    std::vector<int> pending_releases;

    // ticks executados desde que a tarefa recebeu o processador
    int quantum_used;

    // estatisticas acumuladas
    int total_executed;
    int total_ready_time;
    int deadline_misses;
    int finish_time;

    // marcada pelo parser para tarefas aperiodicas (ignoradas no projeto a)
    bool ignored;

    Task();

    Task(
        int id,
        const std::string& color,
        int arrival_time,
        int duration,
        int period,
        int deadline
    );

    bool is_periodic() const;

    bool is_aperiodic() const;

    // a tarefa possui uma ativacao em andamento (pronta, executando ou suspensa)
    bool has_job() const;

    // quanto a ativacao atual ja executou
    int executed_in_job() const;

    // coloca o tcb no estado inicial da simulacao, preservando os parametros
    void reset_runtime();

    // descricao textual completa do tcb (usada no modo depuracao e no console)
    std::string describe() const;
};

#endif
