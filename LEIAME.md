# Simulador de SO Multitarefa de Tempo Compartilhado — Projeto A

Simulador gráfico (Windows) de escalonamento preemptivo em múltiplos processadores,
com os algoritmos **Rate Monotonic (RM)** e **Earliest Deadline First (EDF)**.

## Executar

`build\simulador.exe` roda em qualquer Windows 10/11 sem instalar nada: o executável
é ligado estaticamente e usa apenas bibliotecas do próprio Windows (GDI, controles comuns).

- Duplo clique em `simulador.exe` e use **Abrir arquivo...**, ou
- `simulador.exe caminho\do\arquivo.txt` (também funciona arrastando o `.txt` sobre o `.exe`), ou
- `simulador.exe --executar arquivo.txt [saida.svg]` executa a simulação completa
  no terminal (sem janela), imprime os eventos e o resultado e grava o Gantt em SVG.

## Compilar

Com MSYS2 (g++ no PATH): `build.bat` ou, com CMake:

```
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

## Uso da interface

| Ação | Botão / atalho |
|---|---|
| Abrir arquivo de configuração (qualquer pasta/unidade) | Abrir arquivo... / Ctrl+O |
| Avançar / voltar um tick (o relógio global acompanha) | F8 / F7 |
| Voltar para t = 0, ir para um instante | Início, Ir para t... |
| Execução completa sem intervenção | Executar tudo / F9 |
| Examinar e alterar uma tarefa (parâmetros, suspender, retomar, encerrar) | duplo clique na tabela / F2 |
| Inserir tarefa durante a simulação | Nova tarefa... |
| Alterar quantum / CPUs (algoritmo só em t = 0) | Sistema... |
| Salvar o gráfico de Gantt completo (sugerido na pasta do arquivo de configuração) | Exportar imagem SVG... / Ctrl+S |
| Zoom do gráfico | Zoom +/− ou Ctrl + roda do mouse |

A tabela mostra o TCB de cada tarefa no instante atual; passar o mouse sobre o gráfico
mostra o estado da tarefa/CPU naquele tick. Ao fim da simulação o programa oferece
salvar a imagem do Gantt.

## Arquivo de configuração

```
algoritmo_escalonamento;quantum;qtde_cpus
id;cor;ingresso;duracao;periodo;prazo;lista_eventos
```

Exemplos em `config/`. Maiúsculas/minúsculas são equivalentes, o `;` final é opcional,
espaços e linhas em branco são ignorados. Campos vazios recebem valores padrão
(RM, quantum 1, 1 CPU, ingresso 0, duração 1, período 10, prazo = período), com aviso.
Tarefas com período 0 (aperiódicas) são ignoradas e o usuário é avisado.
Todos os erros do arquivo são listados de uma vez, com o número da linha.

## Decisões de simulação

- Cada tick `t` simula o intervalo `[t, t+1)`: ativações → verificação de prazos →
  fim de quantum → escalonamento global → execução → fim de ativações.
- Fila de prontas **global**; as `m` tarefas mais prioritárias ocupam as `m` CPUs.
  CPU sem tarefa é desligada (faixa listrada no gráfico; tempo desligado no status).
- Uma tarefa volta preferencialmente para a CPU que usou por último.
- Desempate (requisito 4.3): já executava → menor prazo → ingresso mais cedo →
  menor duração → sorteio (marcado com "?"). O sorteio é determinístico
  (semente guardada no estado), então voltar e avançar reproduz o mesmo resultado.
- Quando o quantum acaba, a tarefa perde a vantagem do critério "já executava"
  naquele escalonamento (senão o quantum nunca teria efeito).
- Ativação liberada antes da anterior terminar fica enfileirada e começa logo depois.
- Tarefa que perde o prazo continua executando até completar a duração.
- Tarefa periódica termina após 10 execuções.
- Estado "aguardando período" não é desenhado (tarefa inativa); "suspensa"
  (alteração manual) aparece em preto pontilhado.
- Alterar algo no meio da simulação descarta os passos já calculados depois daquele instante.

## Novos algoritmos de escalonamento

O algoritmo só define a **chave de prioridade** de cada tarefa (menor = mais prioritária);
o simulador cuida do resto. Duas formas de adicionar um:

1. **Embutido**: criar uma classe derivada de `Scheduler` em `src/Scheduler.cpp` e
   registrá-la no construtor de `SchedulerRegistry`.
2. **Externo, sem recompilar o simulador**: compilar uma DLL que exporte
   `escalonador_nome`, `escalonador_prioridade` (e opcionalmente `escalonador_descricao`),
   conforme `include/SchedulerPlugin.h`, e copiá-la para `escalonadores\` ao lado do
   `simulador.exe`. Exemplo: `plugins/escalonador_fifo.c` (algoritmo "FIFO").

## Estrutura

| Arquivo | Conteúdo |
|---|---|
| `Task` | TCB: parâmetros e estado dinâmico de cada tarefa |
| `ConfigParser` | leitura e validação do arquivo de configuração |
| `Scheduler` | interface dos algoritmos, RM, EDF e carga de DLLs |
| `Simulator` | relógio global, escalonamento multi-CPU, histórico, edições manuais |
| `GanttRenderer` | desenho do Gantt sobre um `Canvas` abstrato |
| `GdiCanvas` / `SvgCanvas` | desenho na tela / geração da imagem SVG |
| `FormDialog`, `MainWindow` | interface gráfica (Win32) |
