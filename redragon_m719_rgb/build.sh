#!/bin/bash

echo "===================================================="
echo "    Instalador de Dependências e Compilador M719"
echo "===================================================="

# 1. Detectar a distribuição Linux e instalar dependências básicas (Compiler + HIDAPI)
if [ -f /etc/debian_version ]; then
    echo "[1/3] Sistema baseado em Debian/Ubuntu detectado."
    echo "      Instalando gcc, make e libhidapi-dev..."
    sudo apt update
    sudo apt install -y build-essential libhidapi-dev

elif [ -f /etc/fedora-release ]; then
    echo "[1/3] Sistema baseado em Fedora detectado."
    echo "      Instalando gcc e hidapi-devel..."
    sudo dnf install -y gcc make hidapi-devel

elif [ -f /etc/arch-release ]; then
    echo "[1/3] Sistema baseado em Arch Linux detectado."
    echo "      Instalando base-devel e hidapi..."
    sudo pacman -Syu --needed --noconfirm base-devel hidapi

else
    echo "Aviso: Distribuição não reconhecida automaticamente."
    echo "Certifique-se de ter o 'gcc' e o pacote de desenvolvimento da 'hidapi' instalados manualmente."
fi

# 2. Verificar se o arquivo main.c existe no diretório atual
if [ ! -f "main.c" ]; then
    echo "----------------------------------------------------"
    echo "Erro: O arquivo 'main.c' não foi encontrado nesta pasta!"
    echo "Certifique-se de salvar o código do controlador como 'main.c' antes de rodar este script."
    exit 1
fi

# 3. Compilar o programa principal
echo "[2/3] Compilando o programa 'main.c'..."
gcc main.c -o controle_m719 -D_DEFAULT_SOURCE -lhidapi-hidraw

# 4. Verificar se a compilação deu certo
if [ $? -eq 0 ]; then
    echo "[3/3] Compilação concluída com sucesso!"
    echo "----------------------------------------------------"
    echo "O executável './controle_m719' foi gerado."
    echo "Você já pode testá-lo rodando:"
    echo "  ./controle_m719 streaming 50"
    echo "----------------------------------------------------"
else
    echo "Erro: Falha catastrófica durante a compilação do código."
    exit 1
fi
