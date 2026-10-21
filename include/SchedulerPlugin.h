#ifndef SCHEDULER_PLUGIN_H
#define SCHEDULER_PLUGIN_H

// interface binaria (abi em c) usada pelos escalonadores externos
//
// um novo algoritmo de escalonamento pode ser adicionado ao simulador sem
// recompilar o executavel (requisito 4.2): basta compilar uma biblioteca
// dinamica (.dll) que exporte as funcoes abaixo e copia-la para a pasta
// "escalonadores" ao lado do simulador.exe
//
// a interface e em c (e nao em c++) porque o formato de classes c++ muda
// entre compiladores; funcoes c com structs simples funcionam com dlls
// geradas por qualquer compilador (mingw, msvc, clang...)

#ifdef __cplusplus
extern "C" {
#endif

// visao somente leitura da tarefa entregue ao escalonador
typedef struct SchedTaskInfo {
    int id;
    int arrival_time;     // instante de ingresso da tarefa
    int duration;         // tempo de execucao de cada ativacao
    int period;           // periodo de ativacao
    int deadline;         // prazo relativo
    int job_release;      // instante em que a ativacao atual foi liberada
    int job_deadline;     // prazo absoluto da ativacao atual
    int remaining;        // tempo restante da ativacao atual
    int executed_in_job;  // quanto a ativacao atual ja executou
} SchedTaskInfo;

// funcoes que a dll deve exportar (nomes exatos):
//
//   const char* escalonador_nome(void);
//       nome usado no arquivo de configuracao (ex.: "FIFO")
//
//   const char* escalonador_descricao(void);          (opcional)
//       texto curto mostrado na interface
//
//   long long escalonador_prioridade(const SchedTaskInfo* tarefa, int agora);
//       devolve a chave de prioridade da tarefa: quanto MENOR o valor,
//       MAIOR a prioridade; os desempates do requisito 4.3 sao aplicados
//       pelo simulador, de forma igual para todos os algoritmos

typedef const char* (*SchedNameFn)(void);
typedef const char* (*SchedDescriptionFn)(void);
typedef long long (*SchedPriorityFn)(const SchedTaskInfo* task, int now);

#ifdef __cplusplus
}
#endif

#endif
