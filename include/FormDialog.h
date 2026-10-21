#ifndef FORM_DIALOG_H
#define FORM_DIALOG_H

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <vector>

// um campo do formulario
struct FormField {

    std::wstring label;

    // valor inicial (valor padrao sugerido) e, ao final, o valor digitado
    std::wstring value;

    // quando nao estiver vazio, o campo vira uma lista de escolha
    std::vector<std::wstring> options;

    bool read_only = false;
};

// mostra um formulario modal generico com os campos informados
//
// decisao de projeto: em vez de desenhar uma janela de dialogo para cada
// tela (sistema, nova tarefa, editar tarefa...), um unico formulario e
// montado em tempo de execucao a partir da lista de campos. assim nao e
// necessario arquivo de recursos (.rc) e todas as telas ficam consistentes
//
// devolve true se o usuario confirmou (os valores ficam em fields)
bool show_form(
    HWND parent,
    const std::wstring& title,
    const std::wstring& info,
    std::vector<FormField>& fields,
    double scale
);

#endif
