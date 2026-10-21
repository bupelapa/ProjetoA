#include "FormDialog.h"

#include <algorithm>

namespace {

const wchar_t* FORM_CLASS = L"SimuladorFormulario";

// dados compartilhados entre show_form e o procedimento da janela
struct FormData {
    std::vector<FormField>* fields = nullptr;
    std::vector<HWND> controls;
    bool done = false;
    bool accepted = false;
};

// o texto do windows usa \r\n como quebra de linha em caixas de texto
std::wstring windows_newlines(const std::wstring& text) {

    std::wstring result;

    for (wchar_t character : text) {
        if (character == L'\n') {
            result += L"\r\n";
        }
        else {
            result += character;
        }
    }

    return result;
}

void read_values(FormData* data) {

    for (std::size_t index = 0; index < data->fields->size(); ++index) {

        FormField& field = (*data->fields)[index];

        HWND control = data->controls[index];

        if (!field.options.empty()) {

            LRESULT selected = SendMessageW(control, CB_GETCURSEL, 0, 0);

            if (selected >= 0 && selected < static_cast<LRESULT>(field.options.size())) {
                field.value = field.options[static_cast<std::size_t>(selected)];
            }

            continue;
        }

        int length = GetWindowTextLengthW(control);

        std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');

        GetWindowTextW(control, &text[0], length + 1);

        text.resize(static_cast<std::size_t>(length));

        field.value = text;
    }
}

LRESULT CALLBACK form_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {

    auto* data = reinterpret_cast<FormData*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {

        case WM_CREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return 0;
        }

        case WM_COMMAND:

            if (data != nullptr && LOWORD(wparam) == IDOK) {
                read_values(data);
                data->accepted = true;
                data->done = true;
                return 0;
            }

            if (data != nullptr && LOWORD(wparam) == IDCANCEL) {
                data->done = true;
                return 0;
            }

            break;

        case WM_CLOSE:

            if (data != nullptr) {
                data->done = true;
            }

            return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

}

bool show_form(
    HWND parent,
    const std::wstring& title,
    const std::wstring& info,
    std::vector<FormField>& fields,
    double scale
) {

    HINSTANCE instance = GetModuleHandleW(nullptr);

    static bool registered = false;

    if (!registered) {

        WNDCLASSW window_class = {};

        window_class.lpfnWndProc = form_procedure;
        window_class.hInstance = instance;
        window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        window_class.lpszClassName = FORM_CLASS;

        RegisterClassW(&window_class);

        registered = true;
    }

    auto s = [&](int value) {
        return static_cast<int>(value * scale + 0.5);
    };

    HFONT font = CreateFontW(-s(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    const int margin = s(12);
    const int label_width = s(270);
    const int field_width = s(330);
    const int row_height = s(30);
    const int button_height = s(30);
    const int client_width = margin * 3 + label_width + field_width;

    // altura do texto informativo calculada com a fonte real,
    // limitada para que o formulario caiba na tela
    int info_height = 0;

    std::wstring info_text = windows_newlines(info);

    if (!info.empty()) {

        HDC screen = GetDC(nullptr);
        HGDIOBJ old_font = SelectObject(screen, font);

        RECT measure = {0, 0, client_width - margin * 2 - s(20), 0};

        DrawTextW(screen, info_text.c_str(), -1, &measure, DT_CALCRECT | DT_WORDBREAK);

        SelectObject(screen, old_font);
        ReleaseDC(nullptr, screen);

        info_height = std::min<int>(measure.bottom + s(12), s(320));
    }

    const int fields_top = margin + info_height + (info_height > 0 ? margin : 0);
    const int buttons_top = fields_top + static_cast<int>(fields.size()) * row_height + margin;
    const int client_height = buttons_top + button_height + margin;

    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    DWORD ex_style = WS_EX_DLGMODALFRAME;

    RECT frame = {0, 0, client_width, client_height};

    AdjustWindowRectEx(&frame, style, FALSE, ex_style);

    int width = frame.right - frame.left;
    int height = frame.bottom - frame.top;

    // centraliza sobre a janela principal
    RECT parent_rect;

    GetWindowRect(parent, &parent_rect);

    int left = parent_rect.left + ((parent_rect.right - parent_rect.left) - width) / 2;
    int top = parent_rect.top + ((parent_rect.bottom - parent_rect.top) - height) / 2;

    FormData data;

    data.fields = &fields;

    HWND window = CreateWindowExW(ex_style, FORM_CLASS, title.c_str(), style,
                                  std::max(0, left), std::max(0, top), width, height,
                                  parent, nullptr, instance, &data);

    auto apply_font = [&](HWND control) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    };

    if (info_height > 0) {

        HWND info_box = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", info_text.c_str(),
                                        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                                        margin, margin, client_width - margin * 2, info_height,
                                        window, nullptr, instance, nullptr);
        apply_font(info_box);
    }

    HWND first_editable = nullptr;

    for (std::size_t index = 0; index < fields.size(); ++index) {

        const FormField& field = fields[index];

        int y = fields_top + static_cast<int>(index) * row_height;

        HWND label = CreateWindowExW(0, L"STATIC", field.label.c_str(), WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                     margin, y + s(5), label_width, row_height - s(6),
                                     window, nullptr, instance, nullptr);
        apply_font(label);

        HWND control = nullptr;

        int id = 1000 + static_cast<int>(index);

        if (!field.options.empty()) {

            control = CreateWindowExW(0, L"COMBOBOX", L"",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                                      margin * 2 + label_width, y + s(2), field_width, s(300),
                                      window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);

            int selected = 0;

            for (std::size_t option = 0; option < field.options.size(); ++option) {

                SendMessageW(control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(field.options[option].c_str()));

                if (field.options[option] == field.value) {
                    selected = static_cast<int>(option);
                }
            }

            SendMessageW(control, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
        }
        else {

            DWORD edit_style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | (field.read_only ? ES_READONLY : 0);

            control = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", field.value.c_str(), edit_style,
                                      margin * 2 + label_width, y + s(2), field_width, row_height - s(6),
                                      window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
        }

        apply_font(control);

        data.controls.push_back(control);

        if (first_editable == nullptr && !field.read_only) {
            first_editable = control;
        }
    }

    int button_width = s(110);

    HWND ok = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                              client_width - margin * 2 - button_width * 2, buttons_top, button_width, button_height,
                              window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), instance, nullptr);

    HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                  client_width - margin - button_width, buttons_top, button_width, button_height,
                                  window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), instance, nullptr);

    apply_font(ok);
    apply_font(cancel);

    // janela modal: a janela principal fica desabilitada enquanto o
    // formulario estiver aberto, como em um dialogo comum do windows
    EnableWindow(parent, FALSE);

    ShowWindow(window, SW_SHOW);

    if (first_editable != nullptr) {
        SetFocus(first_editable);
        SendMessageW(first_editable, EM_SETSEL, 0, -1);
    }
    else {
        SetFocus(ok);
    }

    MSG message;

    bool quit = false;

    while (!data.done) {

        BOOL result = GetMessageW(&message, nullptr, 0, 0);

        if (result <= 0) {
            quit = true;
            break;
        }

        // IsDialogMessage trata tab entre campos, enter (ok) e esc (cancelar)
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    EnableWindow(parent, TRUE);

    DestroyWindow(window);

    SetActiveWindow(parent);

    DeleteObject(font);

    if (quit) {
        PostQuitMessage(static_cast<int>(message.wParam));
    }

    return data.accepted;
}
