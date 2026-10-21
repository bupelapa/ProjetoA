@echo off
rem compila o simulador e o escalonador de exemplo sem precisar do cmake
rem requer o g++ do msys2 (ucrt64 ou mingw64) no PATH
rem
rem -static: o executavel nao depende das dlls do mingw, apenas de dlls do
rem          proprio windows, e roda em qualquer computador sem instalar nada
rem -mwindows: aplicacao grafica (sem janela de console)

if not exist build mkdir build
if not exist build\escalonadores mkdir build\escalonadores

g++ -std=c++17 -O2 -DUNICODE -D_UNICODE -Iinclude src\*.cpp -o build\simulador.exe -static -mwindows -lcomctl32 -lcomdlg32 -lgdi32 -luser32 -lshell32
if errorlevel 1 goto erro

gcc -shared -O2 -static -o build\escalonadores\escalonador_fifo.dll plugins\escalonador_fifo.c
if errorlevel 1 goto erro

echo.
echo compilado com sucesso: build\simulador.exe
goto fim

:erro
echo.
echo falha na compilacao

:fim
