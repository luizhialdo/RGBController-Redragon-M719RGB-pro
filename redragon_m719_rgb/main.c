#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <hidapi/hidapi.h>

// Definições de constantes do hardware do Redragon M719 Ultimate
#define REDRAGON_VID      0x25a7  // Vendor ID (Redragon)
#define REDRAGON_PID_CABO 0xfab0  // Product ID quando conectado via Cabo USB
#define REDRAGON_PID_24GH 0xfa7e  // Product ID quando conectado via Dongle Wireless 2.4GHz
#define TARGET_INTERFACE  1       // Interface USB específica para envio de Custom Features

// --- FUNÇÃO DE CONVERSÃO DO MODO STREAMING ---

/**
 * Converte a porcentagem de brilho informada pelo usuário para o byte correspondente do chip.
 * No modo streaming, a velocidade fica fixa em 50% (0x13) baseado nos payloads reais mapeados.
 */
unsigned char obter_streaming_brilho(int brilho_porcentagem) {
    if (brilho_porcentagem == 10)      return 0x19; // Brilho Mínimo (10%)
    else if (brilho_porcentagem == 50) return 0x7d; // Brilho Médio (50%)
    return 0xff;                       // Brilho Máximo (100% - Padrão)
}

// --- FUNÇÕES AUXILIARES DE PARSING DE CORES ---

/**
 * Converte um caractere hexadecimal ('0'-'9', 'a'-'f') em seu valor numérico de 0 a 15.
 */
int hex_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * Pega um par de caracteres hexadecimais (ex: "ff") e converte em um único byte numérico (255).
 */
int parse_hex_byte(const char *hex_pair) {
    int high = hex_to_val(hex_pair[0]);
    int low = hex_to_val(hex_pair[1]);
    if (high == -1 || low == -1) return -1;
    return (high << 4) | low; // Junta os nibbles high e low em 1 byte
}

/**
 * Exibe o menu de ajuda completo detalhando o uso de cada modo e os argumentos aceitos.
 */
void exibir_ajuda(const char *nome_programa) {
    printf("========================================================================\n");
    printf("                REDRAGON M719 ULTIMATE - CONTROLLER HELP                \n");
    printf("========================================================================\n\n");
    printf("Uso Geral:\n");
    printf("  %s <modo> [argumentos_especificos]\n\n", nome_programa);
    
    printf("------------------------------------------------------------------------\n");
    printf("1. MODO ESTÁTICO (static / estatico)\n");
    printf("   Define uma cor fixa para os LEDs do mouse.\n");
    printf("   Argumentos:\n");
    printf("     <cor_hex> : Nome da cor (red, green, blue, yellow, purple, white, orange)\n");
    printf("                 OU código hexadecimal de 6 dígitos (ex: ff0055).\n");
    printf("     [brilho]  : Opcional. Níveis aceitos: 10, 50 ou 100 (Padrão: 100).\n");
    printf("   Exemplos:\n");
    printf("     %s static blue          <- Cor azul fixa no brilho máximo\n", nome_programa);
    printf("     %s static ff5500 50     <- Cor laranja customizada no brilho médio (50%%)\n\n", nome_programa);

    printf("------------------------------------------------------------------------\n");
    printf("2. MODO RESPIRAÇÃO (respiration / respiracao)\n");
    printf("   Faz a cor escolhida pulsar (efeito fade in / fade out).\n");
    printf("   Argumentos:\n");
    printf("     <cor_hex>    : Nome da cor ou código hexadecimal de 6 dígitos.\n");
    printf("     [brilho]     : Opcional. Níveis aceitos: 10, 50 ou 100 (Padrão: 100).\n");
    printf("     [velocidade] : Opcional. Níveis aceitos: 1 (Lento), 2 (Médio), 3 (Rápido) (Padrão: 2).\n");
    printf("   Exemplos:\n");
    printf("     %s respiration red 100 1   <- Vermelho pulsando de forma Lenta\n", nome_programa);
    printf("     %s respiration 00ffff 50 3 <- Ciano pulsando de forma Rápida no brilho de 50%%\n\n", nome_programa);

    printf("------------------------------------------------------------------------\n");
    printf("3. MODO STREAMING (streaming)\n");
    printf("   Ativa o efeito RGB dinâmico fluído (onda de cores de fábrica do mouse).\n");
    printf("   Argumentos:\n");
    printf("     [brilho] : Opcional. Altera exclusivamente o brilho do efeito.\n");
    printf("                Níveis aceitos: 10, 50 ou 100 (Padrão: 100).\n");
    printf("                Nota: A velocidade neste modo é travada em 50%% estável.\n");
    printf("   Exemplos:\n");
    printf("     %s streaming      <- Ativa o RGB fluido no brilho máximo (100%%)\n", nome_programa);
    printf("     %s streaming 10   <- Ativa o RGB fluido no brilho mínimo (10%%)\n\n", nome_programa);

    printf("------------------------------------------------------------------------\n");
    printf("4. MODO DESLIGADO (off / desligado)\n");
    printf("   Apaga completamente a iluminação RGB do mouse.\n");
    printf("   Exemplo:\n");
    printf("     %s off\n", nome_programa);
    printf("========================================================================\n");
}

int main(int argc, char *argv[]) {
    // Se o usuário não passar nenhum argumento, ou passar explicitly "help" ou "-h", exibe o menu estendido
    if (argc < 2 || strcasecmp(argv[1], "help") == 0 || strcasecmp(argv[1], "-h") == 0 || strcasecmp(argv[1], "--help") == 0) {
        exibir_ajuda(argv[0]);
        return 0;
    }

    // Captura e categoriza o modo solicitado pelo usuário
    char *modo_input = argv[1];
    int modo_off = (strcasecmp(modo_input, "off") == 0 || strcasecmp(modo_input, "desligado") == 0);
    int modo_streaming = (strcasecmp(modo_input, "streaming") == 0);
    int modo_static = (strcasecmp(modo_input, "static") == 0 || strcasecmp(modo_input, "estatico") == 0);
    int modo_respiration = (strcasecmp(modo_input, "respiration") == 0 || strcasecmp(modo_input, "respiracao") == 0);

    // Validação de segurança: modos que não sejam 'off' ou 'streaming' precisam obrigatoriamente de uma cor informada
    if (!modo_off && !modo_streaming && argc < 3) {
        fprintf(stderr, "Erro: O modo '%s' requer que você informe uma cor. Digite '%s help' para ver exemplos.\n", modo_input, argv[0]);
        return -1;
    }

    // Inicialização das variáveis com os estados padrões de fábrica
    int brilho_porcentagem = 100; 
    int vel_input = 2; // Representa velocidade Média (Padrão)
    int customizado = 0;

    // Parser dos argumentos dinâmicos com base no modo escolhido
    if (modo_streaming) {
        if (argc >= 3) {
            brilho_porcentagem = atoi(argv[2]);
            customizado = 1;
        }
    } else if (modo_static) {
        if (argc >= 4) {
            brilho_porcentagem = atoi(argv[3]);
            customizado = 1;
        }
    } else if (modo_respiration) {
        if (argc >= 4) {
            brilho_porcentagem = atoi(argv[3]);
            customizado = 1;
        }
        if (argc >= 5) {
            vel_input = atoi(argv[4]);
        }
    }

    // Validação dos limites de brilho aceitos pelo controlador físico do mouse
    if (customizado && brilho_porcentagem != 10 && brilho_porcentagem != 50 && brilho_porcentagem != 100) {
        fprintf(stderr, "Erro: Brilho inválido (%d). Valores aceitos: 10, 50 ou 100.\n", brilho_porcentagem);
        return -1;
    }

    // Definição do ID de Operação do efeito de iluminação (Byte 6 do Payload)
    unsigned char mode_byte = 0x02; 
    if (modo_streaming)        mode_byte = 0x00;
    else if (modo_off)         mode_byte = 0x04; 
    else if (modo_static)      mode_byte = 0x02;
    else if (modo_respiration) mode_byte = 0x01;

    // Tratamento e normalização dos canais de cores (RGB)
    int r = 0, g = 0, b = 0;
    if (modo_streaming) { 
        // Streaming usa paleta automática, mas o pacote precisa inicializar o cabeçalho RGB com esses bytes estáveis
        r = 0xff; g = 0x00; b = 0x00; 
    }
    else if (modo_off) { 
        r = 0xff; g = 0x00; b = 0xff; 
    }
    else {
        char color_str[7];
        memset(color_str, 0, sizeof(color_str));

        // Mapeamento de apelidos de cores amigáveis para Hexadecimal bruto
        if (strcasecmp(argv[2], "red") == 0)         strcpy(color_str, "ff0000");
        else if (strcasecmp(argv[2], "green") == 0)   strcpy(color_str, "00ff00");
        else if (strcasecmp(argv[2], "blue") == 0)    strcpy(color_str, "0000ff");
        else if (strcasecmp(argv[2], "yellow") == 0)  strcpy(color_str, "ffff00");
        else if (strcasecmp(argv[2], "purple") == 0)  strcpy(color_str, "ff00ff");
        else if (strcasecmp(argv[2], "white") == 0)   strcpy(color_str, "ffffff");
        else if (strcasecmp(argv[2], "orange") == 0)  strcpy(color_str, "ff4000");
        else {
            // Se não for um apelido, processa a string Hex literal removendo espaços vazios se houver
            int len = 0;
            for (int i = 0; argv[2][i] != '\0'; i++) {
                if (!isspace((unsigned char)argv[2][i])) {
                    if (len < 6) color_str[len++] = argv[2][i];
                    else len++;
                }
            }
            if (len != 6) {
                fprintf(stderr, "Erro: O código de cor hexadecimal deve conter exatamente 6 caracteres (Ex: ff00ff).\n");
                return -1;
            }
        }
        // Converte as fatias de texto Hex em inteiros numéricos para o payload
        r = parse_hex_byte(&color_str[0]);
        g = parse_hex_byte(&color_str[2]);
        b = parse_hex_byte(&color_str[4]);
    }

    // Inicialização do pacote USB Feature Report (Tamanho total de 17 Bytes requisitado pelo protocolo do M719)
    unsigned char payload[17];
    memset(payload, 0, sizeof(payload));

    // Montagem do cabeçalho estático padrão do firmware Redragon
    payload[0] = 0x08; payload[1] = 0x07; payload[2] = 0x00; payload[3] = 0x00;
    payload[4] = 0xa0; payload[5] = 0x07;
    payload[6] = mode_byte; 
    payload[7] = (unsigned char)r; payload[8] = (unsigned char)g; payload[9] = (unsigned char)b;

    // Conta quantas cores estão operando acima de zero (usado no cálculo do Checksum dinâmico)
    int cores_ativas = (r > 0) + (g > 0) + (b > 0);

    // --- MONTAGEM DOS BYTES DE CONTROLE E CHECKSUM (BYTES 10, 11 e 12) ---
    if (modo_off) {
        payload[10] = 0x96; payload[11] = 0xff; payload[12] = 0xbe;
    } 
    else if (modo_streaming) {
        // Modo Streaming: Trava a velocidade no padrão estável de 50% (0x13) e injeta o brilho dinâmico mapeado
        unsigned char s_byte = 0x13; 
        unsigned char b_byte = obter_streaming_brilho(brilho_porcentagem);

        payload[10] = s_byte;
        payload[11] = b_byte;
        
        // Aplicação da equação matemática reversa idêntica aos logs reais coletados:
        // Byte 12 Checksum = Velocidade + Brilho - 0xCE
        payload[12] = (unsigned char)(s_byte + b_byte - 0xce);
    } 
    else if (mode_byte == 0x01) {
        // --- MODO RESPIRAÇÃO ---
        unsigned char brightness_byte = 0xff; 
        if (brilho_porcentagem == 10)       brightness_byte = 0x19; 
        else if (brilho_porcentagem == 50)  brightness_byte = 0x7f; 
        else if (brilho_porcentagem == 100) brightness_byte = 0xff;

        // Traduz os argumentos simplificados 1, 2 e 3 para a linguagem de máquina do chip
        unsigned char speed_byte = 0x96; 
        if (vel_input == 1)      speed_byte = 0xff; // Velocidade Lenta
        else if (vel_input == 3) speed_byte = 0x28; // Velocidade Rápida
        else                     speed_byte = 0x96; // Velocidade Média (Padrão)

        payload[10] = speed_byte;      
        payload[11] = brightness_byte;  
        
        // Estrutura de Lookup Table (Matriz) para o Checksum de Respiração gerado pelo hardware original
        unsigned char base_idx12 = 0xc0; 
        if (speed_byte == 0x28) { 
            if (brightness_byte == 0xff) base_idx12 = 0x2e; 
            else if (brightness_byte == 0x19) base_idx12 = 0x48; 
            else base_idx12 = 0x3b; 
        } 
        else if (speed_byte == 0x96) { 
            if (brightness_byte == 0xff) base_idx12 = 0xc0; 
            else if (brightness_byte == 0x19) base_idx12 = 0xa6; 
            else base_idx12 = 0xb3;
        } 
        else if (speed_byte == 0xff) { 
            if (brightness_byte == 0xff) base_idx12 = 0x57; 
            else if (brightness_byte == 0x19) base_idx12 = 0x3d; 
            else base_idx12 = 0x4a;
        }

        // Ajusta o Checksum adicionando o offset das cores que estão ativas no canal RGB
        if (cores_ativas > 0) payload[12] = base_idx12 + (cores_ativas - 1);
        else payload[12] = base_idx12;

    } else {
        // --- MODO ESTÁTICO ---
        unsigned char brightness_byte = 0xff; 
        if (brilho_porcentagem == 10)       brightness_byte = 0x19; 
        else if (brilho_porcentagem == 50)  brightness_byte = 0x7f; 
        else if (brilho_porcentagem == 100) brightness_byte = 0xff;

        if (brilho_porcentagem == 100) {
            if (cores_ativas <= 1) {
                payload[10] = 0xff; payload[11] = 0xff; payload[12] = 0x56;
            } else {
                payload[10] = 0x96; payload[11] = 0xff;
                // Exceções de cores compostas específicas mapeadas no modo estático
                if (r == 0xff && g == 0x40 && b == 0x00)       payload[12] = 0x7f; // Laranja
                else if (r == 0xff && g == 0xff && b == 0xff)  payload[12] = 0xc1; // Branco
                else                                           payload[12] = 0xc0;
            }
        } else {
            payload[10] = (brilho_porcentagem == 10) ? 0x96 : 0xcb; 
            payload[11] = brightness_byte; 
            unsigned char base_idx12_static = (brilho_porcentagem == 10) ? 0xa5 : 0x7d; 
            
            if (cores_ativas > 1) payload[12] = base_idx12_static + (cores_ativas - 1);
            else payload[12] = base_idx12_static;
        }
    }
    
    // Bytes de finalização exigidos pelo protocolo de comunicação USB do dispositivo
    payload[13] = 0x00; payload[14] = 0x00; payload[15] = 0x00; 
    payload[16] = 0x4a; 

    // --- COMUNICAÇÃO USB VIA HIDAPI ---

    if (hid_init() < 0) {
        fprintf(stderr, "Falha crítica ao inicializar a biblioteca HIDAPI.\n");
        return -1;
    }

    // Varre os barramentos USB do sistema procurando os IDs da Redragon
    struct hid_device_info *devs, *cur_dev;
    devs = hid_enumerate(REDRAGON_VID, 0x0000); 
    cur_dev = devs;
    char *path_to_open = NULL;
    int pid_encontrado = 0;
    
    // Busca refinada filtrando exclusivamente pela Interface número 1 (onde os pacotes RGB trafegam)
    while (cur_dev) {
        if ((cur_dev->product_id == REDRAGON_PID_CABO || cur_dev->product_id == REDRAGON_PID_24GH) 
            && cur_dev->interface_number == TARGET_INTERFACE) {
            path_to_open = strdup(cur_dev->path);
            pid_encontrado = cur_dev->product_id;
            break;
        }
        cur_dev = cur_dev->next;
    }
    hid_free_enumeration(devs); // Libera memória da listagem bruta

    // Tenta abrir o canal de comunicação direta com o hardware encontrado
    hid_device *handle = NULL;
    if (path_to_open) {
        handle = hid_open_path(path_to_open);
        free(path_to_open);
    }

    if (!handle) {
        fprintf(stderr, "Erro: Mouse não encontrado ou sem permissão de acesso. Verifique a conexão ou execute com 'sudo'.\n");
        hid_exit();
        return -1;
    }

    // Transmite o pacote completo de 17 bytes para a controladora interna do mouse
    int res = hid_send_feature_report(handle, payload, sizeof(payload));
    if (res < 0) {
        fprintf(stderr, "Falha ao enviar comando para o dispositivo: %ls\n", hid_error(handle));
    } else {
        const char *conexao = (pid_encontrado == REDRAGON_PID_24GH) ? "Sem Fio (2.4G)" : "Com Cabo";
        if (modo_respiration) {
            const char *vel_txt = (vel_input == 1) ? "Lento (1)" : (vel_input == 3) ? "Rápido (3)" : "Médio (2)";
            printf("Sucesso! [%s] Modo: respiration | Brilho: %d%% | Velocidade: %s\n", conexao, brilho_porcentagem, vel_txt);
        } else if (modo_streaming) {
            printf("Sucesso! [%s] Modo: streaming | Brilho: %d%%\n", conexao, brilho_porcentagem);
        } else if (modo_static) {
            printf("Sucesso! [%s] Modo: static | Brilho: %d%%\n", conexao, brilho_porcentagem);
        } else {
            printf("Sucesso! [%s] Modo: %s aplicado\n", conexao, modo_input);
        }
    }

    // Fecha a conexão de forma limpa e encerra a HIDAPI
    hid_close(handle);
    hid_exit();
    return 0;
}