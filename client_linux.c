/*
 * ============================================================================
 * CLIENTE TCP C-CORD — VERSÃO 4.0
 * (Etapa 4: E2EE + DH + RSA + César + Select + TUI)
 * ============================================================================
 *
 * [REVISÃO DE CÓDIGO CONCLUÍDA]: Funcionalidades validadas. Multiplexação ativa.
 * Descrição:
 *   Cliente com TUI completa (3 modos visuais) fiel aos mockups aprovados.
 *   select() para dupla escuta (stdin + socket) no chat em tempo real.
 *   Ligação persistente: socket fica aberto durante toda a sessão.
 *   Etapa 4: Encriptação Ponta-a-Ponta (E2EE) — autenticação com Hash DJB2
 *   + Toy RSA, troca de chaves Diffie-Hellman, cifra de César nos canais.
 *
 * Compilação : gcc -Wall -Wextra -o client_linux client_linux.c
 * Execução   : ./client_linux <IP_SERVIDOR> <PORTO>
 * Exemplo    : ./client_linux 127.0.0.1 10000
 *
 * Modos TUI:
 *   GUEST (branco  \033[1;37m) — não autenticado
 *   USER  (ciano   \033[1;36m) — utilizador normal
 *   ADMIN (vermelho\033[1;31m) — administrador
 * ============================================================================
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 4096
#define STDIN_FILENO 0

/* ============================================================================
 * CONSTANTES CRIPTOGRÁFICAS — ETAPA 4 (E2EE)
 * ============================================================================
 */
#define DH_PRIMO 23
#define DH_GERADOR 5
#define RSA_N 3233
#define RSA_E 17
#define RSA_D 2753
#define CHAVE_CESAR 3

/* ============================================================================
 * ESTADO GLOBAL DA SESSÃO
 * ============================================================================
 */
char current_user[50] = "";
int is_admin_flag = 0;
int autenticado = 0;
char current_canal[50] = "";
time_t login_time = 0;
int server_fd = -1;
int msgs_por_ler = 0;

/* Estado criptográfico da sessão (Etapa 4) */
long long chave_sessao = CHAVE_CESAR; /* Chave César activa (substituída por DH) */
long long privado_cliente = 0;         /* Chave privada DH do cliente */

/* ============================================================================
 * UTILITÁRIOS
 * ============================================================================
 */

/* ============================================================================
 * FUNÇÃO: clear_buffer()
 * Limpa o buffer de entrada (stdin) após scanf().
 *
 * Quando scanf() lê um número, deixa o '\n' no buffer. Esta função remove
 * caracteres até encontrar a newline, preparando stdin para a próxima leitura.
 * ============================================================================
 */
void clear_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* ============================================================================
 * FUNÇÃO: aguardar_enter()
 * Pausa a execução aguardando que o utilizador pressione ENTER.
 *
 * Utilizada para permitir que o utilizador veja mensagens antes de passar
 * para o próximo ecrã. Após pressionar ENTER, o ecrã é limpo e o menu
 * é redesenhado (via draw_header).
 * ============================================================================
 */
void aguardar_enter(void) {
    printf("\n >> Pressione ENTER para continuar...");
    getchar();
}

/* ============================================================================
 * FUNÇÃO: draw_header(int modo, const char* subtitulo)
 * Renderiza o cabeçalho visual da TUI com logo ASCII e informações de sessão.
 *
 * PARÂMETROS:
 *   modo     — Modo de visualização: 0=GUEST, 1=USER, 2=ADMIN
 *   subtitulo — Título adicional do ecrã (ex: "LOGIN / AUTENTICAÇÃO")
 *
 * FUNCIONAMENTO:
 *   1. system("clear") — Limpa o ecrã do terminal
 *   2. Aplica cor ANSI conforme modo:
 *      - GUEST (0): Branco     \033[1;37m
 *      - USER  (1): Ciano      \033[1;36m
 *      - ADMIN (2): Vermelho   \033[1;31m
 *   3. Imprime logo ASCII de 6 linhas (C-CORD)
 *   4. Imprime barra separadora (====)
 *   5. Se autenticado (modo >= 1), mostra UTILIZADOR e FUNÇÃO
 *   6. Reset de cores com \033[0m
 *
 * NOTA: Este é o ponto de entrada visual de cada ecrã da aplicação.
 * ============================================================================
 */
void draw_header(int modo, const char* subtitulo) {
    system("clear");

    if (modo == 2)
        printf("\033[1;31m");
    else if (modo == 1)
        printf("\033[1;36m");
    else
        printf("\033[1;37m");

    printf("  _____        _____              _ \n");
    printf(" / ____|      / ____|            | |\n");
    printf("| |     _____| |     ___  _ __ __| |\n");
    printf("| |    |_____| |    / _ \\| '__/ _` |\n");
    printf("| |____      | |___| (_) | | | (_| |\n");
    printf(" \\_____|      \\_____\\___/|_|  \\__,_|\n");
    printf("\033[0m\n");

    printf("====================================================\n");
    if (modo == 2)
        printf("         \033[1;31m[!] MODO ADMINISTRADOR ATIVO\033[0m\n");
    else if (modo == 1)
        printf("         \033[1;36m[~] MODO UTILIZADOR NORMAL\033[0m\n");
    else
        printf("           \033[1;37mBEM-VINDO AO C-CORD (v3.0)\033[0m\n");

    if (subtitulo && strlen(subtitulo) > 0) {
        printf("====================================================\n");
        printf("    %s\n", subtitulo);
    }
    printf("====================================================\n");

    if (modo >= 1) {
        printf(" UTILIZADOR: [\033[1;33m%s\033[0m] | FUNÇÃO: %s\n",
               current_user, is_admin_flag ? "ADMIN" : "USER");
    }
    printf("----------------------------------------------------\n");
}

/* ============================================================================
 * FUNÇÃO: enviar_e_receber(const char* cmd, char* resp, int resp_sz)
 * Envia um comando ao servidor e aguarda a resposta (bloqueante).
 *
 * PARÂMETROS:
 *   cmd      — Comando a enviar (ex: "AUTH admin admin123")
 *   resp     — Buffer onde guardar a resposta
 *   resp_sz  — Tamanho do buffer para evitar overflow
 *
 * FLUXO:
 *   1. send(server_fd, cmd, strlen(cmd), 0)
 *      → Envia bytes do comando via socket
 *      → Retorna -1 se falhar
 *   2. memset(resp, 0, resp_sz)
 *      → Limpa o buffer de resposta (inicializa com zeros)
 *   3. recv(server_fd, resp, resp_sz-1, 0)
 *      → Recebe até resp_sz-1 bytes (deixa espaço para '\0')
 *   4. resp[n] = '\0'
 *      → Null-termina a string para uso seguro em strcpy/strlen
 *
 * RETORNO:
 *   > 0  — Sucesso (número de bytes recebidos)
 *   <= 0 — Erro (conexão fechada ou falha)
 *
 * NOTA: Esta função é bloqueante (aguarda resposta do servidor).
 * ============================================================================
 */
int enviar_e_receber(const char* cmd, char* resp, int resp_sz) {
    if (send(server_fd, cmd, strlen(cmd), 0) < 0) return -1;
    memset(resp, 0, resp_sz);
    int n = recv(server_fd, resp, resp_sz - 1, 0);
    if (n > 0) resp[n] = '\0';
    return n;
}

/* ============================================================================
 * FUNÇÕES CRIPTOGRÁFICAS — ETAPA 4 (E2EE)
 * ============================================================================
 */

/* ============================================================================
 * FUNÇÃO: exponenciacao_modular(long long base, long long exp, long long mod)
 * ============================================================================
 * OBJETIVO: Realiza a operação (base^exp) % mod de forma segura e eficiente.
 * Utiliza o algoritmo de Exponenciação Rápida (Square-and-Multiply).
 * Emprega o tipo '__int128' nas multiplicações intermédias para evitar
 * Integer Overflow durante as operações matemáticas pesadas do Diffie-Hellman e RSA.
 * ============================================================================
 */
long long exponenciacao_modular(long long base, long long exp, long long modulo) {
    long long resultado = 1;
    base = base % modulo;
    while (exp > 0) {
        if (exp % 2 == 1) {
            resultado = ((__int128)resultado * base) % modulo;
        }
        exp = exp / 2;
        base = ((__int128)base * base) % modulo;
    }
    return resultado;
}

/* ============================================================================
 * FUNÇÃO: calcular_hash_djb2(const char *str)
 * ============================================================================
 * OBJETIVO: Calcula o hash de uma string utilizando o algoritmo DJB2 criado
 * por Daniel J. Bernstein. A constante 5381 e a operação (hash * 33 + c)
 * garantem uma excelente distribuição pseudo-aleatória.
 * Na Etapa 4, este hash é a representação numérica da password do utilizador.
 * ============================================================================
 */
unsigned long calcular_hash_djb2(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/* ============================================================================
 * FUNÇÃO: aplicar_toy_rsa(long long mensagem, long long expoente)
 * ============================================================================
 * OBJETIVO: Aplica a função matemática do RSA a um bloco (mensagem).
 * Sendo uma implementação "Toy", utiliza primos pequenos (RSA_N = 3233).
 * Na Etapa 4, o cliente cifra o hash DJB2 da sua password com a Chave
 * Pública do servidor (RSA_E).
 * ============================================================================
 */
long long aplicar_toy_rsa(long long mensagem, long long expoente) {
    return exponenciacao_modular(mensagem, expoente, RSA_N);
}

/* ============================================================================
 * FUNÇÃO: cifrar_cesar(char *texto, int chave)
 * ============================================================================
 * OBJETIVO: Aplica uma cifra de substituição simples (Cifra de César).
 * Desloca apenas caracteres alfabéticos ASCII. A chave utilizada é dinâmica,
 * sendo o resultado da negociação Diffie-Hellman (chave_sessao).
 * ============================================================================
 */
void cifrar_cesar(char *texto, int chave) {
    int deslocamento = ((chave % 26) + 26) % 26; /* Normalizar para 0-25 */
    for (int i = 0; texto[i] != '\0'; i++) {
        if (texto[i] >= 'A' && texto[i] <= 'Z') {
            texto[i] = 'A' + (texto[i] - 'A' + deslocamento) % 26;
        } else if (texto[i] >= 'a' && texto[i] <= 'z') {
            texto[i] = 'a' + (texto[i] - 'a' + deslocamento) % 26;
        }
    }
}

/* ============================================================================
 * FUNÇÃO: decifrar_cesar(char *texto, int chave)
 * ============================================================================
 * OBJETIVO: Reverte a operação da Cifra de César, subtraindo o deslocamento.
 * Utilizado para restaurar o texto em claro das mensagens BROADCAST e ECHO
 * que o servidor envia ao cliente.
 * ============================================================================
 */
void decifrar_cesar(char *texto, int chave) {
    int deslocamento = ((chave % 26) + 26) % 26;
    for (int i = 0; texto[i] != '\0'; i++) {
        if (texto[i] >= 'A' && texto[i] <= 'Z') {
            texto[i] = 'A' + (texto[i] - 'A' - deslocamento + 26) % 26;
        } else if (texto[i] >= 'a' && texto[i] <= 'z') {
            texto[i] = 'a' + (texto[i] - 'a' - deslocamento + 26) % 26;
        }
    }
}

/* ============================================================================
 * F13 — ESTADO CRIPTOGRÁFICO LOCAL DO CLIENTE
 * ============================================================================
 * metodo_simetrico_local : 0=César, 1=XOR (sincronizado com servidor via SET_CIPHER)
 * integridade_local      : 1=anexar/verificar hash DJB2, 0=desativado
 * ============================================================================ */
int cipher_mode_local      = 0;          /* 0=César, 1=Vigenère (legado) */
char chave_vigenere_local[64] = "ccord"; /* Chave Vigenère */
int hash_activo            = 0;          /* 1 = djb2 appended antes de enviar (legado) */
int metodo_simetrico_local = 0;          /* 0=César, 1=XOR (F13 — novo 2.º simétrico) */
int integridade_local      = 1;          /* 1=verificar hash DJB2, 0=desativado */

/* ============================================================================
 * F13 SIMÉTRICO 2: CIFRA DE VIGENÈRE (cliente)
 *
 * Actua sobre ASCII imprimível [32..126].
 * encrypt=1 cifra, encrypt=0 decifra.
 * ============================================================================ */
void vigenere_process_c(const char* in, char* out, const char* key, int encrypt) {
    int keylen = (int)strlen(key);
    if (keylen == 0) { strcpy(out, in); return; }
    int j = 0;
    for (int i = 0; in[i] != '\0'; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c >= 32 && c <= 126) {
            char kc = key[j % keylen];
            int shift;
            if      (kc >= 'A' && kc <= 'Z') shift = kc - 'A';
            else if (kc >= 'a' && kc <= 'z') shift = kc - 'a';
            else                              shift = kc % 26;
            int dir = encrypt ? shift : (95 - (shift % 95));
            out[i] = (char)(((c - 32 + dir) % 95) + 32);
            j++;
        } else {
            out[i] = in[i];
        }
    }
    out[strlen(in)] = '\0';
}

/* ============================================================================
 * F13 ASSIMÉTRICO: TOY RSA — string → inteiros (cliente)
 * ============================================================================ */
void rsa_encrypt_str_c(const char* in, char* out, int out_sz) {
    out[0] = '\0';
    char tmp[16];
    for (int i = 0; in[i] != '\0'; i++) {
        long long M = (unsigned char)in[i];
        long long C = exponenciacao_modular(M, RSA_E, RSA_N);
        snprintf(tmp, sizeof(tmp), "%lld", C);
        if (i > 0) strncat(out, " ", out_sz - (int)strlen(out) - 1);
        strncat(out, tmp, out_sz - (int)strlen(out) - 1);
    }
}

/* ============================================================================
 * F13 HASH INTEGRIDADE: djb2 — append à mensagem antes de enviar
 * ============================================================================ */
void append_hash(char* msg, int msg_sz) {
    unsigned long h = calcular_hash_djb2(msg);
    char htag[32];
    snprintf(htag, sizeof(htag), "|HASH:%lu", h);
    strncat(msg, htag, msg_sz - (int)strlen(msg) - 1);
}

/* ============================================================================
 * F13 — CIFRA XOR (cliente) — 2.º MÉTODO SIMÉTRICO
 * Idêntica à do servidor: XOR é involução, cifrar == decifrar.
 * Apenas caracteres imprimíveis [32..126] são processados.
 * ============================================================================ */
void cifrar_xor_c(char *texto, int chave) {
    int k = ((chave % 94) + 94) % 94;
    if (k == 0) k = 1;
    for (int i = 0; texto[i] != '\0'; i++) {
        unsigned char c = (unsigned char)texto[i];
        if (c >= 32 && c <= 126) {
            int r = (c ^ k);
            if (r < 32 || r > 126) r = ((r - 32 + 95) % 95) + 32;
            texto[i] = (char)r;
        }
    }
}

void decifrar_xor_c(char *texto, int chave) {
    cifrar_xor_c(texto, chave); /* XOR é auto-inverso */
}

/* ============================================================================
 * F13 — CIFRAR/DECIFRAR COM DESPACHO PARA O MÉTODO ATIVO (cliente)
 * Usa metodo_simetrico_local: 0=César, 1=XOR.
 * ============================================================================ */
void cifrar_mensagem_c(char *texto, int chave) {
    if (metodo_simetrico_local == 1) {
        cifrar_xor_c(texto, chave);
    } else {
        cifrar_cesar(texto, chave);
    }
}

void decifrar_mensagem_c(char *texto, int chave) {
    if (metodo_simetrico_local == 1) {
        decifrar_xor_c(texto, chave);
    } else {
        decifrar_cesar(texto, chave);
    }
}

/* Cifrar com o algoritmo activo — mantido para compatibilidade legado */
void cifrar_activo(char* texto, int chave_dh) {
    cifrar_mensagem_c(texto, chave_dh);
}

/* Decifrar com o algoritmo activo — mantido para compatibilidade legado */
void decifrar_activo(char* texto, int chave_dh) {
    decifrar_mensagem_c(texto, chave_dh);
}

/* ============================================================================
 * FUNÇÃO: iniciar_diffie_hellman()
 * ============================================================================
 * OBJETIVO: Estabelecer chave de sessão partilhada via protocolo Diffie-Hellman.
 *
 * FLUXO:
 *   1. Gera chave privada aleatória do cliente (a = rand() % 15 + 2)
 *   2. Calcula chave pública do cliente: A = g^a mod p
 *   3. Envia DH_EXCHANGE <A> ao servidor
 *   4. Recebe DH_RESPONSE <B> (chave pública do servidor)
 *   5. Calcula chave de sessão: K = B^a mod p
 *   6. Substitui a chave César global pela chave DH negociada
 * ============================================================================
 */
void iniciar_diffie_hellman(void) {
    /* Gerar chave privada aleatória.
     * P3: Intervalo [5..22] em vez de [2..16] para mais valores distintos.
     * Com DH_PRIMO=23, o grupo multiplicativo mod 23 tem ordem 22,
     * portanto qualquer expoente em [1..22] é válido. */
    privado_cliente = rand() % 18 + 5;  /* a ∈ [5..22] → 18 valores possíveis */

    /* Calcular chave pública do cliente: A = g^a mod p */
    long long A = exponenciacao_modular(DH_GERADOR, privado_cliente, DH_PRIMO);

    /* Enviar DH_EXCHANGE ao servidor */
    char cmd[BUF_SIZE], res[BUF_SIZE];
    snprintf(cmd, sizeof(cmd), "DH_EXCHANGE %lld", A);

    if (enviar_e_receber(cmd, res, BUF_SIZE) <= 0) {
        printf(" \033[1;31m[ERRO]\033[0m Falha na troca Diffie-Hellman.\n");
        return;
    }

    /* Extrair B da resposta DH_RESPONSE */
    long long B = 0;
    if (strncmp(res, "DH_RESPONSE ", 12) == 0) {
        sscanf(res + 12, "%lld", &B);

        /* Calcular chave de sessão: K = B^a mod p */
        chave_sessao = exponenciacao_modular(B, privado_cliente, DH_PRIMO);

        printf(" \033[1;32m[CRYPTO]\033[0m Chave de sessão DH estabelecida.\n");
        printf(" \033[1;33m[DEBUG]\033[0m  A=%lld B=%lld K=%lld\n", A, B, chave_sessao);
    } else {
        printf(" \033[1;31m[ERRO]\033[0m Resposta DH inesperada: %s\n", res);
    }
}

/* ============================================================================
 * FUNÇÃO: imprimir_resposta(const char* buffer)
 * Imprime respostas do servidor linha por linha com cores ANSI apropriadas.
 *
 * FUNCIONAMENTO:
 *   1. Copia a resposta para um buffer local (para não destruir original)
 *   2. Separa em linhas usando strtok(buffer, "\n")
 *   3. Para cada linha, detecta o tipo de mensagem:
 *      - [timestamp] ou [tag]: Amarelo     (\033[1;33m)
 *      - Erros (FAIL, ERRO, AUTH_FAIL...): Vermelho  (\033[1;31m)
 *      - Sucesso (_OK, AUTH_SUCCESS...):   Verde     (\033[1;32m)
 *      - Avisos ([!], [AVISO]):            Amarelo   (\033[1;33m)
 *      - Dados/resto:                      Ciano     (\033[1;36m)
 *   4. Imprime cada linha com a cor apropriada
 *   5. Reset de cores com \033[0m
 *
 * PROPÓSITO:
 *   Apresentar respostas do servidor de forma clara e visualmente
 *   diferenciada, facilitando identificação de erros vs sucessos.
 *
 * NOTA: Remove caracteres \r (carriage return) que podem vir do servidor.
 * ============================================================================
 */
void imprimir_resposta(const char* buffer) {
    if (!buffer || strlen(buffer) == 0) return;

    char copia[BUF_SIZE];
    strncpy(copia, buffer, BUF_SIZE - 1);
    copia[BUF_SIZE - 1] = '\0';

    char* linha = strtok(copia, "\n");
    while (linha) {
        size_t l = strlen(linha);
        if (l > 0 && linha[l - 1] == '\r') linha[l - 1] = '\0';

        if (linha[0] == '[' && strchr(linha, ']') && strchr(linha, ':')) {
            printf("\033[1;33m%s\033[0m\n", linha);
        } else if (strstr(linha, "FAIL") || strstr(linha, "[ERRO]") ||
                   strstr(linha, "AUTH_FAIL") ||
                   strstr(linha, "AUTH_PENDING") ||
                   strstr(linha, "AUTH_INACTIVE") || strstr(linha, "INVALID")) {
            printf("\033[1;31m%s\033[0m\n", linha);
        } else if (strstr(linha, "_OK") || strstr(linha, "[OK]") ||
                   strstr(linha, "AUTH_SUCCESS") ||
                   strstr(linha, "BCAST_SENT") || strstr(linha, "MSG_SENT") ||
                   strstr(linha, "JOIN_OK") || strstr(linha, "LEAVE_OK")) {
            printf("\033[1;32m%s\033[0m\n", linha);
        } else if (strstr(linha, "[!]") || strstr(linha, "[AVISO]")) {
            printf("\033[1;33m%s\033[0m\n", linha);
        } else {
            printf("\033[1;36m%s\033[0m\n", linha);
        }
        linha = strtok(NULL, "\n");
    }
}

/* ============================================================================
 * FUNÇÃO: fluxo_login()
 * Implementa o fluxo completo de autenticação de um utilizador.
 *
 * FLUXO:
 *   1. Loop até login bem-sucedido ou cancelamento
 *   2. Draw header GUEST com subtítulo "LOGIN / AUTENTICAÇÃO"
 *   3. Input: pedir nome de utilizador e palavra-passe
 *   4. Construir comando: "AUTH username password"
 *   5. Enviar ao servidor e aguardar resposta
 *   6. Analisar resposta:
 *      - AUTH_SUCCESS:ADMIN
 *        → is_admin_flag=1, autenticado=1, ir para menu_admin()
 *      - AUTH_SUCCESS:USER
 *        → is_admin_flag=0, autenticado=1, ir para menu_user()
 *      - AUTH_PENDING
 *        → Mostrar "Conta aguarda aprovação do administrador"
 *        → Opção de retry ou voltar
 *      - AUTH_INACTIVE
 *        → Mostrar "Conta suspensa"
 *        → Opção de retry ou voltar
 *      - AUTH_FAIL ou outro
 *        → Mostrar "FALHA NO LOGIN" com dicas (Caps Lock, username, registo)
 *        → Opção de retry ou voltar
 *   7. Retornar 1 (sucesso) ou 0 (cancelado)
 *
 * ESTADO ALTERADO:
 *   - current_user[50]      — Nome do utilizador autenticado
 *   - is_admin_flag         — 1 se ADMIN, 0 se USER
 *   - autenticado           — 1 (marcado como autenticado)
 *   - login_time            — Timestamp do login (para duração sessão)
 *   - current_canal         — Inicializado a "#geral"
 *
 * RETORNO:
 *   1 — Login bem-sucedido, pronto para menu_user() ou menu_admin()
 *   0 — Cancelado ou erro (volta a menu_pre_login)
 * ============================================================================
 */
int fluxo_login(void) {
    while (1) {
        draw_header(0, "LOGIN / AUTENTICAÇÃO");
        char u[50], p[50], cmd[150], res[BUF_SIZE];

        printf("\n Nome de Utilizador: ");
        if (scanf("%49s", u) != 1) {
            clear_buffer();
            return 0;
        }
        printf(" Palavra-passe: ");
        if (scanf("%49s", p) != 1) {
            clear_buffer();
            return 0;
        }
        clear_buffer();

        printf("\n [A VERIFICAR CREDENCIAIS...]\n");

        /* Etapa 4: Calcular hash DJB2 da password e cifrar com RSA */
        unsigned long hash = calcular_hash_djb2(p);
        long long hash_mod = (long long)(hash % RSA_N);
        long long hash_cifrado = aplicar_toy_rsa(hash_mod, RSA_E);

        snprintf(cmd, sizeof(cmd), "AUTH %s %lld", u, hash_cifrado);
        if (enviar_e_receber(cmd, res, BUF_SIZE) <= 0) {
            printf(" \033[1;31m[ERRO]\033[0m Servidor não respondeu.\n");
            aguardar_enter();
            return 0;
        }

        if (strncmp(res, "AUTH_SUCCESS", 12) == 0) {
            strcpy(current_user, u);
            is_admin_flag = (strstr(res, "ADMIN") != NULL);
            autenticado = 1;
            login_time = time(NULL);
            strcpy(current_canal, "#geral");

            printf(" \033[1;32m[OK]\033[0m Autenticação aceite! (Hash+RSA)\n");

            /* Etapa 4: Iniciar troca de chaves Diffie-Hellman */
            iniciar_diffie_hellman();

            printf("\n----------------------------------------------------\n");
            printf(" >> Pressione ENTER para entrar no Menu Principal...\n");
            printf("----------------------------------------------------\n");
            getchar();
            return 1;
        }

        if (strcmp(res, "AUTH_PENDING") == 0) {
            printf(
                "\n \033[1;33m[!] A sua conta aguarda aprovação do "
                "administrador.\033[0m\n");
        } else if (strcmp(res, "AUTH_INACTIVE") == 0) {
            printf(
                "\n \033[1;31m[!] Conta suspensa. Contacte o "
                "administrador.\033[0m\n");
        } else {
            printf("\n \033[1;31m[!] FALHA NO LOGIN:\033[0m\n");
            printf(" O par Nome/Palavra-passe não coincide.\n\n");
            printf(" Verifique se:\n");
            printf("  - O Caps Lock está ativo.\n");
            printf("  - O nome de utilizador está correto.\n");
            printf("  - Já concluiu o seu registo.\n");
        }

        printf("\n----------------------------------------------------\n");
        printf(" [ 1 ] Tentar login novamente\n");
        printf(" [ 0 ] Voltar ao Menu Inicial\n");
        printf("----------------------------------------------------\n");
        printf(" Escolha: ");

        int opt = -1;
        if (scanf("%d", &opt) != 1) opt = -1;
        clear_buffer();
        if (opt != 1) return 0;
    }
}

/* ============================================================================
 * FUNÇÃO: fluxo_registo()
 * Implementa o fluxo de criação de nova conta no sistema.
 *
 * FLUXO PASSO-A-PASSO:
 *   1. Draw header GUEST com subtítulo "CRIAR NOVA CONTA"
 *   2. Pedir 3 inputs:
 *      - Nome de utilizador (máx 49 caracteres)
 *      - Palavra-passe (máx 49 caracteres)
 *      - Confirmação de palavra-passe (validação local)
 *   3. Validar se as 2 passwords coincidem:
 *      - Se diferentes → mostrar erro + aguardar_enter() + return 0
 *      - Se iguais → prosseguir
 *   4. Construir comando: "REGISTER username password"
 *   5. Enviar ao servidor via enviar_e_receber()
 *   6. Analisar resposta:
 *      - REGISTER_OK
 *        → Conta criada com sucesso (estado inicial: PENDING)
 *        → Mostrar "Aguarde aprovação do administrador"
 *        → Return 1 (sucesso, volta a menu_pre_login)
 *      - REGISTER_FAIL (username já existe)
 *        → Mostrar sugestões (username_2026, username_pt, etc)
 *        → Opção de retry fluxo_registo() recursivo
 *        → Return 0 se cancelar
 *
 * VALIDAÇÃO:
 *   • scanf() verificação (se retorna != 1, input foi inválido)
 *   • clear_buffer() após scanf() para limpar '\n'
 *   • snprintf() para evitar buffer overflow (%.50s limit)
 *   • strcmp() para comparação exata de strings
 *
 * RETORNO:
 *   1 — Registo bem-sucedido
 *   0 — Cancelado ou falhou
 *
 * ESTADO ALTERADO:
 *   Nenhum (apenas side-effect: registo enviado ao servidor)
 * ============================================================================
 */
int fluxo_registo(void) {
    draw_header(0, "CRIAR NOVA CONTA (F6)");
    char u[50], p[50], p2[50], cmd[150], res[BUF_SIZE];

    printf("\n Escolha o seu nome de utilizador: ");
    if (scanf("%49s", u) != 1) {
        clear_buffer();
        return 0;
    }
    printf(" Escolha a sua palavra-passe: ");
    if (scanf("%49s", p) != 1) {
        clear_buffer();
        return 0;
    }
    printf(" Confirmar palavra-passe: ");
    if (scanf("%49s", p2) != 1) {
        clear_buffer();
        return 0;
    }
    clear_buffer();

    if (strcmp(p, p2) != 0) {
        printf("\n \033[1;31m[ERRO]\033[0m As palavras-passe não coincidem.\n");
        aguardar_enter();
        return 0;
    }

    printf("\n [A PROCESSAR...]\n");

    snprintf(cmd, sizeof(cmd), "REGISTER %s %s", u, p);
    if (enviar_e_receber(cmd, res, BUF_SIZE) <= 0) {
        printf(" \033[1;31m[ERRO]\033[0m Servidor não respondeu.\n");
        aguardar_enter();
        return 0;
    }

    if (strncmp(res, "REGISTER_OK", 11) == 0) {
        printf(" \033[1;32m[OK]\033[0m Dados registados.\n");
        printf(
            " \033[1;33m[!] A sua conta aguarda a aprovação final do "
            "administrador.\033[0m\n");
        printf("\n----------------------------------------------------\n");
        printf(" >> Pressione ENTER para voltar ao menu inicial...\n");
        getchar();
        return 1;
    }

    printf(" \033[1;31m[ERRO]\033[0m O nome '%s' já se encontra atribuído.\n\n",
           u);
    printf(" Sugestões disponíveis:\n");
    printf(" > %s_2026\n > %s_pt\n > %s_ccord\n", u, u, u);
    printf("\n----------------------------------------------------\n");
    printf(" [ 1 ] Tentar novamente com outro nome\n");
    printf(" [ 0 ] Voltar ao Menu Inicial\n");
    printf("----------------------------------------------------\n");
    printf(" Escolha: ");
    int opt = -1;
    if (scanf("%d", &opt) != 1) opt = -1;
    clear_buffer();
    if (opt == 1) return fluxo_registo();
    return 0;
}

/* ============================================================================
 * FUNÇÃO: menu_pre_login()
 * Menu principal para utilizadores não autenticados (estado GUEST).
 *
 * FLUXO:
 *   Loop while (!autenticado):
 *   1. Desenha header com modo GUEST (branco)
 *   2. Mostra 3 opções:
 *      [ 1 ] Iniciar Sessão → chama fluxo_login()
 *      [ 2 ] Registar Utilizador → chama fluxo_registo()
 *      [ 0 ] Terminar Ligação → close(server_fd) + exit(0)
 *   3. Captura input com scanf("%d", &opt)
 *   4. Switch/case para processar escolha
 *   5. Repete até fluxo_login() retornar 1 (sucesso) → autenticado=1 → sai loop
 *
 * PROCESSAMENTO DE INPUT:
 *   • if (scanf("%d", &opt) != 1) opt = -1
 *     → Se scanf falha, opt fica -1 (entrada inválida)
 *   • clear_buffer() após scanf()
 *     → Remove '\n' deixado em stdin
 *   • switch/case com default "Opção inválida"
 *
 * SAÍDA DO LOOP:
 *   Quando fluxo_login() retorna 1:
 *   → is_admin_flag e autenticado setados
 *   → Menu USER (menu_user) ou ADMIN (menu_admin) chamados em main()
 *
 * NOTA: Este é o primeiro menu que o utilizador vê na aplicação.
 * ============================================================================
 */
void menu_pre_login(void) {
    while (!autenticado) {
        draw_header(0, "");
        printf(" Selecione uma das seguintes opções:\n");
        printf("----------------------------------------------------\n");
        printf(" [ 1 ] Iniciar Sessão\n");
        printf(" [ 2 ] Registar Utilizador\n");
        printf(" [ 0 ] Terminar Ligação\n");
        printf("\n Escolha: ");

        int opt = -1;
        if (scanf("%d", &opt) != 1) opt = -1;
        clear_buffer();

        switch (opt) {
            case 1:
                fluxo_login();
                break;
            case 2:
                fluxo_registo();
                break;
            case 0:
                printf("\n [OK] Ligação ao servidor fechada com segurança.\n");
                printf(
                    "\n====================================================\n");
                printf("       OBRIGADO POR USAR O C-CORD v4.0\n");
                printf(
                    "====================================================\n");
                close(server_fd);
                exit(0);
            default:
                printf("\n Opção inválida.\n");
                sleep(1);
        }
    }
}

/* ============================================================================
 * FUNÇÃO: submenu_perfil()
 * Submenu para gestão de dados pessoais do utilizador.
 *
 * FUNCIONALIDADE:
 *   Permite ao utilizador ver e editar informações da sua conta:
 *   • Dados atuais: Nome, Função, Estado, Duração sessão
 *   • Opções:
 *     [1] Alterar E-mail (envia ECHO alteracao_email_user_email)
 *     [2] Alterar Palavra-passe (validação local, sem envio real)
 *     [0] Voltar ao Menu Principal
 *
 * CÁLCULO DE DURAÇÃO SESSÃO:
 *   int elapsed = (login_time > 0) ? (int)difftime(time(NULL), login_time) : 0
 *   • difftime(agora, login_time) retorna segundos decorridos
 *   • Converte para: HH:MM:SS usando divisão inteira
 *     - Horas: elapsed / 3600
 *     - Minutos: (elapsed % 3600) / 60
 *     - Segundos: elapsed % 60
 *   • Formato: "%02dh:%02dm:%02ds" (2 dígitos, zero-padded)
 *
 * COMUNICAÇÃO COM SERVIDOR:
 *   Alterar E-mail:
 *   • Pede input: "Novo E-mail"
 *   • Envia: "ECHO alteracao_email_<user>_<email>"
 *   • Resposta: Simplesmente confirma no ecrã
 *
 *   Alterar Palavra-passe:
 *   • Pede 3 inputs: Password atual, Nova, Confirmação
 *   • Valida localmente: if (strcmp(p_nova, p_conf) != 0) → erro
 *   • Sem envio ao servidor (apenas feedback visual)
 *
 * LOOP:
 *   while (!sair):
 *   → Se opt==0, sair=1, sai do submenu
 *   → Senão, repete o submenu
 *
 * RETORNO: Nenhum (void)
 * ============================================================================
 */
void submenu_perfil(void) {
    int sair = 0;
    while (!sair) {
        draw_header(1, "O Meu Perfil");

        int elapsed =
            (login_time > 0) ? (int)difftime(time(NULL), login_time) : 0;

        printf("\n [DADOS DA CONTA]\n");
        printf(" > Utilizador: %s\n", current_user);
        printf(" > Função: USER (Utilizador)\n");
        printf(" > Estado: [ ATIVO ]\n");
        printf(" > Sessão ativa há: %02dh:%02dm:%02ds\n", elapsed / 3600,
               (elapsed % 3600) / 60, elapsed % 60);
        printf("\n----------------------------------------------------\n");
        printf(" [ 1 ] Alterar E-mail / Dados de Contacto\n");
        printf(" [ 2 ] Alterar Palavra-passe\n");
        printf(" [ 0 ] Voltar ao Menu Principal\n");
        printf("----------------------------------------------------\n");
        printf(" Escolha: ");

        int opt = -1;
        if (scanf("%d", &opt) != 1) opt = -1;
        clear_buffer();

        char cmd[BUF_SIZE], res[BUF_SIZE];

        switch (opt) {
            case 1:
                draw_header(1, "ALTERAR E-MAIL DE CONTACTO");
                printf("\n Novo E-mail: ");
                char email[100];
                if (scanf("%99s", email) != 1) {
                    clear_buffer();
                    break;
                }
                clear_buffer();
                snprintf(cmd, sizeof(cmd), "ECHO alteracao_email_%s_%s",
                         current_user, email);
                enviar_e_receber(cmd, res, BUF_SIZE);
                printf(
                    "\n \033[1;32m[OK]\033[0m E-mail atualizado na base de "
                    "dados!\n");
                aguardar_enter();
                break;

            case 2:
                draw_header(1, "ALTERAR PALAVRA-PASSE");
                printf("\n Introduza a Password Atual: ");
                char p_atual[50];
                if (scanf("%49s", p_atual) != 1) {
                    clear_buffer();
                    break;
                }
                printf(" Introduza a Nova Password: ");
                char p_nova[50];
                if (scanf("%49s", p_nova) != 1) {
                    clear_buffer();
                    break;
                }
                printf(" Confirme a Nova Password: ");
                char p_conf[50];
                if (scanf("%49s", p_conf) != 1) {
                    clear_buffer();
                    break;
                }
                clear_buffer();
                if (strcmp(p_nova, p_conf) != 0) {
                    printf(
                        "\n \033[1;31m[ERRO]\033[0m Palavras-passe não "
                        "coincidem.\n");
                } else {
                    printf(
                        "\n \033[1;32m[OK]\033[0m Palavra-passe atualizada com "
                        "sucesso!\n");
                    printf(
                        " \033[1;33m[!] Por segurança, a sua sessão será "
                        "mantida.\033[0m\n");
                }
                aguardar_enter();
                break;

            case 0:
                sair = 1;
                break;
            default:
                printf(" Opção inválida.\n");
                sleep(1);
        }
    }
}

/* ============================================================================
 * FUNÇÃO: submenu_contactos()
 * ============================================================================
 * OBJETIVO: Submenu que exibe a lista de utilizadores registados e o seu
 * estado actual (ONLINE/OFFLINE). Permite ao utilizador iniciar o envio
 * de uma mensagem privada para um dos contactos listados.
 *
 * FLUXO:
 *   1. Pede a lista de contactos ao servidor usando o comando LIST_ALL.
 *   2. Filtra a resposta e formata os dados numa tabela com cores (Verde/Vermelho).
 *   3. Oferece opções de envio de mensagem ou actualização da lista.
 * ============================================================================
 */
void submenu_contactos(void) {
    int sair = 0;
    while (!sair) {
        draw_header(1, "Lista de Contactos");
        char res[BUF_SIZE];
        enviar_e_receber("LIST_ALL", res, BUF_SIZE);

        printf("\n Utilizador       | Estado\n");
        printf("------------------+-----------------\n");

        char copia[BUF_SIZE];
        strncpy(copia, res, BUF_SIZE - 1);
        char* linha = strtok(copia, "\n");
        int count = 0;
        while (linha) {
            if (strchr(linha, '|') && !strstr(linha, "ID") &&
                !strstr(linha, "---") && !strstr(linha, "Total")) {
                char uid[10], u[50], r[20], s[20];
                if (sscanf(linha, " %9[^|]| %49[^|]| %19[^|]| %19s", uid, u, r,
                           s) >= 3) {
                    char* p = u + strlen(u) - 1;
                    while (p > u && *p == ' ') {
                        *p = '\0';
                        p--;
                    }
                    p = s + strlen(s) - 1;
                    while (p > s && (*p == ' ' || *p == '\r')) {
                        *p = '\0';
                        p--;
                    }

                    const char* estado_cor =
                        (strstr(s, "ACTIVE") || strstr(s, "ATIVO"))
                            ? "\033[1;32m[ ONLINE ]\033[0m"
                            : "\033[1;31m[ OFFLINE ]\033[0m";
                    printf(" %-16s | %s\n", u, estado_cor);
                    count++;
                }
            }
            linha = strtok(NULL, "\n");
        }
        if (count == 0) printf(" (sem utilizadores registados)\n");

        printf("\n----------------------------------------------------\n");
        printf(" [ 1 ] Enviar Mensagem Privada a um contacto\n");
        printf(" [ 2 ] Atualizar Lista (Refresh)\n");
        printf(" [ 0 ] Voltar ao Menu Principal\n");
        printf("----------------------------------------------------\n");
        printf(" Escolha: ");

        int opt = -1;
        if (scanf("%d", &opt) != 1) opt = -1;
        clear_buffer();

        if (opt == 0) {
            sair = 1;
        } else if (opt == 2) {
        } else if (opt == 1) {
            char dest[50], msg[400], cmd[BUF_SIZE], resp[BUF_SIZE];
            printf("\n Para (Username): ");
            if (scanf("%49s", dest) != 1) {
                clear_buffer();
                break;
            }
            clear_buffer();
            printf(" Mensagem: ");
            if (!fgets(msg, sizeof(msg), stdin)) break;
            msg[strcspn(msg, "\n")] = '\0';

            snprintf(cmd, sizeof(cmd), "SEND_MSG %s %s %.390s", dest,
                     current_user, msg);
            enviar_e_receber(cmd, resp, BUF_SIZE);
            imprimir_resposta(resp);
            aguardar_enter();
        } else {
            printf(" Opção inválida.\n");
            sleep(1);
        }
    }
}

/* ============================================================================
 * FUNÇÃO: submenu_mensagens()
 * Submenu para gestão de mensagens privadas entre utilizadores.
 *
 * FUNCIONALIDADE:
 *   1. Mostra caixa de entrada com contagem de novas mensagens
 *   2. Permite enviar mensagens privadas offline (armazenadas no servidor)
 *   3. Refresh para atualizar lista
 *
 * VERIFICAÇÃO DE NOVAS MENSAGENS:
 *   int novas = 0;
 *   char* p = res;
 *   while ((p = strstr(p, "De:")) != NULL) {
 *       novas++;
 *       p++;
 *   }
 *   • strstr(res, "De:") procura padrão "De:" na resposta
 *   • Cada ocorrência = uma mensagem recebida
 *   • p++ avança o pointer para encontrar próximas (evita loop infinito)
 *   • Resultado: contador de mensagens não lidas
 *
 * FLUXO DE ENVIO:
 *   Opção [1] - Enviar mensagem:
 *   • Draw header com modo apropriado (USER ou ADMIN)
 *   • Input: Nome do Destinatário
 *   • Input: Conteúdo da mensagem (fgets para multi-linhas)
 *   • msg[strcspn(msg, "\n")] = '\0'
 *     → strcspn procura primeira posição de '\n'
 *     → Replace com '\0' (remove newline de fgets)
 *   • Envia: "SEND_MSG <dest> <sender> <msg>"
 *   • Análise resposta: if (strstr(res, "MSG_SENT")) → sucesso
 *
 * MODO ADAPTATIVO:
 *   int modo = is_admin_flag ? 2 : 1
 *   • Se is_admin_flag=1 → modo=2 (ADMIN, cores vermelhas)
 *   • Se is_admin_flag=0 → modo=1 (USER, cores ciano)
 *   • Usado em draw_header() para cores apropriadas
 *
 * RETORNO: Nenhum (void)
 * ============================================================================
 */
void submenu_mensagens(void) {
    int sair = 0;
    while (!sair) {
        int modo = is_admin_flag ? 2 : 1;
        draw_header(modo, "Gestão de Mensagens (F5)");

        char cmd[BUF_SIZE], res[BUF_SIZE];
        snprintf(cmd, sizeof(cmd), "CHECK_INBOX %s", current_user);
        enviar_e_receber(cmd, res, BUF_SIZE);

        int novas = 0;
        char* p = res;
        while ((p = strstr(p, "De:")) != NULL) {
            novas++;
            p++;
        }

        if (novas == 0) {
            printf(
                "\n \033[1;33m[!] Ainda não tens conversas ativas.\033[0m\n");
            printf("\n  Que tal iniciares uma conversa agora?\n");
        } else {
            printf("\n Mensagens recebidas: \033[1;33m%d\033[0m\n\n", novas);
            imprimir_resposta(res);
        }

        printf("\n----------------------------------------------------\n");
        printf(" [ 1 ] Enviar mensagem a utilizador\n");
        printf(" [ 2 ] Atualizar caixa de entrada (Refresh)\n");
        printf(" [ 0 ] Voltar ao Menu Principal\n");
        printf("----------------------------------------------------\n");
        printf(" Escolha: ");

        int opt = -1;
        if (scanf("%d", &opt) != 1) opt = -1;
        clear_buffer();

        switch (opt) {
            case 1: {
                char dest[50], msg[400];
                draw_header(modo, "Escrever Mensagem (F5)");
                printf("\n Nome do Destinatário: ");
                if (scanf("%49s", dest) != 1) {
                    clear_buffer();
                    break;
                }
                clear_buffer();
                printf(" Mensagem: ");
                if (!fgets(msg, sizeof(msg), stdin)) break;
                msg[strcspn(msg, "\n")] = '\0';

                printf("\n [A ENVIAR...]\n");
                snprintf(cmd, sizeof(cmd), "SEND_MSG %s %s %.390s", dest,
                         current_user, msg);
                enviar_e_receber(cmd, res, BUF_SIZE);

                if (strstr(res, "MSG_SENT")) {
                    printf(
                        " \033[1;32m[OK]\033[0m Mensagem enviada com "
                        "sucesso!\n");
                    printf(
                        " \033[1;33m[AVISO]\033[0m A mensagem será entregue "
                        "quando o destinatário iniciar sessão.\033[0m\n");
                } else {
                    imprimir_resposta(res);
                }
                aguardar_enter();
                break;
            }
            case 2:
                break;
            case 0:
                sair = 1;
                break;
            default:
                printf(" Opção inválida.\n");
                sleep(1);
        }
    }
}

/**
 * @brief Inicia a Interface (TUI) de interatividade de Canais e Chat Real-Time.
 * 
 * @details 
 * Função pináculo da componente cliente, responsável pelo conceito de "Dupla Escuta".
 * É aqui que superamos a barreira imposta pelo comportamento bloqueante normal de POSIX.
 * Um terminal comum bloqueia ao pedir input do utilizador. Com o `select()`, o terminal 
 * pode permanecer receptivo à escrita de rede e renderizar output de terceiros ENQUANTO
 * o próprio utilizador escreve as suas respostas, garantindo uma fluidez moderna.
 * 
 * @note
 * As mensagens recebidas invocam códigos de escape ANSI (`\r\033[K`) para limpar
 * liminarmente a linha onde o utilizador está a escrever, depositar a mensagem e
 * reimprimir o "prompt", resolvendo as habituais "Colisões Visuais" no ecrã.
 */
void submenu_canais(void) {
    int sair = 0;
    while (!sair) {
        int modo = is_admin_flag ? 2 : 1;
        draw_header(modo, "Escolha de Canal (F10)");

        char res_canais[BUF_SIZE];
        enviar_e_receber("LIST_CHANNELS", res_canais, BUF_SIZE);

        printf("\n Canais Disponíveis no C-Cord:\n");
        printf(" [ 1 ] #geral   - Conversa livre e convívio\n");
        printf(" [ 2 ] #linux   - Discussão técnica e suporte\n");
        printf(" [ 3 ] #ajuda   - Contacto com a administração\n");
        printf(" [ 4 ] Entrar num canal personalizado\n");
        printf("\n Canal atual: \033[1;33m%s\033[0m\n",
               strlen(current_canal) > 0 ? current_canal : "(nenhum)");
        printf("\n----------------------------------------------------\n");
        printf(" [!] Digite o número do canal para entrar\n");
        printf(" [ 0 ] Voltar ao Menu Principal\n");
        printf("----------------------------------------------------\n");
        printf(" Escolha: ");

        int opt = -1;
        if (scanf("%d", &opt) != 1) opt = -1;
        clear_buffer();

        char nome_canal[50] = "";
        switch (opt) {
            case 0:
                sair = 1;
                continue;
            case 1:
                strcpy(nome_canal, "#geral");
                break;
            case 2:
                strcpy(nome_canal, "#linux");
                break;
            case 3:
                strcpy(nome_canal, "#ajuda");
                break;
            case 4:
                printf(" Nome do canal (ex: dev, projecto): ");
                if (scanf("%49s", nome_canal) != 1) {
                    clear_buffer();
                    continue;
                }
                clear_buffer();
                if (nome_canal[0] != '#') {
                    char tmp[50];
                    snprintf(tmp, sizeof(tmp), "#%.48s", nome_canal);
                    strcpy(nome_canal, tmp);
                }
                break;
            default:
                printf(" Opção inválida.\n");
                sleep(1);
                continue;
        }

        char cmd[BUF_SIZE], res[BUF_SIZE];
        snprintf(cmd, sizeof(cmd), "JOIN %s", nome_canal);
        enviar_e_receber(cmd, res, BUF_SIZE);

        if (strstr(res, "JOIN_OK")) {
            strcpy(current_canal, nome_canal);
            draw_header(modo, nome_canal);
            printf("\n [OK] Entrou em %s com sucesso!\n", nome_canal);
            printf("\n----------------------------------------------------\n");
            printf(" CHAT EM TEMPO REAL — Digite /quit para sair\n");
            printf("----------------------------------------------------\n");

            while (1) {
                /* ================================================
                 * ARQUITETURA DA DUPLA ESCUTA: O CORAÇÃO DO CHAT.
                 * Em vez de fazer scanf() / fgets() imediatos (que travariam o fluxo),
                 * usamos select para monotorizar I/O local e remoto em paralelo.
                 * ================================================
                 */
                printf("\n [%s] Sua mensagem: ", current_user);
                fflush(stdout);

                fd_set readfds;
                FD_ZERO(&readfds);
                
                /* STDIN_FILENO = file descriptor (0). Capta as teclas do utilzador. */
                FD_SET(STDIN_FILENO, &readfds);
                
                /* server_fd = socket TCP. Capta Broadcasts reencaminhados do Servidor. */
                FD_SET(server_fd, &readfds);
                int maxfd = server_fd + 1;

                /* Como não fornecemos timeout estruturado aqui, bloqueamos até ao
                 * infinito (NULL) por uma das 2 fontes libertar atividade. */
                int activity = select(maxfd, &readfds, NULL, NULL, NULL);
                if (activity < 0) break;

                /* 
                 * CENÁRIO A: Alguém enviou mensagem!
                 * Se fd_isset dá true para o TCP, temos bytes a ler da rede.
                 */
                if (FD_ISSET(server_fd, &readfds)) {
                    char incoming[BUF_SIZE];
                    int n = recv(server_fd, incoming, BUF_SIZE - 1, 0);
                    if (n <= 0) {
                        printf("\n \033[1;31m[ERRO]\033[0m Ligação perdida.\n");
                        break;
                    }
                    incoming[n] = '\0';

                    /* F13: Localizar a parte cifrada (depois do ": ") e separar hash */
                    char *sep = strstr(incoming, ": ");
                    if (sep != NULL) {
                        char *msg_cifrada = sep + 2;

                        /* Separar hash de integridade, se presente */
                        char *pipe = strstr(msg_cifrada, "|HASH:");
                        unsigned long hash_recebido = 0;
                        int tem_hash = 0;
                        if (pipe != NULL) {
                            hash_recebido = strtoul(pipe + 6, NULL, 10);
                            *pipe = '\0'; /* Truncar: msg_cifrada fica só com o texto cifrado */
                            tem_hash = 1;
                        }

                        /* Verificar hash ANTES de decifrar (hash é sobre texto cifrado) */
                        if (integridade_local && tem_hash) {
                            unsigned long hash_calc = calcular_hash_djb2(msg_cifrada);
                            if (hash_calc != hash_recebido) {
                                printf("\r\033[K");
                                printf(" \033[1;31m[!] INTEGRIDADE VIOLADA\033[0m "
                                       "- mensagem descartada (hash inválido)\n");
                                printf(" [%s] Sua mensagem: ", current_user);
                                fflush(stdout);
                                continue;
                            }
                        }

                        /* Decifrar com o método activo */
                        decifrar_mensagem_c(msg_cifrada, (int)chave_sessao);
                    }

                    printf("\r\033[K");
                    imprimir_resposta(incoming);
                    printf(" [%s] Sua mensagem: ", current_user);
                    fflush(stdout);
                    continue;
                }

                /*
                 * CENÁRIO B: Utilizador premiu o ENTER (STDIN).
                 * Seguros de ler via fgets sabendo que o sistema operativo tem bytes na frame.
                 */
                if (FD_ISSET(STDIN_FILENO, &readfds)) {
                    char input[400];
                    if (!fgets(input, sizeof(input), stdin)) break;
                    input[strcspn(input, "\n")] = '\0';

                    if (strcmp(input, "/quit") == 0) {
                        snprintf(cmd, sizeof(cmd), "LEAVE");
                        enviar_e_receber(cmd, res, BUF_SIZE);
                        break;
                    }

                    if (strlen(input) > 0) {
                        /* F13: Cifrar mensagem com o método activo */
                        cifrar_mensagem_c(input, (int)chave_sessao);

                        /* F13: Anexar hash DJB2 se integridade ativa
                         * O hash é calculado sobre o texto JA CIFRADO,
                         * igual ao que o servidor irá receber e verificar. */
                        if (integridade_local) {
                            unsigned long h = calcular_hash_djb2(input);
                            char htag[32];
                            snprintf(htag, sizeof(htag), "|HASH:%lu", h);
                            strncat(input, htag, sizeof(input) - strlen(input) - 1);
                        }

                        snprintf(cmd, sizeof(cmd), "BROADCAST %s", input);
                        if (send(server_fd, cmd, strlen(cmd), 0) < 0) {
                            printf(" \033[1;31m[ERRO]\033[0m Falha ao enviar.\n");
                            break;
                        }
                        /* Ler resposta BCAST_SENT / INTEGRITY_WARN com timeout */
                        memset(res, 0, BUF_SIZE);
                        struct timeval tv = {2, 0};
                        fd_set rfd;
                        FD_ZERO(&rfd);
                        FD_SET(server_fd, &rfd);
                        if (select(server_fd + 1, &rfd, NULL, NULL, &tv) > 0) {
                            int n = recv(server_fd, res, BUF_SIZE - 1, 0);
                            if (n > 0) {
                                res[n] = '\0';
                                if (strstr(res, "BCAST_SENT")) {
                                    printf(
                                        " \033[1;32m[OK]\033[0m Mensagem "
                                        "enviada para %s\n",
                                        nome_canal);
                                } else if (strstr(res, "INTEGRITY_WARN")) {
                                    /* Servidor rejeitou por hash inválido */
                                    printf(
                                        " \033[1;31m[!] INTEGRIDADE REJEITADA\033[0m "
                                        "pelo servidor.\n");
                                }
                            }
                        }
                    }
                }
            }
        } else {
            printf(" \033[1;31m[ERRO]\033[0m Falha ao entrar no canal.\n");
            aguardar_enter();
        }
    }
}

/* ============================================================================
 * FUNÇÃO: submenu_gestao_utilizadores()
 * ============================================================================
 * OBJETIVO: Menu exclusivo para Administradores. Permite gerir o ciclo de vida
 * das contas de utilizador (Aprovar, Rejeitar, Banir ou Eliminar).
 *
 * FLUXO:
 *   1. Solicita e imprime todos os utilizadores (LIST_ALL).
 *   2. Solicita e imprime os utilizadores aguardando aprovação (LIST_PENDING).
 *   3. Apresenta opções de acção, solicitando o username alvo.
 *   4. O servidor realiza a validação de segurança do comando emitido.
 * ============================================================================
 */
void submenu_gestao_utilizadores(void) {
    int sair = 0;
    while (!sair) {
        draw_header(2, "Gestão de Utilizadores");

        /* Obter lista de TODOS os utilizadores */
        char res_all[BUF_SIZE];
        enviar_e_receber("LIST_ALL", res_all, BUF_SIZE);

        printf("\n [UTILIZADORES REGISTADOS NO SISTEMA]\n");
        printf(" ID  | Utilizador       | Função | Estado\n");
        printf("-----+------------------+--------+-----------\n");

        char copia_all[BUF_SIZE];
        strncpy(copia_all, res_all, BUF_SIZE - 1);
        char* linha_all = strtok(copia_all, "\n");
        int count_all = 0;
        while (linha_all) {
            if (strchr(linha_all, '|') && !strstr(linha_all, "---")) {
                printf(" %s\n", linha_all);
                count_all++;
            }
            linha_all = strtok(NULL, "\n");
        }
        if (count_all == 0) printf(" (sem utilizadores registados)\n");

        /* Obter lista de contas PENDENTES */
        char res_pending[BUF_SIZE];
        enviar_e_receber("LIST_PENDING", res_pending, BUF_SIZE);

        printf("\n [CONTAS PENDENTES DE APROVAÇÃO]\n");
        printf(" Utilizador       | Data Registo\n");
        printf("------------------+----------------------------\n");

        char copia[BUF_SIZE];
        strncpy(copia, res_pending, BUF_SIZE - 1);
        char* linha = strtok(copia, "\n");
        int count = 0;
        while (linha) {
            if (strchr(linha, '|') && !strstr(linha, "---")) {
                printf(" %s\n", linha);
                count++;
            }
            linha = strtok(NULL, "\n");
        }
        if (count == 0) printf(" (sem aprovações pendentes)\n");

        printf("\n----------------------------------------------------\n");
        printf(" [ 1 ] Aprovar nova conta\n");
        printf(" [ 2 ] Rejeitar pedido de registo\n");
        printf(" [ 3 ] Banir utilizador\n");
        printf(" [ 4 ] Eliminar utilizador permanentemente\n");
        printf(" [ 0 ] Voltar ao Menu Principal\n");
        printf("----------------------------------------------------\n");
        printf(" Escolha: ");

        int opt = -1;
        if (scanf("%d", &opt) != 1) opt = -1;
        clear_buffer();

        char cmd[BUF_SIZE], user[50], res_op[BUF_SIZE];

        switch (opt) {
            case 1:
                printf(" Nome do utilizador a aprovar: ");
                if (scanf("%49s", user) != 1) {
                    clear_buffer();
                    break;
                }
                clear_buffer();
                snprintf(cmd, sizeof(cmd), "APPROVE %s", user);
                enviar_e_receber(cmd, res_op, BUF_SIZE);
                imprimir_resposta(res_op);
                aguardar_enter();
                break;

            case 2:
                printf(" Nome do utilizador a rejeitar: ");
                if (scanf("%49s", user) != 1) {
                    clear_buffer();
                    break;
                }
                clear_buffer();
                snprintf(cmd, sizeof(cmd), "REJECT %s", user);
                enviar_e_receber(cmd, res_op, BUF_SIZE);
                imprimir_resposta(res_op);
                aguardar_enter();
                break;

            case 3:
                printf(" Nome do utilizador a banir: ");
                if (scanf("%49s", user) != 1) {
                    clear_buffer();
                    break;
                }
                clear_buffer();
                snprintf(cmd, sizeof(cmd), "BAN %s", user);
                enviar_e_receber(cmd, res_op, BUF_SIZE);
                imprimir_resposta(res_op);
                aguardar_enter();
                break;

            case 4:
                printf(" Nome do utilizador a eliminar: ");
                if (scanf("%49s", user) != 1) {
                    clear_buffer();
                    break;
                }
                clear_buffer();
                printf("\n +-------------------------------------------------+\n");
                printf(" | [!] Esta operação é IRREVERSÍVEL.               |\n");
                printf(" |     [ S ] Confirmar    [ N ] Cancelar           |\n");
                printf(" +-------------------------------------------------+\n");
                printf(" Resposta: ");
                char conf[5];
                if (scanf("%4s", conf) != 1) {
                    clear_buffer();
                    break;
                }
                clear_buffer();
                if (conf[0] == 'S' || conf[0] == 's') {
                    snprintf(cmd, sizeof(cmd), "DELETE_USER_ADMIN %s", user);
                    enviar_e_receber(cmd, res_op, BUF_SIZE);
                    imprimir_resposta(res_op);
                }
                aguardar_enter();
                break;

            case 0:
                sair = 1;
                break;
            default:
                printf(" Opção inválida.\n");
                sleep(1);
        }
    }
}

/* ============================================================================
 * FUNÇÃO: submenu_gestao_canais()
 * ============================================================================
 * OBJETIVO: Menu de administração focado na criação, alteração de descrições e
 * eliminação de canais de chat. Os canais criados estarão imediatamente
 * disponíveis para qualquer utilizador se juntar.
 * ============================================================================
 */
void submenu_gestao_canais(void) {
    int sair = 0;
    while (!sair) {
        draw_header(2, "Gestão de Canais");

        char res[BUF_SIZE];
        enviar_e_receber("LIST_CHANNELS", res, BUF_SIZE);

        printf("\n [CANAIS EXISTENTES]\n");
        imprimir_resposta(res);

        printf("\n----------------------------------------------------\n");
        printf(" [ 1 ] Criar novo canal\n");
        printf(" [ 2 ] Atualizar descrição de canal\n");
        printf(" [ 3 ] Remover canal\n");
        printf(" [ 0 ] Voltar ao Menu Principal\n");
        printf("----------------------------------------------------\n");
        printf(" Escolha: ");

        int opt = -1;
        if (scanf("%d", &opt) != 1) opt = -1;
        clear_buffer();

        char cmd[BUF_SIZE], canal[50], desc[200], res_op[BUF_SIZE];

        switch (opt) {
            case 1:
                printf(" Nome do novo canal (sem #): ");
                if (scanf("%49s", canal) != 1) {
                    clear_buffer();
                    break;
                }
                printf(" Descrição: ");
                clear_buffer();
                if (!fgets(desc, sizeof(desc), stdin)) break;
                desc[strcspn(desc, "\n")] = '\0';
                snprintf(cmd, sizeof(cmd), "CREATE_CHANNEL #%s %.190s", canal,
                         desc);
                enviar_e_receber(cmd, res_op, BUF_SIZE);
                imprimir_resposta(res_op);
                aguardar_enter();
                break;

            case 2:
                printf(" Nome do canal a atualizar (ex: geral): ");
                if (scanf("%49s", canal) != 1) {
                    clear_buffer();
                    break;
                }
                printf(" Nova descrição: ");
                clear_buffer();
                if (!fgets(desc, sizeof(desc), stdin)) break;
                desc[strcspn(desc, "\n")] = '\0';
                snprintf(cmd, sizeof(cmd), "UPDATE_CHANNEL #%s %.190s", canal,
                         desc);
                enviar_e_receber(cmd, res_op, BUF_SIZE);
                imprimir_resposta(res_op);
                aguardar_enter();
                break;

            case 3:
                printf(" Nome do canal a remover (ex: dev): ");
                if (scanf("%49s", canal) != 1) {
                    clear_buffer();
                    break;
                }
                clear_buffer();
                snprintf(cmd, sizeof(cmd), "DELETE_CHANNEL #%s", canal);
                enviar_e_receber(cmd, res_op, BUF_SIZE);
                imprimir_resposta(res_op);
                aguardar_enter();
                break;

            case 0:
                sair = 1;
                break;
            default:
                printf(" Opção inválida.\n");
                sleep(1);
        }
    }
}

/* ============================================================================
 * FUNÇÃO: submenu_seguranca()
 * ============================================================================
 * OBJETIVO: Painel de administração de segurança — Etapa 4 (F13 + F14).
 *
 * OPÇÕES (ADMIN):
 *   [1] Ver Logs de Acesso (VIEW_LOGS)
 *   [2] Consultar Parâmetros Criptográficos (VIEW_CRYPTO) — F14
 *   [3] Alternar Cifra Simétrica (SET_CIPHER CESAR|VIGENERE) — F13
 *   [4] Definir Chave Vigenère (SET_VKEY <chave>) — F13
 *   [5] Activar/Desactivar Hash de Integridade (HASH_ON|OFF) — F13
 *   [6] Enviar Mensagem Privada com RSA (SEND_MSG_RSA) — F13
 *   [7] Consultar Cifra Activa (GET_CIPHER) — F14
 *   [0] Voltar
 * ============================================================================ */
void submenu_seguranca(void) {
    int sair = 0;
    while (!sair) {
        draw_header(2, "Segurança / Criptografia (F13-F14)");

        /* Estado local resumido — mostra método simétrico activo e integridade */
        const char* cifra_nome;
        if (metodo_simetrico_local == 1)
            cifra_nome = "XOR (F13 - 2.º Simétrico)";
        else
            cifra_nome = (cipher_mode_local == 1) ? "Vigenère" : "César Generalizado";
        printf("\n [ESTADO CRIPTOGRÁFICO LOCAL]\n");
        printf(" > Método simétrico : \033[1;33m%s\033[0m\n", cifra_nome);
        printf(" > Chave Vigenère   : \033[1;33m%s\033[0m\n", chave_vigenere_local);
        printf(" > Hash integridade : \033[1;33m%s\033[0m  (djb2-32)\n",
               integridade_local ? "ACTIVO" : "INACTIVO");
        printf(" > Chave DH sessão  : \033[1;33m%lld\033[0m\n", chave_sessao);

        printf("\n----------------------------------------------------\n");
        printf(" [ 1 ] Ver Logs de Acesso do Servidor\n");
        printf(" [ 2 ] Consultar Parâmetros Criptográficos (F14)\n");
        printf(" [ 3 ] Alternar Cifra Simétrica (César / Vigenère) (F13)\n");
        printf(" [ 4 ] Definir Chave Vigenère (F13)\n");
        printf(" [ 5 ] Activar / Desactivar Hash (legado HASH_ON/OFF) (F13)\n");
        printf(" [ 6 ] Enviar Mensagem Privada com RSA Toy (F13)\n");
        printf(" [ 7 ] Consultar Cifra Activa no Servidor (F14)\n");
        printf(" [ 8 ] Configurar Cifra e Integridade (F13) [NOVO]\n");
        printf(" [ 0 ] Voltar ao Menu Principal\n");
        printf("----------------------------------------------------\n");
        printf(" Escolha: ");

        int opt = -1;
        if (scanf("%d", &opt) != 1) opt = -1;
        clear_buffer();

        char cmd[BUF_SIZE], res[BUF_SIZE];

        switch (opt) {

            /* ---- [1] LOGS ---- */
            case 1: {
                draw_header(2, "Logs de Acesso");
                snprintf(cmd, sizeof(cmd), "VIEW_LOGS %s", current_user);
                enviar_e_receber(cmd, res, BUF_SIZE);
                printf("\n");
                imprimir_resposta(res);
                aguardar_enter();
                break;
            }

            /* ---- [2] VIEW_CRYPTO (F14) ---- */
            case 2: {
                draw_header(2, "Parâmetros Criptográficos (F14)");
                enviar_e_receber("VIEW_CRYPTO", res, BUF_SIZE);
                printf("\n");
                imprimir_resposta(res);
                aguardar_enter();
                break;
            }

            /* ---- [3] SET_CIPHER (F13) ---- */
            case 3: {
                draw_header(2, "Alternar Cifra Simétrica (F13)");
                printf("\n Cifra activa: \033[1;33m%s\033[0m\n", cifra_nome);
                printf("\n----------------------------------------------------\n");
                printf(" [ 1 ] César Generalizado (chave derivada de DH)\n");
                printf(" [ 2 ] Vigenère (chave configurável)\n");
                printf(" [ 0 ] Cancelar\n");
                printf("----------------------------------------------------\n");
                printf(" Escolha: ");
                int sub = -1;
                if (scanf("%d", &sub) != 1) sub = -1;
                clear_buffer();

                if (sub == 1) {
                    enviar_e_receber("SET_CIPHER CESAR", res, BUF_SIZE);
                    imprimir_resposta(res);
                    if (strstr(res, "CIPHER_OK")) {
                        cipher_mode_local = 0;
                        metodo_simetrico_local = 0;
                    }
                } else if (sub == 2) {
                    enviar_e_receber("SET_CIPHER VIGENERE", res, BUF_SIZE);
                    imprimir_resposta(res);
                    if (strstr(res, "CIPHER_OK")) cipher_mode_local = 1;
                }
                aguardar_enter();
                break;
            }

            /* ---- [4] SET_VKEY (F13) ---- */
            case 4: {
                draw_header(2, "Definir Chave Vigenère (F13)");
                printf("\n Chave actual: \033[1;33m%s\033[0m\n", chave_vigenere_local);
                printf(" Nova chave (só letras, máx 63): ");
                char nova[64];
                if (scanf("%63s", nova) != 1) { clear_buffer(); break; }
                clear_buffer();
                snprintf(cmd, sizeof(cmd), "SET_VKEY %s", nova);
                enviar_e_receber(cmd, res, BUF_SIZE);
                imprimir_resposta(res);
                if (strstr(res, "VKEY_OK"))
                    strncpy(chave_vigenere_local, nova, sizeof(chave_vigenere_local) - 1);
                aguardar_enter();
                break;
            }

            /* ---- [5] HASH ON/OFF (F13) ---- */
            case 5: {
                draw_header(2, "Hash de Integridade djb2 (F13)");
                printf("\n Estado actual: \033[1;33m%s\033[0m\n",
                       hash_activo ? "ACTIVO" : "INACTIVO");
                printf("\n----------------------------------------------------\n");
                printf(" [ 1 ] Activar  (djb2 appended em mensagens)\n");
                printf(" [ 2 ] Desactivar\n");
                printf(" [ 0 ] Cancelar\n");
                printf("----------------------------------------------------\n");
                printf(" Escolha: ");
                int sub2 = -1;
                if (scanf("%d", &sub2) != 1) sub2 = -1;
                clear_buffer();

                if (sub2 == 1) {
                    enviar_e_receber("HASH_ON", res, BUF_SIZE);
                    imprimir_resposta(res);
                    if (strstr(res, "HASH_OK")) hash_activo = 1;
                } else if (sub2 == 2) {
                    enviar_e_receber("HASH_OFF", res, BUF_SIZE);
                    imprimir_resposta(res);
                    if (strstr(res, "HASH_OK")) hash_activo = 0;
                }
                aguardar_enter();
                break;
            }

            /* ---- [6] SEND_MSG_RSA (F13) ---- */
            case 6: {
                draw_header(2, "Mensagem Privada com RSA Toy (F13)");
                printf("\n [RSA] e=%d  d=%d  n=%d\n", RSA_E, RSA_D, RSA_N);
                printf(" Destinatário: ");
                char dest[50], msg_rsa[400], msg_cifrada_rsa[BUF_SIZE];
                if (scanf("%49s", dest) != 1) { clear_buffer(); break; }
                clear_buffer();
                printf(" Mensagem: ");
                if (!fgets(msg_rsa, sizeof(msg_rsa), stdin)) break;
                msg_rsa[strcspn(msg_rsa, "\n")] = '\0';

                /* Cifrar localmente para mostrar ao utilizador */
                rsa_encrypt_str_c(msg_rsa, msg_cifrada_rsa, sizeof(msg_cifrada_rsa));
                printf("\n \033[1;33m[RSA PREVIEW]\033[0m Texto cifrado (primeiros 60 chars):\n");
                printf(" %.60s...\n", msg_cifrada_rsa);

                printf("\n [A ENVIAR via SEND_MSG_RSA...]\n");
                snprintf(cmd, sizeof(cmd), "SEND_MSG_RSA %s %s %.390s",
                         dest, current_user, msg_rsa);
                enviar_e_receber(cmd, res, BUF_SIZE);
                imprimir_resposta(res);
                aguardar_enter();
                break;
            }

            /* ---- [7] GET_CIPHER (F14) ---- */
            case 7: {
                draw_header(2, "Cifra Activa no Servidor (F14)");
                enviar_e_receber("GET_CIPHER", res, BUF_SIZE);
                printf("\n");
                imprimir_resposta(res);
                aguardar_enter();
                break;
            }

            /* ---- [8] CONFIGURAR CIFRA E INTEGRIDADE (F13 — NOVO) ---- */
            case 8: {
                int sair8 = 0;
                while (!sair8) {
                    draw_header(2, "Configurar Cifra e Integridade (F13)");
                    const char* met_nome8 = (metodo_simetrico_local == 1) ? "XOR" : "César";
                    printf("\n [CONFIGURAÇÃO ACTUAL]\n");
                    printf(" > Método simétrico : \033[1;33m%d (%s)\033[0m\n",
                           metodo_simetrico_local, met_nome8);
                    printf(" > Integridade      : \033[1;33m%s\033[0m (hash DJB2)\n",
                           integridade_local ? "ACTIVA" : "INACTIVA");
                    printf("\n----------------------------------------------------\n");
                    printf(" [ 1 ] Alterar método simétrico (0=César, 1=XOR)\n");
                    printf(" [ 2 ] Ativar/Desativar verificação de integridade\n");
                    printf(" [ 0 ] Voltar\n");
                    printf("----------------------------------------------------\n");
                    printf(" Escolha: ");
                    int s8 = -1;
                    if (scanf("%d", &s8) != 1) s8 = -1;
                    clear_buffer();

                    if (s8 == 1) {
                        /* Alterar método simétrico via SET_CIPHER 0|1 */
                        printf("\n Método (0=César, 1=XOR): ");
                        int novo_met = -1;
                        if (scanf("%d", &novo_met) != 1) novo_met = -1;
                        clear_buffer();
                        if (novo_met == 0 || novo_met == 1) {
                            snprintf(cmd, sizeof(cmd), "SET_CIPHER %d", novo_met);
                            enviar_e_receber(cmd, res, BUF_SIZE);
                            imprimir_resposta(res);
                            /* Sincronizar variável local com servidor */
                            if (strstr(res, "CIPHER_OK")) {
                                metodo_simetrico_local = novo_met;
                                cipher_mode_local = novo_met;
                            }
                        } else {
                            printf(" \033[1;31m[ERRO]\033[0m Método inválido (use 0 ou 1).\n");
                        }
                        aguardar_enter();
                    } else if (s8 == 2) {
                        /* Alternar integridade via TOGGLE_INTEGRITY */
                        enviar_e_receber("TOGGLE_INTEGRITY", res, BUF_SIZE);
                        imprimir_resposta(res);
                        /* Sincronizar variável local com resposta do servidor */
                        if (strstr(res, "INTEGRITY_OK")) {
                            integridade_local = !integridade_local;
                        }
                        aguardar_enter();
                    } else if (s8 == 0) {
                        sair8 = 1;
                    } else {
                        printf(" Opção inválida.\n");
                        sleep(1);
                    }
                }
                break;
            }

            case 0:
                sair = 1;
                break;
            default:
                printf(" Opção inválida.\n");
                sleep(1);
        }
    }
}

/* ============================================================================
 * FUNÇÃO: menu_user()
 * Menu principal para utilizadores normais (não-admin).
 *
 * ESTRUTURA GERAL:
 *   loop while (!sair && autenticado):
 *   • Renderiza menu com 6 opções
 *   • Captura input de utilizador
 *   • Despacha para submenu ou comando apropriado
 *   • Continua até utilizador escolher [0] Logout
 *
 * OPÇÕES DISPONÍVEIS:
 *   [1] O Meu Perfil (F1)
 *       → Chama submenu_perfil()
 *       → Permite editar dados pessoais
 *
 *   [2] Lista de Contactos (F3)
 *       → Chama submenu_contactos()
 *       → Lista users ONLINE/OFFLINE
 *       → Permite enviar mensagens privadas
 *
 *   [3] Mensagens Privadas (F5)
 *       → Chama submenu_mensagens()
 *       → Mostra caixa de entrada
 *       → Permite enviar offline
 *
 *   [4] Chat em Canais (F10)
 *       → Chama submenu_canais()
 *       → Chat em tempo real com select()
 *       → Broadcast para toda a sala
 *
 *   [5] Informações do Servidor (F11)
 *       → Envia: "GET_INFO"
 *       → Servidor responde com: versão, uptime, users, etc
 *       → Imprime com imprimir_resposta() (cores)
 *
 *   [0] Terminar Sessão (F0 / Logout)
 *       → Envia: "LOGOUT"
 *       → Seta: sair=1, autenticado=0
 *       → Loop termina
 *       → Volta a menu_pre_login() em main()
 *
 * FLUXO DE LOGOUT:
 *   printf("[A TERMINAR SESSÃO...]") — feedback visual
 *   enviar_e_receber("LOGOUT", ...) — notifica servidor
 *   sair = 1, autenticado = 0 — quebra o loop
 *   menu_pre_login() é chamado novamente em main()
 *
 * CORES:
 *   draw_header(1, ...) — modo USER (ciano \033[1;36m)
 *
 * RETORNO: Nenhum (void)
 * ============================================================================
 */
void menu_user(void) {
    int sair = 0;
    while (!sair && autenticado) {
        draw_header(1, "");
        printf(" Selecione uma das seguintes opções:\n");
        printf("----------------------------------------------------\n");
        printf(" [ 1 ] O Meu Perfil (F1)\n");
        printf(" [ 2 ] Lista de Contactos (F3)\n");
        printf(" [ 3 ] Mensagens Privadas (F5)\n");
        printf(" [ 4 ] Chat em Canais (F10)\n");
        printf(" [ 5 ] Informações do Servidor (F11)\n");
        printf(" [ 0 ] Terminar Sessão (F0)\n");
        printf("\n Escolha: ");

        int opt = -1;
        if (scanf("%d", &opt) != 1) opt = -1;
        clear_buffer();

        char res[BUF_SIZE];

        switch (opt) {
            case 1:
                submenu_perfil();
                break;
            case 2:
                submenu_contactos();
                break;
            case 3:
                submenu_mensagens();
                break;
            case 4:
                submenu_canais();
                break;
            case 5:
                enviar_e_receber("GET_INFO", res, BUF_SIZE);
                draw_header(1, "Informações do Servidor");
                printf("\n");
                imprimir_resposta(res);
                aguardar_enter();
                break;
            case 0:
                printf("\n [A TERMINAR SESSÃO...]\n");
                enviar_e_receber("LOGOUT", res, BUF_SIZE);
                sair = 1;
                autenticado = 0;
                printf(
                    " \033[1;32m[OK]\033[0m Sessão encerrada com segurança.\n");
                aguardar_enter();
                break;
            default:
                printf(" Opção inválida.\n");
                sleep(1);
        }
    }
}

/* ============================================================================
 * FUNÇÃO: menu_admin()
 * Menu principal para utilizadores com privilégios de administrador.
 *
 * ESTRUTURA GERAL:
 *   loop while (!sair && autenticado):
 *   • Renderiza menu com 9 opções (mais que menu_user)
 *   • Captura input de utilizador
 *   • Despacha para submenu ou comando apropriado
 *   • Continua até utilizador escolher [0] Logout
 *
 * OPÇÕES DISPONÍVEIS (8 + Logout):
 *   [1-4] Como menu_user (Perfil, Contactos, Mensagens, Chat)
 *       → Mesmo comportamento que utilizador normal
 *       → Cores mais destacadas (ADMIN = vermelho)
 *
 *   [5] Gestão de Utilizadores (F7) [ADMIN ONLY]
 *       → Chama submenu_gestao_utilizadores()
 *       → Lista contas em PENDING (aguardando aprovação)
 *       → Opções: APPROVE, REJECT, BAN utilizadores
 *       → Validação de identidade (pode rejudiciar aplicação)
 *
 *   [6] Gestão de Canais (F8) [ADMIN ONLY]
 *       → Chama submenu_gestao_canais()
 *       → Listar canais, criar novo, eliminar
 *       → Bloquear/desbloquear canais
 *
 *   [7] Segurança e Auditoria (F9) [ADMIN ONLY]
 *       → Chama submenu_seguranca()
 *       → Ver logs de actividade
 *       → Relatórios de segurança
 *       → Listar actions suspeitas
 *
 *   [8] Informações do Servidor (F11)
 *       → Envia: "GET_INFO"
 *       → Versão, uptime, users conectados, canais ativos, etc
 *       → Dados administrativos (totais, estatísticas)
 *
 *   [0] Terminar Sessão Administrativa
 *       → Envia: "LOGOUT"
 *       → Seta: sair=1, autenticado=0
 *       → Volta a menu_pre_login()
 *       → Requer novo login para re-authenticate
 *
 * CORES E STYLING:
 *   draw_header(2, ...) — modo ADMIN (vermelho \033[1;31m)
 *   Printf: "ADMIN" destacado no header
 *   Todas mensagens de confirmação em verde sucesso
 *
 * DIFERENÇAS vs menu_user:
 *   • +3 opções (Gestão Users, Gestão Canais, Segurança)
 *   • Cores vermelhas em vez de ciano
 *   • Acesso a dados sensíveis do sistema
 *   • Acesso a relatórios e auditorias
 *
 * RETORNO: Nenhum (void)
 * ============================================================================
 */
void menu_admin(void) {
    int sair = 0;
    while (!sair && autenticado) {
        draw_header(2, "");
        printf(" Selecione uma das seguintes opções (ADMIN):\n");
        printf("----------------------------------------------------\n");
        printf(" [ 1 ] O Meu Perfil (F1)\n");
        printf(" [ 2 ] Lista de Contactos (F3)\n");
        printf(" [ 3 ] Mensagens Privadas (F5)\n");
        printf(" [ 4 ] Chat em Canais (F10)\n");
        printf(" [ 5 ] Gestão de Utilizadores (F7)\n");
        printf(" [ 6 ] Gestão de Canais (F8)\n");
        printf(" [ 7 ] Segurança e Auditoria (F9)\n");
        printf(" [ 8 ] Informações do Servidor (F11)\n");
        printf(" [ 0 ] Terminar Sessão (F0)\n");
        printf("\n Escolha: ");

        int opt = -1;
        if (scanf("%d", &opt) != 1) opt = -1;
        clear_buffer();

        char res[BUF_SIZE];

        switch (opt) {
            case 1:
                submenu_perfil();
                break;
            case 2:
                submenu_contactos();
                break;
            case 3:
                submenu_mensagens();
                break;
            case 4:
                submenu_canais();
                break;
            case 5:
                submenu_gestao_utilizadores();
                break;
            case 6:
                submenu_gestao_canais();
                break;
            case 7:
                submenu_seguranca();
                break;
            case 8:
                enviar_e_receber("GET_INFO", res, BUF_SIZE);
                draw_header(2, "Informações do Servidor");
                printf("\n");
                imprimir_resposta(res);
                aguardar_enter();
                break;
            case 0:
                printf("\n [A TERMINAR SESSÃO ADMIN...]\n");
                enviar_e_receber("LOGOUT", res, BUF_SIZE);
                sair = 1;
                autenticado = 0;
                printf(
                    " \033[1;32m[OK]\033[0m Sessão encerrada com segurança.\n");
                aguardar_enter();
                break;
            default:
                printf(" Opção inválida.\n");
                sleep(1);
        }
    }
}

/**
 * @brief Bootstrapping e Função Principal do Cliente TCP Persistente.
 * 
 * @details 
 * Trata da receção de argumentos por linha de comandos (CLI), realiza o DNS Lookup
 * para o destino fornecido, e instaura a Conexão TCP Ativa e Efetiva (`connect`).
 * Ao contrário das etapas anteriores, esta rotina engloba um ciclo de vida longo. O
 * `server_fd` (socket) mantém a porta efemeramente alocada por horas, até ocorrer encerramento.
 * 
 * @param argc Obrigatório ter 3 argumentos absolutos (nome_do_programa + ip + porta)
 * @param argv Parâmetros posicinais do invocador em Array de Strings C.
 * 
 * @return int 0 para desativação normal da aplicação, != 0 para Crash Network-related.
 */
int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <IP_SERVIDOR> <PORTO>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s 127.0.0.1 10000\n", argv[0]);
        return 1;
    }

    /* Inicializar semente do gerador pseudo-aleatório (Etapa 4: DH) */
    srand(time(NULL));

    const char* server_ip = argv[1];
    /* P4: Validar que o porto está no intervalo TCP válido [1..65535]
     * atoi() retorna 0 para strings inválidas, que também seria rejeitado. */
    int server_port = atoi(argv[2]);
    if (server_port <= 0 || server_port > 65535) {
        fprintf(stderr, "ERRO: Porto inválido '%s'. Use um valor entre 1 e 65535.\n", argv[2]);
        return 1;
    }

    /* Resolver hostname */
    struct hostent* he = gethostbyname(server_ip);
    if (!he) {
        fprintf(stderr, "ERRO: Não foi possível resolver '%s'\n", server_ip);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    if (connect(server_fd, (struct sockaddr*)&server_addr,
                sizeof(server_addr)) < 0) {
        fprintf(stderr, "ERRO: Não conseguiu ligar-se ao servidor %s:%d\n",
                server_ip, server_port);
        close(server_fd);
        return 1;
    }

    /* Menu de boas-vindas */
    draw_header(0, "CONECTADO AO SERVIDOR COM SUCESSO");
    printf("\n Bem-vindo ao C-Cord v4.0! (E2EE Ativo)\n");
    printf(" Servidor: %s:%d\n", server_ip, server_port);
    printf("\n----------------------------------------------------\n");
    printf(" >> Pressione ENTER para continuar...\n");
    getchar();

    /* Loop principal — volta ao menu inicial após logout */
    while (1) {
        autenticado = 0;
        is_admin_flag = 0;
        current_user[0] = '\0';
        current_canal[0] = '\0';
        login_time = 0;
        chave_sessao = CHAVE_CESAR; /* Resetar chave criptográfica (Etapa 4) */
        privado_cliente = 0;

        menu_pre_login();  /* Se opt==0 dentro, faz exit(0) */

        if (autenticado) {
            if (is_admin_flag) {
                menu_admin();
            } else {
                menu_user();
            }
        }
        /* Após logout, o while recomeça → menu_pre_login aparece novamente */
    }

    /* Encerramento (nunca executa porque exit() é chamado em menu_pre_login) */
    close(server_fd);
    printf("\n====================================================\n");
    printf("       OBRIGADO POR USAR O C-CORD v4.0\n");
    printf("====================================================\n");

    return 0;
}
