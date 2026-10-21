cmake_minimum_required(VERSION 3.15)

project(SimuladorTempoReal VERSION 0.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

# WIN32: aplicacao grafica do windows (sem janela de console)
add_executable(simulador WIN32
    src/main.cpp
    src/Task.cpp
    src/ConfigParser.cpp
    src/Util.cpp
    src/Scheduler.cpp
    src/Simulator.cpp
    src/GanttRenderer.cpp
    src/SvgCanvas.cpp
    src/GdiCanvas.cpp
    src/FormDialog.cpp
    src/MainWindow.cpp
)

target_compile_definitions(simulador PRIVATE UNICODE _UNICODE)

# somente bibliotecas que fazem parte do proprio windows
target_link_libraries(simulador PRIVATE comctl32 comdlg32 gdi32 user32 shell32)

if(MINGW)
    # ligacao estatica: o .exe nao depende das dlls do mingw (libstdc++,
    # libgcc, winpthread) e roda em qualquer windows sem instalar nada
    target_link_options(simulador PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(MSVC)
    # codigo fonte em utf-8 (textos com acentos) e runtime estatico
    target_compile_options(simulador PRIVATE /utf-8)
    set_property(TARGET simulador PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# escalonador externo de exemplo, carregado em tempo de execucao a partir
# da pasta "escalonadores" ao lado do simulador.exe
add_library(escalonador_fifo SHARED plugins/escalonador_fifo.c)

set_target_properties(escalonador_fifo PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/escalonadores
)

if(MINGW)
    target_link_options(escalonador_fifo PRIVATE -static -static-libgcc)
endif()
