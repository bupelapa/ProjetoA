#include "Task.h"

#include "Config.h"

#include <sstream>

const char* task_state_name(TaskState state) {

    switch (state) {
        case TaskState::NotArrived: return "não ingressou";
        case TaskState::Ready:      return "pronta";
        case TaskState::Running:    return "executando";
        case TaskState::Suspended:  return "suspensa";
        case TaskState::Waiting:    return "aguardando período";
        case TaskState::Terminated: return "terminada";
        case TaskState::Ignored:    return "ignorada (aperiódica)";
    }

    return "?";
}

Task::Task()
    : Task(0, "FFFFFF", 0, 0, 0, 0)
{
}

Task::Task(
    int id,
    const std::string& color,
    int arrival_time,
    int duration,
    int period,
    int deadline
)
    : id(id),
      color(color),
      arrival_time(arrival_time),
      duration(duration),
      period(period),
      deadline(deadline),
      ignored(false)
{
    reset_runtime();
}

// verifica se a tarefa possui periodo maior que zero
// tarefas com periodo maior que zero sao consideradas periodicas
bool Task::is_periodic() const {
    return period > 0;
}

// verifica se a tarefa possui periodo igual a zero
// tarefas com periodo igual a zero sao consideradas aperiodicas
bool Task::is_aperiodic() const {
    return period == 0;
}

bool Task::has_job() const {
    return remaining > 0;
}

int Task::executed_in_job() const {
    return has_job() ? duration - remaining : 0;
}

void Task::reset_runtime() {

    state = ignored ? TaskState::Ignored : TaskState::NotArrived;
    cpu = -1;
    last_cpu = -1;
    activations = 0;
    executions_completed = 0;
    remaining = 0;
    job_release = -1;
    job_deadline = -1;
    job_missed = false;
    next_release = arrival_time;
    last_release = -1;
    pending_releases.clear();
    quantum_used = 0;
    total_executed = 0;
    total_ready_time = 0;
    deadline_misses = 0;
    finish_time = -1;
}

std::string Task::describe() const {

    std::ostringstream out;

    out << "Tarefa T" << id << "\n"
        << "  estado: " << task_state_name(state);

    if (cpu >= 0) {
        out << " (CPU " << cpu << ")";
    }

    out << "\n"
        << "  cor: " << color << "\n"
        << "  ingresso: " << arrival_time
        << " | duração: " << duration
        << " | período: " << period
        << " | prazo relativo: " << deadline << "\n"
        << "  ativações liberadas: " << activations << "/" << MAX_ACTIVATIONS
        << " | concluídas: " << executions_completed << "/" << MAX_ACTIVATIONS << "\n";

    if (has_job()) {
        out << "  ativação atual: liberada em t=" << job_release
            << ", prazo absoluto t=" << job_deadline
            << ", restante " << remaining << " de " << duration
            << (job_missed ? " (PRAZO PERDIDO)" : "") << "\n";
    }
    else {
        out << "  sem ativação em andamento\n";
    }

    if (!pending_releases.empty()) {
        out << "  ativações enfileiradas: " << pending_releases.size() << "\n";
    }

    if (state != TaskState::Terminated && state != TaskState::Ignored && activations < MAX_ACTIVATIONS) {
        out << "  próxima ativação: t=" << next_release << "\n";
    }

    out << "  quantum usado: " << quantum_used
        << " | executado no total: " << total_executed
        << " | tempo na fila de prontas: " << total_ready_time << "\n"
        << "  perdas de prazo: " << deadline_misses;

    if (finish_time >= 0) {
        out << " | terminou em t=" << finish_time;
    }

    out << "\n";

    if (!event_list.empty()) {
        out << "  lista de eventos (projeto B): " << event_list << "\n";
    }

    return out.str();
}
