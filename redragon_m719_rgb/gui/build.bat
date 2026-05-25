@echo off
SETLOCAL EnableDelayedExpansion
title Instalador do Controlador Redragon M719 RGB (GTK4 + HIDAPI)

echo ============================================================
echo   VERIFICANDO DEPENDENCIAS E INICIANDO INSTALACAO (WINDOWS)  
echo ============================================================
echo.

:: 1. Verificar se o instalador do MSYS2 existe, se nao, efetuar o download
if not exist "C:\msys64\msys2_shell.cmd" (
    echo [INFO] MSYS2 nao detectado. Baixando o instalador oficial...
    powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; (New-Object System.Net.WebClient).DownloadFile('https://github.com/msys2/msys2-installer/releases/download/2024-01-13/msys2-x86_64-20240113.exe', 'msys2-installer.exe')"
    
    echo [INFO] Executando instalador do MSYS2 em modo silencioso...
    echo Aguarde, este processo pode levar alguns minutos...
    start /wait msys2-installer.exe --mode unattended --disable-components msys2_ide
    del msys2-installer.exe
    echo [OK] MSYS2 instalado com sucesso em C:\msys64.
) else (
    echo [OK] MSYS2 ja instalado no sistema.
)

echo.
echo ============================================================
echo   INSTALANDO COMPILADOR, GTK4 E HIDAPI VIA MSYS2            
echo ============================================================
echo.

:: 2. Instalar as dependencias necessarias usando o gerenciador Pacman do MSYS2
:: O comando abaixo atualiza e instala o compilador GCC, GTK4, HIDAPI e pkg-config de forma silenciosa
C:\msys64\usr\bin\bash.exe -lc "pacman -Syu --noconfirm"
C:\msys64\usr\bin\bash.exe -lc "pacman -S --needed --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-gtk4 mingw-w64-x86_64-hidapi mingw-w64-x86_64-pkgconf"

echo [OK] Dependencias instaladas com sucesso.
echo.
echo ============================================================
echo   COMPILANDO O CONTROLADOR REDRAGON M719                      
echo ============================================================
echo.

:: Copiar o arquivo main.c temporariamente para dentro do ambiente msys64 para compilar de forma limpa
copy /Y "main.c" "C:\msys64\home\main.c" >nul

:: Executar o comando de compilacao GCC configurado para a arquitetura de 64 bits do Windows
C:\msys64\usr\bin\bash.exe -lc "gcc /home/main.c -o /home/controle_m719_gui.exe $(pkg-config --cflags --libs gtk4) -lhidapi"

:: Mover o executavel finalizado de volta para a pasta do usuario
if exist "C:\msys64\home\controle_m719_gui.exe" (
    move /Y "C:\msys64\home\controle_m719_gui.exe" ".\controle_m719_gui.exe" >nul
    del "C:\msys64\home\main.c"
    echo [OK] Compilacao concluida com sucesso! Arquivo gerado: controle_m719_gui.exe
) else (
    echo [ERRO] Ocorreu uma falha critica durante a compilacao do arquivo main.c.
    pause
    exit /b
)

echo.
echo ============================================================
echo   CRIANDO SCRIPT DE INICIALIZACAO COM AS DLLs DO GTK4        
echo ============================================================
echo.

:: Como o GTK necessita das suas DLLs para abrir no Windows, criamos um iniciador .bat inteligente
:: Ele vincula as DLLs temporariamente ao PATH do sistema apenas durante a execucao do programa
(
echo @echo off
echo set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%%PATH%%
echo start "" "%%~dp0controle_m719_gui.exe"
) > "Iniciar_Controlador.bat"

echo [OK] Arquivo 'Iniciar_Controlador.bat' gerado na pasta atual.
echo.

echo ============================================================
echo   INSTALACAO CONCLUIDA!                                      
echo ============================================================
echo.
echo Para executar a interface do seu mouse no Windows, use o arquivo:
echo --- Iniciar_Controlador.bat ---
echo.
pause