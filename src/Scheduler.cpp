#include "Scheduler.h"

#include "Util.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

// rate monotonic preemptivo: a prioridade e fixa e inversamente
// proporcional ao periodo, ou seja, menor periodo = maior prioridade
class RateMonotonicScheduler : public Scheduler {

public:

    std::string name() const override {
        return "RM";
    }

    std::string description() const override {
        return "Rate Monotonic preemptivo (menor período = maior prioridade)";
    }

    long long priority(const SchedTaskInfo& task, int) const override {
        return task.period;
    }
};

// earliest deadline first preemptivo: a prioridade e dinamica, a ativacao
// com o prazo absoluto mais proximo e a mais prioritaria
class EarliestDeadlineFirstScheduler : public Scheduler {

public:

    std::string name() const override {
        return "EDF";
    }

    std::string description() const override {
        return "Earliest Deadline First preemptivo (prazo absoluto mais próximo primeiro)";
    }

    long long priority(const SchedTaskInfo& task, int) const override {
        return task.job_deadline;
    }
};

// adaptador que transforma as funcoes exportadas por uma dll
// em um objeto Scheduler comum
class PluginScheduler : public Scheduler {

public:

    PluginScheduler(const std::string& name, const std::string& description, SchedPriorityFn priority_fn)
        : name_(name), description_(description), priority_fn_(priority_fn)
    {
    }

    std::string name() const override {
        return name_;
    }

    std::string description() const override {
        return description_;
    }

    long long priority(const SchedTaskInfo& task, int now) const override {
        return priority_fn_(&task, now);
    }

private:

    std::string name_;
    std::string description_;
    SchedPriorityFn priority_fn_;
};

}

SchedulerRegistry& SchedulerRegistry::instance() {

    // instancia unica criada na primeira utilizacao, o que garante que os
    // algoritmos embutidos ja estejam registrados antes de qualquer consulta
    static SchedulerRegistry registry;

    return registry;
}

SchedulerRegistry::SchedulerRegistry() {

    // para incluir um novo algoritmo embutido basta escrever a classe acima
    // e registra-la aqui; nenhum outro arquivo precisa ser modificado
    add(std::make_unique<RateMonotonicScheduler>());
    add(std::make_unique<EarliestDeadlineFirstScheduler>());
}

bool SchedulerRegistry::add(std::unique_ptr<Scheduler> scheduler) {

    if (!scheduler || find(scheduler->name()) != nullptr) {
        return false;
    }

    schedulers_.push_back(std::move(scheduler));

    return true;
}

const Scheduler* SchedulerRegistry::find(const std::string& name) const {

    std::string wanted = to_upper(trim(name));

    for (const auto& scheduler : schedulers_) {
        if (to_upper(scheduler->name()) == wanted) {
            return scheduler.get();
        }
    }

    return nullptr;
}

std::vector<std::string> SchedulerRegistry::names() const {

    std::vector<std::string> result;

    for (const auto& scheduler : schedulers_) {
        result.push_back(scheduler->name());
    }

    return result;
}

void SchedulerRegistry::load_plugins(const std::wstring& directory, std::vector<std::string>& messages) {

#ifdef _WIN32
    WIN32_FIND_DATAW data;

    HANDLE search = FindFirstFileW((directory + L"\\*.dll").c_str(), &data);

    // a pasta de escalonadores e opcional: se ela nao existir, apenas
    // os algoritmos embutidos ficam disponiveis
    if (search == INVALID_HANDLE_VALUE) {
        return;
    }

    do {

        std::wstring path = directory + L"\\" + data.cFileName;

        std::string file = wide_to_utf8(data.cFileName);

        HMODULE module = LoadLibraryW(path.c_str());

        if (module == nullptr) {
            messages.push_back("escalonador '" + file + "' não pôde ser carregado (DLL inválida ou de outra arquitetura)");
            continue;
        }

        auto name_fn = reinterpret_cast<SchedNameFn>(reinterpret_cast<void*>(GetProcAddress(module, "escalonador_nome")));
        auto description_fn = reinterpret_cast<SchedDescriptionFn>(reinterpret_cast<void*>(GetProcAddress(module, "escalonador_descricao")));
        auto priority_fn = reinterpret_cast<SchedPriorityFn>(reinterpret_cast<void*>(GetProcAddress(module, "escalonador_prioridade")));

        if (name_fn == nullptr || priority_fn == nullptr || name_fn() == nullptr) {
            messages.push_back("escalonador '" + file + "' ignorado: a DLL não exporta escalonador_nome e escalonador_prioridade");
            FreeLibrary(module);
            continue;
        }

        std::string name = to_upper(trim(name_fn()));

        std::string description = (description_fn != nullptr && description_fn() != nullptr)
            ? description_fn()
            : "escalonador externo (" + file + ")";

        if (name.empty() || !add(std::make_unique<PluginScheduler>(name, description, priority_fn))) {
            messages.push_back("escalonador '" + file + "' ignorado: nome vazio ou já existente (" + name + ")");
            FreeLibrary(module);
            continue;
        }

        // a dll permanece carregada ate o fim do programa, pois o
        // objeto registrado guarda ponteiros para funcoes dentro dela
        messages.push_back("escalonador externo '" + name + "' carregado de " + file);

    } while (FindNextFileW(search, &data));

    FindClose(search);
#else
    (void)directory;
    (void)messages;
#endif
}
