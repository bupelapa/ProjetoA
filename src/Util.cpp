#include "Util.h"

#include <cctype>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

std::string trim(const std::string& text) {

    std::size_t start = 0;

    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
        ++start;
    }

    std::size_t end = text.size();

    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }

    return text.substr(start, end - start);
}

std::string to_upper(const std::string& text) {

    std::string result = text;

    for (char& character : result) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }

    return result;
}

std::vector<std::string> split(const std::string& line, char delimiter) {

    std::vector<std::string> fields;

    std::stringstream stream(line);

    std::string field;

    while (std::getline(stream, field, delimiter)) {
        fields.push_back(trim(field));
    }

    return fields;
}

bool parse_int(const std::string& text, int& value) {

    std::string clean = trim(text);

    if (clean.empty()) {
        return false;
    }

    std::size_t position = 0;

    bool negative = false;

    if (clean[0] == '+' || clean[0] == '-') {
        negative = (clean[0] == '-');
        position = 1;
    }

    if (position >= clean.size()) {
        return false;
    }

    // acumula em long long para detectar valores grandes demais
    // antes que eles estourem a capacidade de um int
    long long result = 0;

    for (; position < clean.size(); ++position) {

        if (!std::isdigit(static_cast<unsigned char>(clean[position]))) {
            return false;
        }

        result = result * 10 + (clean[position] - '0');

        if (result > 1000000000LL) {
            return false;
        }
    }

    value = static_cast<int>(negative ? -result : result);

    return true;
}

std::string normalize_color(const std::string& text) {

    std::string clean = trim(text);

    if (!clean.empty() && clean[0] == '#') {
        clean = clean.substr(1);
    }

    if (clean.size() != 6) {
        return "";
    }

    for (char character : clean) {
        if (!std::isxdigit(static_cast<unsigned char>(character))) {
            return "";
        }
    }

    return to_upper(clean);
}

void color_to_rgb(const std::string& hex, unsigned char& r, unsigned char& g, unsigned char& b) {

    std::string clean = normalize_color(hex);

    // cor invalida nunca deveria chegar aqui, mas o cinza evita
    // que um erro de validacao derrube o desenho do grafico
    if (clean.empty()) {
        r = g = b = 128;
        return;
    }

    unsigned long value = std::stoul(clean, nullptr, 16);

    r = static_cast<unsigned char>((value >> 16) & 0xFF);
    g = static_cast<unsigned char>((value >> 8) & 0xFF);
    b = static_cast<unsigned char>(value & 0xFF);
}

std::string default_color(int index) {

    // paleta com cores bem distintas entre si, para que tarefas vizinhas
    // no grafico de gantt nao sejam confundidas
    static const char* palette[] = {
        "E6194B", "3CB44B", "4363D8", "F58231", "911EB4", "42D4F4",
        "F032E6", "BFEF45", "469990", "9A6324", "800000", "808000",
        "000075", "FFE119", "DCBEFF", "AAFFC3"
    };

    const int size = static_cast<int>(sizeof(palette) / sizeof(palette[0]));

    if (index < 0) {
        index = -index;
    }

    return palette[index % size];
}

std::wstring utf8_to_wide(const std::string& text) {

#ifdef _WIN32
    if (text.empty()) {
        return std::wstring();
    }

    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);

    std::wstring result(static_cast<std::size_t>(size), L'\0');

    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &result[0], size);

    return result;
#else
    return std::wstring(text.begin(), text.end());
#endif
}

std::string wide_to_utf8(const std::wstring& text) {

#ifdef _WIN32
    if (text.empty()) {
        return std::string();
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

    std::string result(static_cast<std::size_t>(size), '\0');

    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &result[0], size, nullptr, nullptr);

    return result;
#else
    return std::string(text.begin(), text.end());
#endif
}
