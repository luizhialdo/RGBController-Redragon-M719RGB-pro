#!/bin/bash

echo "===================================================="
echo "   Compilador do M719 com Interface Gráfica GTK 4"
echo "===================================================="

# 1. Instalar as dependências do GTK 4 + HIDAPI nativo do sistema
if [ -f /etc/debian_version ]; then
    echo "[1/2] Instalando dependências de desenvolvimento do GTK4 no Ubuntu/Debian..."
    sudo apt update
    sudo apt install -y libgtk-4-dev libhidapi-dev pkg-config gcc

elif [ -f /etc/fedora-release ]; then
    echo "[1/2] Instalando dependências no Fedora..."
    sudo dnf install -y gtk4-devel hidapi-devel pkgconf-pkg-config gcc

elif [ -f /etc/arch-release ]; then
    echo "[1/2] Instalando dependências no Arch Linux..."
    sudo pacman -Syu --needed --noconfirm gtk4 hidapi pkgconf gcc
fi

# 2. Compilar o programa injetando as diretivas do pkg-config
echo "[2/2] Compilando interface gráfica main.c..."
gcc main.c -o Redragon_M719RGB-PRO `pkg-config --cflags --libs gtk4` -lhidapi-hidraw

if [ $? -eq 0 ]; then
    echo "----------------------------------------------------"
    echo "Compilação concluída com sucesso!"
    echo "Abra o painel gráfico rodando o executável:"
    echo "  ./Redragon_M719RGB-PRO"
    echo "===================================================="
else
    echo "Erro: Ocorreu uma falha ao compilar a interface GTK."
    exit 1
fi