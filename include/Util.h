#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>

// funcoes auxiliares de texto usadas por varias partes do simulador
// ficam centralizadas aqui para que o parser, o simulador e a interface
// grafica usem exatamente as mesmas regras (ex.: o que e um numero valido)

// remove espacos e tabulacoes do inicio e do fim do texto
std::string trim(const std::string& text);

// converte para maiusculas, permitindo comparar "rm", "Rm" e "RM" como iguais
std::string to_upper(const std::string& text);

// separa a linha pelo delimitador e aplica trim em cada campo
std::vector<std::string> split(const std::string& line, char delimiter);

// converte texto em inteiro de forma estrita: "10abc" ou "" sao rejeitados
// (std::stoi aceitaria "10abc" como 10, escondendo erros do arquivo)
bool parse_int(const std::string& text, int& value);

// valida uma cor no formato rrggbb (aceita "#" opcional) e devolve a cor
// normalizada em maiusculas; devolve texto vazio quando a cor e invalida
std::string normalize_color(const std::string& text);

// separa uma cor rrggbb ja normalizada em seus tres componentes
void color_to_rgb(const std::string& hex, unsigned char& r, unsigned char& g, unsigned char& b);

// cor sugerida (valor padrao) para a tarefa de indice informado,
// usada quando o arquivo ou o usuario nao informa a cor
std::string default_color(int index);

// conversoes entre utf-8 (usado internamente e nos arquivos) e utf-16
// (usado pela api do windows para exibir texto com acentos)
std::wstring utf8_to_wide(const std::string& text);
std::string wide_to_utf8(const std::wstring& text);

#endif
