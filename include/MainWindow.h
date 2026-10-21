#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <string>
#include <vector>

// cria a janela principal do simulador e executa o laco de mensagens
// initial_file: arquivo de configuracao a abrir na inicializacao (opcional)
// plugin_messages: resultado da carga dos escalonadores externos
int run_main_window(const std::wstring& initial_file, const std::vector<std::string>& plugin_messages);

#endif
