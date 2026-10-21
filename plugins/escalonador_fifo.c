// exemplo de escalonador externo carregado em tempo de execucao (requisito 4.2)
//
// compile como dll e copie o arquivo para a pasta "escalonadores" ao lado do
// simulador.exe; o algoritmo passa a aceitar "FIFO" no arquivo de configuracao
// sem que o simulador precise ser recompilado
//
// exemplo com mingw:
//   gcc -shared -O2 -static -o escalonadores/escalonador_fifo.dll plugins/escalonador_fifo.c

#include "../include/SchedulerPlugin.h"

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

EXPORT const char* escalonador_nome(void) {
    return "FIFO";
}

EXPORT const char* escalonador_descricao(void) {
    return "First In, First Out (ativação liberada primeiro executa primeiro) - DLL de exemplo";
}

// quanto menor a chave, maior a prioridade: a ativacao mais antiga vence;
// como o simulador favorece quem ja estava executando em caso de empate,
// uma tarefa so perde a cpu para uma ativacao liberada antes dela
EXPORT long long escalonador_prioridade(const SchedTaskInfo* tarefa, int agora) {
    (void)agora;
    return tarefa->job_release;
}
