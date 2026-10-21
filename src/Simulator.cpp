#include "Simulator.h"

#include "Scheduler.h"
#include "Util.h"

#include <algorithm>
#include <tuple>

namespace {

std::string task_label(int id) {
    return "T" + std::to_string(id);
}

// gerador xorshift64*: simples, rapido e totalmente determinado pelo
// estado guardado em SimState, o que torna os sorteios reproduziveis
unsigned long long next_random(unsigned long long& state) {

    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;

    return state * 2685821657736338717ULL;
}

SchedTaskInfo make_info(const Task& task) {

    SchedTaskInfo info;

    info.id = task.id;
    info.arrival_time = task.arrival_time;
    info.duration = task.duration;
    info.period = task.period;
    info.deadline = task.deadline;
    info.job_release = task.job_release;
    info.job_deadline = task.job_deadline;
    info.remaining = task.remaining;
    info.executed_in_job = task.executed_in_job();

    return info;
}

// libera o processador ocupado pela tarefa (se houver)
void release_cpu(SimState& state, Task& task) {

    if (task.cpu >= 0 && task.cpu < static_cast<int>(state.cpu_task.size()) && state.cpu_task[task.cpu] == task.id) {
        state.cpu_task[task.cpu] = -1;
    }

    if (task.cpu >= 0) {
        task.last_cpu = task.cpu;
    }

    task.cpu = -1;
    task.quantum_used = 0;
}

}

const char* event_type_name(EventType type) {

    switch (type) {
        case EventType::Arrival:          return "Chegada";
        case EventType::Activation:       return "Ativação";
        case EventType::ActivationQueued: return "Ativação enfileirada";
        case EventType::Deadline:         return "Prazo";
        case EventType::DeadlineMiss:     return "PERDA DE PRAZO";
        case EventType::JobEnd:           return "Fim da ativação";
        case EventType::TaskEnd:          return "Término da tarefa";
        case EventType::Preemption:       return "Preempção";
        case EventType::QuantumExpired:   return "Quantum esgotado";
        case EventType::RandomTie:        return "Sorteio";
        case EventType::CpuOff:           return "CPU desligada";
        case EventType::CpuOn:            return "CPU ligada";
        case EventType::Manual:           return "Alteração manual";
    }

    return "?";
}

// ---------------------------------------------------------------------------
// SimState
// ---------------------------------------------------------------------------

Task* SimState::find(int id) {

    for (Task& task : tasks) {
        if (task.id == id) {
            return &task;
        }
    }

    return nullptr;
}

const Task* SimState::find(int id) const {

    for (const Task& task : tasks) {
        if (task.id == id) {
            return &task;
        }
    }

    return nullptr;
}

bool SimState::finished() const {

    for (const Task& task : tasks) {
        if (task.state != TaskState::Ignored && task.state != TaskState::Terminated) {
            return false;
        }
    }

    return true;
}

int SimState::cpu_off_time(int cpu) const {

    if (cpu < 0 || cpu >= static_cast<int>(cpu_off_total.size())) {
        return 0;
    }

    int total = cpu_off_total[cpu];

    if (cpu_off_since[cpu] >= 0) {
        total += time - cpu_off_since[cpu];
    }

    return total;
}

// ---------------------------------------------------------------------------
// carga e navegacao no historico
// ---------------------------------------------------------------------------

void Simulator::load(const Config& config, const std::vector<Task>& tasks) {

    SimState state;

    state.time = 0;
    state.algorithm = to_upper(config.scheduling_algorithm);
    state.quantum = config.quantum;
    state.cpu_count = config.cpu_count;
    state.tasks = tasks;

    for (Task& task : state.tasks) {
        task.reset_runtime();
    }

    // manter as tarefas ordenadas por id deixa a tabela e o grafico estaveis
    std::sort(state.tasks.begin(), state.tasks.end(), [](const Task& a, const Task& b) {
        return a.id < b.id;
    });

    state.cpu_task.assign(state.cpu_count, -1);
    state.cpu_off_since.assign(state.cpu_count, -1);
    state.cpu_off_total.assign(state.cpu_count, 0);

    history_.clear();
    history_.push_back(state);
    position_ = 0;
    events_.clear();
    ticks_.clear();
}

const SimState& Simulator::state() const {
    return history_[position_];
}

int Simulator::position() const {
    return position_;
}

int Simulator::history_size() const {
    return static_cast<int>(history_.size());
}

bool Simulator::finished() const {
    return history_.empty() || state().finished();
}

bool Simulator::step_forward(std::string& message) {

    if (history_.empty()) {
        message = "Nenhuma simulação carregada.";
        return false;
    }

    if (finished()) {
        message = "A simulação já terminou em t = " + std::to_string(state().time) + ": todas as tarefas concluíram suas execuções.";
        return false;
    }

    // passo ja calculado anteriormente: basta avancar no historico
    if (position_ + 1 < static_cast<int>(history_.size())) {
        ++position_;
        return true;
    }

    std::vector<SimEvent> new_events;

    TickRecord record;

    SimState next = compute_next(history_[position_], new_events, record);

    events_.insert(events_.end(), new_events.begin(), new_events.end());
    ticks_.push_back(record);
    history_.push_back(next);

    ++position_;

    return true;
}

bool Simulator::step_back() {

    if (position_ == 0) {
        return false;
    }

    --position_;

    return true;
}

void Simulator::go_to_start() {
    position_ = 0;
}

bool Simulator::go_to(int time, std::string& message) {

    if (time < 0) {
        message = "O instante deve ser maior ou igual a zero.";
        return false;
    }

    if (time < static_cast<int>(history_.size())) {
        position_ = time;
        return true;
    }

    while (position_ < time) {

        if (!step_forward(message)) {
            message = "A simulação parou em t = " + std::to_string(position_) + ". " + message;
            return false;
        }
    }

    return true;
}

bool Simulator::can_progress(std::string& reason) const {

    const SimState& current = state();

    for (const Task& task : current.tasks) {

        if (task.state == TaskState::Ignored || task.state == TaskState::Terminated) {
            continue;
        }

        if (task.state != TaskState::Suspended) {
            return true;
        }
    }

    reason = "Todas as tarefas que ainda não terminaram estão suspensas: a simulação nunca terminaria. "
             "Retome alguma tarefa (Editar tarefa → Retomar) ou encerre-as.";

    return false;
}

bool Simulator::run_to_end(std::string& message) {

    while (!finished()) {

        // sem esta verificacao, tarefas suspensas para sempre fariam a
        // execucao completa entrar em um laco infinito
        if (!can_progress(message)) {
            return false;
        }

        if (!step_forward(message)) {
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// escalonamento
// ---------------------------------------------------------------------------

std::vector<int> Simulator::rank_candidates(
    const SimState& state,
    const std::vector<char>& quantum_expired,
    unsigned long long& rng,
    std::vector<char>* random_tie,
    bool include_running
) const {

    const Scheduler* scheduler = SchedulerRegistry::instance().find(state.algorithm);

    // chave de ordenacao de cada candidata, na ordem dos criterios:
    // prioridade do algoritmo, depois os desempates do requisito 4.3
    struct Candidate {
        int index;
        long long priority;   // definida pelo algoritmo (menor = melhor)
        int not_running;      // (1) quem ja estava executando vence
        int job_deadline;     // (2) menor prazo
        int arrival;          // (3) chegou antes
        int duration;         // (4) menor duracao
        unsigned long long draw;  // (5) sorteio
        int id;               // garante uma ordem total
    };

    std::vector<Candidate> candidates;

    for (std::size_t index = 0; index < state.tasks.size(); ++index) {

        const Task& task = state.tasks[index];

        bool running = (task.state == TaskState::Running);

        if (task.state != TaskState::Ready && !(include_running && running)) {
            continue;
        }

        bool expired = index < quantum_expired.size() && quantum_expired[index];

        Candidate candidate;

        candidate.index = static_cast<int>(index);
        candidate.priority = scheduler != nullptr ? scheduler->priority(make_info(task), state.time) : 0;

        // uma tarefa cujo quantum acabou volta para a fila de prontas e,
        // por isso, perde a vantagem do criterio (1); sem isso o quantum
        // nunca faria efeito, pois a tarefa sempre venceria o empate
        candidate.not_running = (running && !expired) ? 0 : 1;

        candidate.job_deadline = task.job_deadline;
        candidate.arrival = task.arrival_time;
        candidate.duration = task.duration;
        candidate.draw = next_random(rng);
        candidate.id = task.id;

        candidates.push_back(candidate);
    }

    auto key = [](const Candidate& c) {
        return std::make_tuple(c.priority, c.not_running, c.job_deadline, c.arrival, c.duration);
    };

    std::sort(candidates.begin(), candidates.end(), [&](const Candidate& a, const Candidate& b) {

        auto key_a = key(a);
        auto key_b = key(b);

        if (key_a != key_b) {
            return key_a < key_b;
        }

        if (a.draw != b.draw) {
            return a.draw < b.draw;
        }

        return a.id < b.id;
    });

    // o sorteio so "decide" algo quando duas tarefas empatadas em todos os
    // criterios ficam uma de cada lado da fronteira entre as que recebem
    // cpu e as que ficam na fila; somente nesse caso o evento e registrado
    if (random_tie != nullptr) {

        random_tie->assign(state.tasks.size(), 0);

        int cpus = state.cpu_count;

        if (static_cast<int>(candidates.size()) > cpus) {

            const Candidate& first_left_out = candidates[cpus];

            for (int position = 0; position < cpus; ++position) {
                if (key(candidates[position]) == key(first_left_out)) {
                    (*random_tie)[candidates[position].index] = 1;
                }
            }
        }
    }

    std::vector<int> order;

    for (const Candidate& candidate : candidates) {
        order.push_back(candidate.index);
    }

    return order;
}

std::vector<int> Simulator::ready_queue() const {

    if (history_.empty()) {
        return {};
    }

    // copia do gerador: consultar a fila nao pode alterar a simulacao
    unsigned long long rng = state().rng;

    std::vector<int> ids;

    for (int index : rank_candidates(state(), {}, rng, nullptr, false)) {
        ids.push_back(state().tasks[index].id);
    }

    return ids;
}

// ---------------------------------------------------------------------------
// um tick de simulacao
// ---------------------------------------------------------------------------

SimState Simulator::compute_next(const SimState& current, std::vector<SimEvent>& events, TickRecord& record) const {

    SimState state = current;

    const int now = state.time;

    auto emit = [&](EventType type, int time, int task_id, int cpu, const std::string& text) {
        events.push_back(SimEvent{now, time, type, task_id, cpu, text});
    };

    // eventos das edicoes manuais feitas neste instante
    for (SimEvent event : state.pending_events) {
        event.step = now;
        events.push_back(event);
    }

    state.pending_events.clear();

    // reconstroi o mapa cpu -> tarefa a partir dos tcbs; isso mantem os dois
    // sempre coerentes, mesmo depois de edicoes manuais
    state.cpu_task.assign(state.cpu_count, -1);

    for (Task& task : state.tasks) {

        if (task.state == TaskState::Running && task.cpu >= 0 && task.cpu < state.cpu_count && state.cpu_task[task.cpu] == -1) {
            state.cpu_task[task.cpu] = task.id;
        }
        else if (task.state == TaskState::Running) {
            task.state = TaskState::Ready;
            release_cpu(state, task);
        }
    }

    // inicia uma ativacao: define o tempo restante e o prazo absoluto
    auto start_job = [&](Task& task, int release) {

        task.remaining = task.duration;
        task.job_release = release;
        task.job_deadline = release + task.deadline;
        task.job_missed = false;

        emit(EventType::Deadline, task.job_deadline, task.id, -1,
             "prazo da ativação " + std::to_string(task.executions_completed + 1) + " de " + task_label(task.id) +
             " em t=" + std::to_string(task.job_deadline));
    };

    // -----------------------------------------------------------------
    // 1) ativacoes periodicas (chegada de novas ativacoes)
    // -----------------------------------------------------------------
    for (Task& task : state.tasks) {

        if (task.state == TaskState::Ignored || task.state == TaskState::Terminated) {
            continue;
        }

        if (task.activations >= MAX_ACTIVATIONS || task.next_release > now) {
            continue;
        }

        task.activations++;
        task.last_release = now;
        task.next_release = now + task.period;

        std::string count = std::to_string(task.activations) + "/" + std::to_string(MAX_ACTIVATIONS);

        if (task.activations == 1) {
            emit(EventType::Arrival, now, task.id, -1, task_label(task.id) + " ingressou no sistema (ativação " + count + ")");
        }

        if (task.has_job()) {

            // a ativacao anterior ainda nao terminou: a nova aguarda a vez
            task.pending_releases.push_back(now);

            emit(EventType::ActivationQueued, now, task.id, -1,
                 "ativação " + count + " de " + task_label(task.id) + " enfileirada (a ativação anterior ainda não terminou)");
        }
        else {

            if (task.activations > 1) {
                emit(EventType::Activation, now, task.id, -1, "ativação " + count + " de " + task_label(task.id));
            }

            start_job(task, now);

            // uma tarefa suspensa recebe a ativacao, mas continua suspensa
            if (task.state != TaskState::Suspended) {
                task.state = TaskState::Ready;
            }
        }
    }

    // -----------------------------------------------------------------
    // 2) verificacao de prazos
    // -----------------------------------------------------------------
    for (Task& task : state.tasks) {

        if (!task.has_job() || task.job_missed || now < task.job_deadline) {
            continue;
        }

        // a tarefa continua executando ate completar a duracao; o erro
        // apenas e registrado e mostrado no grafico
        task.job_missed = true;
        task.deadline_misses++;

        emit(EventType::DeadlineMiss, now, task.id, task.cpu,
             task_label(task.id) + " perdeu o prazo (t=" + std::to_string(task.job_deadline) +
             ") faltando " + std::to_string(task.remaining) + " tick(s)");
    }

    // -----------------------------------------------------------------
    // 3) fim do quantum
    // -----------------------------------------------------------------
    std::vector<char> expired(state.tasks.size(), 0);

    if (state.quantum > 0) {

        for (std::size_t index = 0; index < state.tasks.size(); ++index) {

            Task& task = state.tasks[index];

            if (task.state == TaskState::Running && task.quantum_used >= state.quantum) {

                expired[index] = 1;

                emit(EventType::QuantumExpired, now, task.id, task.cpu,
                     "quantum de " + task_label(task.id) + " esgotado na CPU " + std::to_string(task.cpu));
            }
        }
    }

    // -----------------------------------------------------------------
    // 4) escalonamento global: as "cpu_count" tarefas mais prioritarias
    //    da fila unica executam neste tick (requisito 1.2)
    // -----------------------------------------------------------------
    std::vector<char> random_tie;

    std::vector<int> order = rank_candidates(state, expired, state.rng, &random_tie, true);

    std::vector<char> selected(state.tasks.size(), 0);

    for (int position = 0; position < static_cast<int>(order.size()) && position < state.cpu_count; ++position) {
        selected[order[position]] = 1;
    }

    // tarefas que estavam executando e nao foram escolhidas sao preemptadas
    for (std::size_t index = 0; index < state.tasks.size(); ++index) {

        Task& task = state.tasks[index];

        if (task.state != TaskState::Running) {
            continue;
        }

        if (selected[index]) {

            // a tarefa continua na mesma cpu; se o quantum acabou e ela
            // venceu de novo, um novo quantum comeca
            if (expired[index]) {
                task.quantum_used = 0;
            }

            continue;
        }

        int cpu = task.cpu;

        release_cpu(state, task);

        task.state = TaskState::Ready;

        emit(EventType::Preemption, now, task.id, cpu,
             task_label(task.id) + " preemptada na CPU " + std::to_string(cpu) +
             (expired[index] ? " (fim do quantum)" : " (tarefa mais prioritária chegou)"));
    }

    // as tarefas escolhidas que ainda nao executavam recebem uma cpu livre;
    // a cpu usada anteriormente pela tarefa tem preferencia
    for (int index : order) {

        if (!selected[index]) {
            continue;
        }

        Task& task = state.tasks[index];

        if (task.state == TaskState::Running) {
            continue;
        }

        int chosen = -1;

        if (task.last_cpu >= 0 && task.last_cpu < state.cpu_count && state.cpu_task[task.last_cpu] == -1) {
            chosen = task.last_cpu;
        }

        for (int cpu = 0; chosen < 0 && cpu < state.cpu_count; ++cpu) {
            if (state.cpu_task[cpu] == -1) {
                chosen = cpu;
            }
        }

        // nunca deveria acontecer (ha no maximo cpu_count escolhidas),
        // mas protege contra um indice invalido
        if (chosen < 0) {
            continue;
        }

        state.cpu_task[chosen] = task.id;
        task.cpu = chosen;
        task.state = TaskState::Running;
        task.quantum_used = 0;

        if (random_tie[index]) {
            emit(EventType::RandomTie, now, task.id, chosen,
                 task_label(task.id) + " escolhida por sorteio (empate em todos os critérios do requisito 4.3)");
        }
    }

    // -----------------------------------------------------------------
    // 5) processadores sem tarefa sao desligados (requisito 1.2)
    // -----------------------------------------------------------------
    for (int cpu = 0; cpu < state.cpu_count; ++cpu) {

        if (state.cpu_task[cpu] == -1) {

            if (state.cpu_off_since[cpu] < 0) {
                state.cpu_off_since[cpu] = now;
                emit(EventType::CpuOff, now, -1, cpu, "CPU " + std::to_string(cpu) + " desligada (nenhuma tarefa pronta)");
            }
        }
        else if (state.cpu_off_since[cpu] >= 0) {

            state.cpu_off_total[cpu] += now - state.cpu_off_since[cpu];
            state.cpu_off_since[cpu] = -1;

            emit(EventType::CpuOn, now, -1, cpu, "CPU " + std::to_string(cpu) + " religada");
        }
    }

    // -----------------------------------------------------------------
    // 6) registro do tick para o gantt e execucao propriamente dita
    // -----------------------------------------------------------------
    record.time = now;
    record.cpu_task = state.cpu_task;

    for (Task& task : state.tasks) {

        if (task.state == TaskState::Running) {

            task.remaining--;
            task.total_executed++;
            task.quantum_used++;

            record.cells.push_back(TickCell{task.id, TaskState::Running, task.cpu});
        }
        else if (task.state == TaskState::Ready) {

            task.total_ready_time++;

            record.cells.push_back(TickCell{task.id, TaskState::Ready, -1});
        }
        else if (task.state == TaskState::Suspended) {
            record.cells.push_back(TickCell{task.id, TaskState::Suspended, -1});
        }
    }

    // avanca o relogio global
    state.time = now + 1;

    // -----------------------------------------------------------------
    // 7) ativacoes concluidas ao final do tick
    // -----------------------------------------------------------------
    for (Task& task : state.tasks) {

        if (task.state != TaskState::Running || task.remaining > 0) {
            continue;
        }

        int cpu = task.cpu;

        task.executions_completed++;

        emit(EventType::JobEnd, state.time, task.id, cpu,
             "ativação " + std::to_string(task.executions_completed) + "/" + std::to_string(MAX_ACTIVATIONS) +
             " de " + task_label(task.id) + " concluída" + (task.job_missed ? " (com atraso)" : " dentro do prazo"));

        release_cpu(state, task);

        if (task.executions_completed >= MAX_ACTIVATIONS) {

            task.state = TaskState::Terminated;
            task.finish_time = state.time;

            emit(EventType::TaskEnd, state.time, task.id, cpu,
                 task_label(task.id) + " terminou (executou " + std::to_string(MAX_ACTIVATIONS) + " vezes)");
        }
        else if (!task.pending_releases.empty()) {

            int release = task.pending_releases.front();

            task.pending_releases.erase(task.pending_releases.begin());

            start_job(task, release);

            task.state = TaskState::Ready;
        }
        else {
            task.state = TaskState::Waiting;
        }
    }

    return state;
}

// ---------------------------------------------------------------------------
// alteracoes manuais
// ---------------------------------------------------------------------------

void Simulator::discard_future() {

    history_.resize(position_ + 1);
    ticks_.resize(position_);

    events_.erase(
        std::remove_if(events_.begin(), events_.end(), [&](const SimEvent& event) {
            return event.step >= position_;
        }),
        events_.end()
    );
}

std::string Simulator::edit_task(int id, const TaskEdit& edit) {

    if (history_.empty()) {
        return "Nenhuma simulação carregada.";
    }

    // as alteracoes sao feitas em uma copia; somente se todas forem validas
    // a copia substitui o estado atual (nada e aplicado pela metade)
    SimState next = state();

    Task* task = next.find(id);

    if (task == nullptr) {
        return "A tarefa " + std::to_string(id) + " não existe.";
    }

    if (task->state == TaskState::Ignored) {
        return "A tarefa " + task_label(id) + " é aperiódica e foi ignorada: o projeto A simula apenas tarefas periódicas.";
    }

    const Task original = *task;

    const int now = next.time;

    std::vector<std::string> changes;

    std::string color = normalize_color(edit.color);

    if (color.empty()) {
        return "Cor inválida: use 6 dígitos hexadecimais RRGGBB (ex.: F0E0D0).";
    }

    if (color != original.color) {
        task->color = color;
        changes.push_back("cor " + color);
    }

    bool parameters_changed =
        edit.arrival_time != original.arrival_time ||
        edit.duration != original.duration ||
        edit.period != original.period ||
        edit.deadline != original.deadline ||
        edit.remaining != original.remaining;

    if (original.state == TaskState::Terminated && (parameters_changed || edit.action != TaskEdit::Action::Keep)) {
        return "A tarefa " + task_label(id) + " já terminou: apenas a cor ainda pode ser alterada.";
    }

    // instante de ingresso
    if (edit.arrival_time != original.arrival_time) {

        if (original.activations > 0) {
            return "A tarefa " + task_label(id) + " já ingressou no sistema em t=" + std::to_string(original.arrival_time) +
                   "; o instante de ingresso não pode mais ser alterado.";
        }

        if (edit.arrival_time < now) {
            return "O novo instante de ingresso (" + std::to_string(edit.arrival_time) +
                   ") está no passado; ele deve ser maior ou igual ao instante atual (t=" + std::to_string(now) + ").";
        }

        task->arrival_time = edit.arrival_time;
        task->next_release = edit.arrival_time;
        changes.push_back("ingresso " + std::to_string(edit.arrival_time));
    }

    // periodo: vale a partir da proxima ativacao
    if (edit.period != original.period) {

        if (edit.period <= 0) {
            return "O período deve ser maior que zero: período 0 indica tarefa aperiódica, que não é suportada no projeto A.";
        }

        task->period = edit.period;

        if (task->activations > 0) {
            task->next_release = std::max(task->last_release + edit.period, now);
        }

        changes.push_back("período " + std::to_string(edit.period));
    }

    // prazo relativo: tambem recalcula o prazo absoluto da ativacao atual
    if (edit.deadline != original.deadline) {

        if (edit.deadline <= 0) {
            return "O prazo deve ser maior que zero.";
        }

        task->deadline = edit.deadline;

        if (task->has_job()) {
            task->job_deadline = task->job_release + edit.deadline;
        }

        changes.push_back("prazo " + std::to_string(edit.deadline));
    }

    // duracao: a ativacao atual preserva o que ja executou
    if (edit.duration != original.duration) {

        if (edit.duration <= 0) {
            return "A duração deve ser maior que zero.";
        }

        if (task->has_job()) {

            int executed = original.duration - original.remaining;

            if (edit.duration <= executed) {
                return "A ativação atual de " + task_label(id) + " já executou " + std::to_string(executed) +
                       " tick(s); a nova duração deve ser maior que " + std::to_string(executed) + ".";
            }

            task->remaining = edit.duration - executed;
        }

        task->duration = edit.duration;
        changes.push_back("duração " + std::to_string(edit.duration));
    }

    // tempo restante da ativacao atual
    if (edit.remaining != original.remaining) {

        if (!original.has_job()) {
            return "A tarefa " + task_label(id) + " não possui ativação em andamento, portanto não há tempo restante a alterar.";
        }

        if (edit.remaining < 1 || edit.remaining > task->duration) {
            return "O tempo restante deve estar entre 1 e a duração da tarefa (" + std::to_string(task->duration) + ").";
        }

        task->remaining = edit.remaining;
        changes.push_back("restante " + std::to_string(edit.remaining));
    }

    // mudanca de estado
    switch (edit.action) {

        case TaskEdit::Action::Keep:
            break;

        case TaskEdit::Action::Suspend:

            if (task->state == TaskState::NotArrived) {
                return "A tarefa " + task_label(id) + " ainda não ingressou no sistema (ingresso em t=" +
                       std::to_string(task->arrival_time) + "); não é possível suspendê-la.";
            }

            if (task->state == TaskState::Suspended) {
                return "A tarefa " + task_label(id) + " já está suspensa.";
            }

            release_cpu(next, *task);
            task->state = TaskState::Suspended;
            changes.push_back("suspensa");
            break;

        case TaskEdit::Action::Resume:

            if (task->state != TaskState::Suspended) {
                return "A tarefa " + task_label(id) + " não está suspensa (estado atual: " +
                       task_state_name(task->state) + "); somente tarefas suspensas podem ser retomadas.";
            }

            task->state = task->has_job() ? TaskState::Ready : TaskState::Waiting;
            changes.push_back("retomada");
            break;

        case TaskEdit::Action::Terminate:

            release_cpu(next, *task);
            task->state = TaskState::Terminated;
            task->finish_time = now;
            task->remaining = 0;
            task->pending_releases.clear();
            changes.push_back("encerrada manualmente");
            break;
    }

    if (changes.empty()) {
        return "";
    }

    std::string text = "alteração manual em " + task_label(id) + ": ";

    for (std::size_t index = 0; index < changes.size(); ++index) {
        text += (index > 0 ? ", " : "") + changes[index];
    }

    next.pending_events.push_back(SimEvent{now, now, EventType::Manual, id, -1, text});

    if (edit.action == TaskEdit::Action::Terminate) {
        next.pending_events.push_back(SimEvent{now, now, EventType::TaskEnd, id, -1, task_label(id) + " encerrada pelo usuário"});
    }

    discard_future();

    history_[position_] = next;

    return "";
}

std::string Simulator::add_task(const Task& task) {

    if (history_.empty()) {
        return "Nenhuma simulação carregada.";
    }

    SimState next = state();

    if (next.find(task.id) != nullptr) {
        return "Já existe uma tarefa com o id " + std::to_string(task.id) + "; o id deve ser único.";
    }

    if (task.id < 0) {
        return "O id da tarefa não pode ser negativo.";
    }

    std::string color = normalize_color(task.color);

    if (color.empty()) {
        return "Cor inválida: use 6 dígitos hexadecimais RRGGBB (ex.: F0E0D0).";
    }

    if (task.arrival_time < next.time) {
        return "O instante de ingresso (" + std::to_string(task.arrival_time) + ") está no passado; use um valor maior ou igual a t=" +
               std::to_string(next.time) + ".";
    }

    if (task.duration <= 0) {
        return "A duração deve ser maior que zero.";
    }

    if (task.period <= 0) {
        return "O período deve ser maior que zero: tarefas aperiódicas (período 0) não são suportadas no projeto A.";
    }

    if (task.deadline <= 0) {
        return "O prazo deve ser maior que zero.";
    }

    Task created = task;

    created.color = color;
    created.ignored = false;
    created.reset_runtime();

    next.tasks.push_back(created);

    std::sort(next.tasks.begin(), next.tasks.end(), [](const Task& a, const Task& b) {
        return a.id < b.id;
    });

    next.pending_events.push_back(SimEvent{next.time, next.time, EventType::Manual, created.id, -1,
                                           "tarefa " + task_label(created.id) + " adicionada pelo usuário"});

    discard_future();

    history_[position_] = next;

    return "";
}

std::string Simulator::set_system(const std::string& algorithm, int quantum, int cpu_count) {

    if (history_.empty()) {
        return "Nenhuma simulação carregada.";
    }

    SimState next = state();

    std::string name = to_upper(trim(algorithm));

    if (SchedulerRegistry::instance().find(name) == nullptr) {
        return "Algoritmo de escalonamento \"" + name + "\" desconhecido.";
    }

    if (name != next.algorithm && next.time > 0) {
        return "O algoritmo de escalonamento deve ser escolhido antes do início da simulação (requisito 4.1). "
               "Volte para t = 0 (botão \"Início\") para trocá-lo.";
    }

    if (quantum < 0) {
        return "O quantum não pode ser negativo (use 0 para não limitar a fatia de tempo).";
    }

    if (cpu_count < 1 || cpu_count > MAX_CPUS) {
        return "A quantidade de CPUs deve estar entre 1 e " + std::to_string(MAX_CPUS) + ".";
    }

    std::vector<std::string> changes;

    if (name != next.algorithm) {
        next.algorithm = name;
        changes.push_back("algoritmo " + name);
    }

    if (quantum != next.quantum) {
        next.quantum = quantum;
        changes.push_back("quantum " + std::to_string(quantum));
    }

    if (cpu_count != next.cpu_count) {

        // tarefas que estavam nas cpus removidas voltam para a fila de prontas
        for (Task& task : next.tasks) {

            if (task.state == TaskState::Running && task.cpu >= cpu_count) {
                release_cpu(next, task);
                task.state = TaskState::Ready;
                task.last_cpu = -1;
            }
        }

        // as cpus removidas encerram a contagem de tempo desligada
        next.cpu_count = cpu_count;
        next.cpu_task.resize(cpu_count, -1);
        next.cpu_off_since.resize(cpu_count, -1);
        next.cpu_off_total.resize(cpu_count, 0);

        changes.push_back(std::to_string(cpu_count) + " CPU(s)");
    }

    if (changes.empty()) {
        return "";
    }

    std::string text = "alteração manual do sistema: ";

    for (std::size_t index = 0; index < changes.size(); ++index) {
        text += (index > 0 ? ", " : "") + changes[index];
    }

    next.pending_events.push_back(SimEvent{next.time, next.time, EventType::Manual, -1, -1, text});

    discard_future();

    history_[position_] = next;

    return "";
}

// ---------------------------------------------------------------------------
// consultas
// ---------------------------------------------------------------------------

std::size_t Simulator::visible_event_count() const {

    // os eventos sao gravados na ordem dos passos, entao os visiveis
    // formam um prefixo do vetor e podem ser achados por busca binaria
    auto end = std::partition_point(events_.begin(), events_.end(), [&](const SimEvent& event) {
        return event.step < position_;
    });

    return static_cast<std::size_t>(end - events_.begin());
}

const std::vector<SimEvent>& Simulator::all_events() const {
    return events_;
}

const std::vector<TickRecord>& Simulator::ticks() const {
    return ticks_;
}

int Simulator::max_cpu_count() const {

    int result = history_.empty() ? 1 : state().cpu_count;

    for (int index = 0; index < position_ && index < static_cast<int>(ticks_.size()); ++index) {
        result = std::max(result, static_cast<int>(ticks_[index].cpu_task.size()));
    }

    return result;
}

int Simulator::total_deadline_misses() const {

    int total = 0;

    if (!history_.empty()) {
        for (const Task& task : state().tasks) {
            total += task.deadline_misses;
        }
    }

    return total;
}
