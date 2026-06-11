/*
 * ============================================================================
 * SERVIDOR TCP C-CORD — VERSÃO 3.0 (Etapa 3: Select + Canais + Broadcast)
 * ============================================================================
 *
 * [REVISÃO DE CÓDIGO CONCLUÍDA]: Base de código validada para a Etapa 3.
 * 
 * Descrição:
 *   Servidor TCP que implementa concorrência com select() para múltiplos
 * clientes. Suporta canais, broadcasts, e comunicação persistente (Etapa 3).
 *   Ligações persistentes: socket do cliente fica aberto enquanto autenticado.
 *
 * Compilação: gcc -Wall -Wextra -o server_linux server_etapa3.c
 * Execução  : ./server_linux
 * Porto     : 10000
 *
 * ============================================================================
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define SERVER_PORT 10000
#define BUF_SIZE 4096
#define MAX_CLIENTES 50
#define USERS_FILE "users.txt"
#define INBOX_FILE "inbox.txt"
#define LOG_FILE "logs.txt"
#define MAX_USERS 200

/* ============================================================================
 * ESTRUTURA DE CLIENTE (ETAPA 3)
 * ============================================================================
 * OBJETIVO: Rastrear estado de cada cliente conectado (ligações persistentes)
 *
 * CAMPOS:
 *   - fd: File descriptor do socket TCP (=-1 se slot vazio/disponível)
 *   - username: Nome do utilizador autenticado (vazio se não autenticado)
 *   - canal: Canal actual onde cliente está (#geral, #admin, #privado, etc)
 *   - autenticado: Flag binária (0=não autenticado, 1=autenticado e ativo)
 *
 * EXPLICAÇÃO ETAPA 3:
 *   Na Etapa 2, servidor era sequencial: accept() → read() → close()
 *   Na Etapa 3, servidor usa select() multiplex + array persistente:
 *
 *   - Array global Cliente clientes[MAX_CLIENTES] (50 slots)
 *   - Cada socket de cliente ocupa um slot
 *   - Socket fica ABERTO durante toda sessão (não fecha após comando)
 *   - Servidor envia broadcasts a todos com autenticado=1 no mesmo canal
 *
 * CICLO DE VIDA:
 *   1. Client conecta → servidor accept() → fd preenchido
 *   2. Client envia AUTH → fd.autenticado=1, fd.username preenchido
 *   3. Client envia JOIN → fd.canal = "#geral" (ou outro canal)
 *   4. Client ativo recebe múltiplos comandos (socket permanece aberto)
 *   5. Client desconecta (recv()<=0) → fd=-1, socket fechado, slot liberado
 */
typedef struct {
    int fd;            /* Socket file descriptor (-1 = slot vazio) */
    char username[50]; /* Utilizador autenticado (vazio se não autenticado) */
    char canal[50];    /* Canal onde cliente está (#geral, #admin, etc) */
    int autenticado;   /* Flag: 0=não autenticado, 1=autenticado e ligado */
} Cliente;

Cliente clientes[MAX_CLIENTES]; /* Array global de clientes */

const char* VERSAO_SERVIDOR = "3.0-Etapa3";
time_t start_time;
int total_pedidos = 0;

/* ============================================================================
 * FUNÇÃO: guardar_log()
 * ============================================================================
 * OBJETIVO: Registar eventos do servidor em ficheiro e ecrã
 *
 * PARÂMETROS:
 *   mensagem: Texto do evento a registar (ex: "Cliente conectou", "AUTH
 * bem-sucedida") tipo: Tipo de mensagem (afeta cor e prefix) 1 = OK (verde) —
 * operação bem-sucedida 2 = INFO (ciano) — informação neutra 3 = ERRO
 * (vermelho) — erro ou aviso
 *
 * EXPLICAÇÃO:
 *   1. Abre ficheiro logs.txt em modo append ("a")
 *   2. Formata timestamp atual (YYYY-MM-DD HH:MM:SS)
 *   3. Escreve em ficheiro: "[timestamp] mensagem"
 *   4. Mostrar também no ecrã do servidor com cor apropriada
 *
 * FICHEIRO LOG:
 *   Útil para auditar: quem fez login, quem saiu, erros, etc.
 *   Path: logs.txt (no directório onde servidor executa)
 *   Formato: [2026-05-25 16:41:55] Cliente 192.168.1.100:5000 conectou
 *
 * EXEMPLO DE SAÍDA:
 *   [OK]    | Cliente conectou em slot 3
 *   [INFO]  | Servidor iniciado na porta 10000
 *   [ERRO]  | Falha ao ler ficheiro users.txt
 */
void guardar_log(const char* mensagem, int tipo) {
    /* ===== ESCREVER EM FICHEIRO ===== */
    FILE* f = fopen(LOG_FILE, "a"); /* Abrir em append mode */
    if (f) {
        char data[64];
        time_t agora = time(NULL);
        struct tm* t = localtime(&agora);
        /* Formatar: YYYY-MM-DD HH:MM:SS */
        strftime(data, sizeof(data), "%Y-%m-%d %H:%M:%S", t);
        fprintf(f, "[%s] %s\n", data, mensagem);
        fclose(f);
    }

    /* ===== MOSTRAR NO ECRÃ COM COR ===== */
    if (tipo == 1)
        printf(" \033[1;32m[OK]\033[0m    | %s\n", mensagem); /* Verde */
    else if (tipo == 3)
        printf(" \033[1;31m[ERRO]\033[0m  | %s\n", mensagem); /* Vermelho */
    else
        printf(" \033[1;36m[INFO]\033[0m  | %s\n", mensagem); /* Ciano */
}

/* ============================================================================
 * FUNÇÃO: proximo_id()
 * ============================================================================
 * OBJETIVO: Gerar próximo ID numérico único para novo utilizador
 *
 * RETORNO:
 *   Inteiro: ID mais alto encontrado em users.txt + 1
 *   Se ficheiro não existe ou está vazio: retorna 1
 *
 * EXPLICAÇÃO:
 *   Formato do ficheiro users.txt:
 *   1:joao:password123:USER:ACTIVE
 *   2:maria:pass456:USER:PENDING
 *   3:admin:admin123:ADMIN:ACTIVE
 *
 *   Esta função:
 *   1. Abre ficheiro em modo leitura
 *   2. Lê cada linha
 *   3. Extrai número do ID (antes de ':')
 *   4. Mantém track do ID máximo encontrado
 *   5. Fecha ficheiro
 *   6. Retorna max_id + 1
 *
 * EXEMPLO:
 *   Se users.txt tem IDs 1,2,3 → retorna 4 (próximo novo utilizador)
 */
int proximo_id() {
    FILE* f = fopen(USERS_FILE, "r");
    if (!f) return 1; /* Se ficheiro não existe, começar em 1 */

    char line[800];
    int max_id = 0, id = 0;

    /* Ler cada linha do ficheiro */
    while (fgets(line, sizeof(line), f)) {
        /* Extrair número do ID (primeiro campo, antes de ':') */
        if (sscanf(line, "%d:", &id) == 1 && id > max_id) {
            max_id = id;
        }
    }
    fclose(f);
    return max_id + 1; /* Retornar próximo ID disponível */
}

void desenhar_cabecalho_servidor() {
    system("clear");
    printf("\033[1;36m");
    printf("   ____         ____ ___  ____  ____    \n");
    printf("  / ___|       / ___/ _ \\|  _ \\|  _ \\   \n");
    printf(" | |     ____ | |  | | | | |_) | | | |  \n");
    printf(" | |___ |____|| |__| |_| |  _ <| |_| |  \n");
    printf("  \\____|       \\____\\___/|_| \\_\\____/   \n");
    printf("\033[0m\n");
    printf(
        "======================================================================"
        "\n");
    printf(
        "         C-CORD SERVER v%s (Etapa 3 — Select + Canais)             \n",
        VERSAO_SERVIDOR);
    printf(
        "======================================================================"
        "\n");
    printf(" STATUS: \033[1;32mONLINE\033[0m | PORTO: %d | BD: %s\n",
           SERVER_PORT, USERS_FILE);
    printf(
        "----------------------------------------------------------------------"
        "\n");
    printf(" LIVE FEED DE ATIVIDADE:\n");
}

/* ============================================================================
 * FUNÇÃO: check_auth()
 * ============================================================================
 * OBJETIVO: Verificar credenciais de login num ficheiro de utilizadores
 *
 * PARÂMETROS:
 *   username: Nome de utilizador a verificar
 *   password: Palavra-passe a verificar
 *   role: Output → preenchido com "USER" ou "ADMIN" se sucesso
 *
 * RETORNO:
 *   1 = Autenticação bem-sucedida (credenciais correctas, conta ativa)
 *   -1 = Conta pendente (à espera de aprovação admin)
 *   -2 = Conta inactiva (suspensa)
 *   0 = Falha (credenciais incorrectas ou utilizador não existe)
 *
 * EXPLICAÇÃO:
 *   1. Abre ficheiro users.txt
 *   2. Lê cada linha (formato: ID:user:pass:role:status)
 *   3. Compara username e password com parâmetros
 *   4. Se encontrado:
 *      - Verifica status (PENDING → retorna -1, INACTIVE → retorna -2)
 *      - Se ACTIVE: copia role para output e retorna 1
 *   5. Se não encontrado: retorna 0
 *
 * FORMATO FICHEIRO:
 *   1:joao:password123:USER:ACTIVE
 *   2:maria:pass456:USER:PENDING
 *   3:admin:admin123:ADMIN:ACTIVE
 */
int check_auth(const char* username, const char* password, char* role) {
    FILE* f = fopen(USERS_FILE, "r");
    if (!f) {
        guardar_log("users.txt nao encontrado!", 3);
        return 0;
    }

    char line[256], id[10], u[50], p[50], r[20], s[20];

    /* Ler cada linha do ficheiro */
    while (fgets(line, sizeof(line), f)) {
        /* Extrair campos: ID:username:password:role:status */
        if (sscanf(line, "%9[^:]:%49[^:]:%49[^:]:%19[^:]:%19s", id, u, p, r,
                   s) == 5) {
            /* Se username e password coincidem */
            if (strcmp(u, username) == 0 && strcmp(p, password) == 0) {
                fclose(f);
                /* Verificar status da conta */
                if (strcmp(s, "PENDING") == 0) return -1;  /* Pendente */
                if (strcmp(s, "INACTIVE") == 0) return -2; /* Inactiva */
                strcpy(role, r); /* Copiar role (USER ou ADMIN) */
                return 1;        /* Sucesso */
            }
        }
    }
    fclose(f);
    return 0; /* Utilizador não encontrado ou password incorrecta */
}

int is_admin(const char* username) {
    FILE* f = fopen(USERS_FILE, "r");
    if (!f) return 0;
    char line[256], id[10], u[50], p[50], r[20], s[20];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%9[^:]:%49[^:]:%49[^:]:%19[^:]:%19s", id, u, p, r,
                   s) == 5) {
            if (strcmp(u, username) == 0 && strcmp(r, "ADMIN") == 0 &&
                strcmp(s, "ACTIVE") == 0) {
                fclose(f);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
}

void list_all(char* response) {
    FILE* f = fopen(USERS_FILE, "r");
    if (!f) {
        strcpy(response, "ERRO: Ficheiro de utilizadores nao encontrado.");
        return;
    }

    strcpy(response,
           "=== UTILIZADORES REGISTADOS ===\n"
           " ID  | Utilizador       | Funcao  | Estado   \n"
           "-----+------------------+---------+----------\n");

    char line[256], id[10], u[50], p[50], r[20], s[20];
    int total = 0;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%9[^:]:%49[^:]:%49[^:]:%19[^:]:%19s", id, u, p, r,
                   s) == 5) {
            char entry[128];
            sprintf(entry, " %-3s | %-16s | %-7s | %s\n", id, u, r, s);
            strncat(response, entry, BUF_SIZE - strlen(response) - 1);
            total++;
        }
    }
    fclose(f);

    char footer[64];
    sprintf(footer, "-----\n Total: %d registo(s)\n", total);
    strncat(response, footer, BUF_SIZE - strlen(response) - 1);
}

void list_pending(char* response) {
    FILE* f = fopen(USERS_FILE, "r");
    if (!f) {
        strcpy(response, "ERRO: Ficheiro nao encontrado.");
        return;
    }

    strcpy(response,
           "=== UTILIZADORES PENDENTES ===\n"
           " ID  | Utilizador       | Estado   \n"
           "-----+------------------+----------\n");

    char line[256], id[10], u[50], p[50], r[20], s[20];
    int total = 0;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%9[^:]:%49[^:]:%49[^:]:%19[^:]:%19s", id, u, p, r,
                   s) == 5) {
            if (strcmp(s, "PENDING") == 0) {
                char entry[128];
                sprintf(entry, " %-3s | %-16s | %s\n", id, u, s);
                strncat(response, entry, BUF_SIZE - strlen(response) - 1);
                total++;
            }
        }
    }
    fclose(f);

    if (total == 0)
        strncat(response, " (sem utilizadores pendentes)\n",
                BUF_SIZE - strlen(response) - 1);
}

void check_inbox(const char* username, char* response) {
    FILE* f = fopen(INBOX_FILE, "r");
    if (!f) {
        strcpy(response, "A sua caixa de entrada esta vazia.");
        return;
    }

    sprintf(response, "=== CAIXA DE ENTRADA DE %s ===\n", username);
    char line[512], dest[50], from[50], msg[400];
    int count = 0;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (sscanf(line, "%49[^:]:%49[^:]:%399[^\n]", dest, from, msg) == 3) {
            if (strcmp(dest, username) == 0) {
                char entry[512];
                /* msg já contém "[YYYY-MM-DD HH:MM:SS] texto" */
                sprintf(entry, " [%d] De: %s | %s\n", ++count, from, msg);
                        /* Verificar se a mensagem possui timestamp (nova versao) */
                        if (msg[0] == '[' && strlen(msg) > 21 && msg[20] == ']') {
                            char data_hora[25];
                            strncpy(data_hora, msg + 1, 19);
                            data_hora[19] = '\0';
                            char* texto_real = msg + 22; /* Salta o "[YYYY-MM-DD HH:MM:SS] " */
                            sprintf(entry, " [%d] De: %-8s | Data: %s | Msg: %s\n", ++count, from, data_hora, texto_real);
                        } else {
                            /* Mensagem antiga sem timestamp */
                            sprintf(entry, " [%d] De: %-8s | Data: (Antiga)          | Msg: %s\n", ++count, from, msg);
                        }
                strncat(response, entry, BUF_SIZE - strlen(response) - 1);
            }
        }
    }
    fclose(f);

    if (count == 0)
        strncat(response, " (sem mensagens novas)\n",
                BUF_SIZE - strlen(response) - 1);
}

void send_msg(const char* dest, const char* from, const char* msg,
              char* response) {
    FILE* f = fopen(USERS_FILE, "r");
    int found = 0;
    if (f) {
        char line[256], id[10], u[50], p[50], r[20], s[20];
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "%9[^:]:%49[^:]:%49[^:]:%19[^:]:%19s", id, u, p, r,
                       s) == 5) {
                if (strcmp(u, dest) == 0) {
                    found = 1;
                    break;
                }
            }
        }
        fclose(f);
    }
    if (!found) {
        sprintf(response, "MSG_FAIL: Utilizador '%s' nao encontrado.", dest);
        return;
    }

    f = fopen(INBOX_FILE, "a");
    if (!f) {
        strcpy(response, "ERRO: Nao foi possivel guardar mensagem.");
        return;
    }
    
    /* Adicionar data/hora à mensagem privada */
    char data_hora[20];
    time_t agora = time(NULL);
    struct tm *t = localtime(&agora);
    strftime(data_hora, sizeof(data_hora), "%Y-%m-%d %H:%M:%S", t);
    
    fprintf(f, "%s:%s:[%s] %s\n", dest, from, data_hora, msg);
    fclose(f);
    sprintf(response, "MSG_SENT: Mensagem entregue na caixa de %s.", dest);
}

void register_user(const char* username, const char* password, char* response) {
    FILE* f = fopen(USERS_FILE, "r");
    if (f) {
        char line[256], id[10], u[50];
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "%9[^:]:%49[^:]", id, u) >= 2) {
                if (strcmp(u, username) == 0) {
                    fclose(f);
                    strcpy(response, "REGISTER_FAIL: Utilizador ja existe.");
                    return;
                }
            }
        }
        fclose(f);
    }

    int novo_id = proximo_id();
    f = fopen(USERS_FILE, "a");
    if (!f) {
        strcpy(response, "ERRO: Nao foi possivel aceder ao ficheiro.");
        return;
    }
    fprintf(f, "%d:%s:%s:USER:PENDING\n", novo_id, username, password);
    fclose(f);
    sprintf(response,
            "REGISTER_OK: Utilizador '%s' registado (ID=%d). Aguarda aprovacao "
            "do administrador.",
            username, novo_id);
}

void approve_user(const char* admin_user, const char* target, char* response) {
    if (!is_admin(admin_user)) {
        strcpy(response, "APPROVE_FAIL: Sem permissoes de administrador.");
        return;
    }

    FILE* f = fopen(USERS_FILE, "r");
    if (!f) {
        strcpy(response, "ERRO: Ficheiro nao encontrado.");
        return;
    }

    char lines[MAX_USERS][256];
    int count = 0, found = 0;
    while (fgets(lines[count], sizeof(lines[count]), f) && count < MAX_USERS)
        count++;
    fclose(f);

    f = fopen(USERS_FILE, "w");
    if (!f) {
        strcpy(response, "ERRO: Nao foi possivel actualizar ficheiro.");
        return;
    }

    for (int i = 0; i < count; i++) {
        char id[10], u[50], p[50], r[20], s[20];
        lines[i][strcspn(lines[i], "\n")] = 0;

        if (sscanf(lines[i], "%9[^:]:%49[^:]:%49[^:]:%19[^:]:%19s", id, u, p, r,
                   s) == 5) {
            if (strcmp(u, target) == 0 && strcmp(s, "PENDING") == 0) {
                fprintf(f, "%s:%s:%s:%s:ACTIVE\n", id, u, p, r);
                found = 1;
            } else {
                fprintf(f, "%s\n", lines[i]);
            }
        } else if (strlen(lines[i]) > 0) {
            fprintf(f, "%s\n", lines[i]);
        }
    }
    fclose(f);

    if (found)
        sprintf(response,
                "APPROVE_OK: Utilizador '%s' aprovado. Pode agora autenticar.",
                target);
    else
        sprintf(
            response,
            "APPROVE_FAIL: Utilizador '%s' nao encontrado ou ja esta activo.",
            target);
}

void suspend_user(const char* admin_user, const char* target, char* response) {
    if (!is_admin(admin_user)) {
        strcpy(response, "SUSPEND_FAIL: Sem permissoes de administrador.");
        return;
    }
    if (strcmp(admin_user, target) == 0) {
        strcpy(response,
               "SUSPEND_FAIL: Nao e possivel suspender a propria conta.");
        return;
    }

    FILE* f = fopen(USERS_FILE, "r");
    if (!f) {
        strcpy(response, "ERRO: Ficheiro nao encontrado.");
        return;
    }

    char lines[MAX_USERS][256];
    int count = 0, found = 0;
    while (fgets(lines[count], sizeof(lines[count]), f) && count < MAX_USERS)
        count++;
    fclose(f);

    f = fopen(USERS_FILE, "w");
    if (!f) {
        strcpy(response, "ERRO: Nao foi possivel actualizar ficheiro.");
        return;
    }

    char novo_estado[20] = "";
    for (int i = 0; i < count; i++) {
        char id[10], u[50], p[50], r[20], s[20];
        lines[i][strcspn(lines[i], "\n")] = 0;

        if (sscanf(lines[i], "%9[^:]:%49[^:]:%49[^:]:%19[^:]:%19s", id, u, p, r,
                   s) == 5) {
            if (strcmp(u, target) == 0 && strcmp(s, "PENDING") != 0) {
                const char* ns =
                    (strcmp(s, "ACTIVE") == 0) ? "INACTIVE" : "ACTIVE";
                fprintf(f, "%s:%s:%s:%s:%s\n", id, u, p, r, ns);
                strcpy(novo_estado, ns);
                found = 1;
            } else {
                fprintf(f, "%s\n", lines[i]);
            }
        } else if (strlen(lines[i]) > 0) {
            fprintf(f, "%s\n", lines[i]);
        }
    }
    fclose(f);

    if (found)
        sprintf(response, "SUSPEND_OK: Estado de '%s' alterado para %s.",
                target, novo_estado);
    else
        sprintf(response,
                "SUSPEND_FAIL: Utilizador '%s' nao encontrado ou esta PENDING.",
                target);
}

void delete_user(const char* admin_user, const char* target, char* response) {
    if (!is_admin(admin_user)) {
        strcpy(response, "DELETE_FAIL: Sem permissoes de administrador.");
        return;
    }
    if (strcmp(admin_user, target) == 0) {
        strcpy(response,
               "DELETE_FAIL: Nao e possivel apagar a propria conta de "
               "administrador.");
        return;
    }

    FILE* f = fopen(USERS_FILE, "r");
    if (!f) {
        strcpy(response, "ERRO: Ficheiro nao encontrado.");
        return;
    }

    char lines[MAX_USERS][256];
    int count = 0, found = 0;
    while (fgets(lines[count], sizeof(lines[count]), f) && count < MAX_USERS) {
        lines[count][strcspn(lines[count], "\n")] = 0;
        count++;
    }
    fclose(f);

    f = fopen(USERS_FILE, "w");
    if (!f) {
        strcpy(response, "ERRO: Nao foi possivel actualizar ficheiro.");
        return;
    }

    for (int i = 0; i < count; i++) {
        char id[10], u[50];
        if (sscanf(lines[i], "%9[^:]:%49[^:]", id, u) >= 2 &&
            strcmp(u, target) == 0) {
            found = 1;
        } else if (strlen(lines[i]) > 0) {
            fprintf(f, "%s\n", lines[i]);
        }
    }
    fclose(f);

    if (found)
        sprintf(response, "DELETE_OK: Utilizador '%s' removido do sistema.",
                target);
    else
        sprintf(response, "DELETE_FAIL: Utilizador '%s' nao encontrado.",
                target);
}

void view_logs(const char* admin_user, char* response) {
    if (!is_admin(admin_user)) {
        strcpy(response, "LOGS_FAIL: Sem permissoes de administrador.");
        return;
    }
    FILE* f = fopen(LOG_FILE, "r");
    if (!f) {
        strcpy(response, "=== LOGS ===\n (ficheiro vazio ou inexistente)\n");
        return;
    }

    strcpy(response, "=== REGISTO DE ATIVIDADE ===\n");
    char line[800];
    int count = 0;
    char buffer[200][256];
    while (fgets(line, sizeof(line), f) && count < 200)
        strcpy(buffer[count++], line);
    fclose(f);

    int start = (count > 50) ? count - 50 : 0;
    for (int i = start; i < count; i++)
        strncat(response, buffer[i], BUF_SIZE - strlen(response) - 1);
}

/* ============================================================================
 * ETAPA 3: NOVOS COMANDOS
 * ============================================================================
 */

/* ============================================================================
 * FUNÇÃO: handle_join(int client_idx, const char* canal_nome, char* response)
 * Processa comando JOIN: utilizador entra num canal específico.
 *
 * PARÂMETROS:
 *   client_idx — Índice no array clientes[] (0-49)
 *   canal_nome — Nome do canal (ex: "#geral" ou "geral")
 *   response   — Buffer para resposta ao cliente
 *
 * FLUXO:
 *   1. Validações:
 *      • client_idx deve estar entre 0 e MAX_CLIENTES-1
 *      • Cliente deve estar autenticado (is_admin_flag ou USER)
 *      • canal_nome não pode estar vazio
 *   2. Normalizar nome do canal:
 *      • Se não começa com '#', adiciona prefix
 *      • Resulta em: "#geral", "#linux", "#ajuda", "#custom", etc
 *   3. Atualizar estado:
 *      • strcpy(clientes[client_idx].canal, canal)
 *      • Este cliente fica a escutar este canal
 *      • Pode receber BROADCASTSs do canal
 *   4. Responder ao cliente:
 *      • "JOIN_OK: Entrou no canal #geral"
 *
 * NOTA ETAPA 3:
 *   Na Etapa 2 (sequencial), JOIN era simples (sem persistência)
 *   Na Etapa 3 (select), JOIN marca o cliente como "subscrito"
 *   → Servidor manda BROADCASTSs apenas a clientes de mesmo canal
 *   → Se cliente muda de canal, muda apenas esta string
 *
 * RETORNO: Nenhum (void) — resposta copiada para buffer
 * ============================================================================
 */
void handle_join(int client_idx, const char* canal_nome, char* response) {
    if (client_idx < 0 || client_idx >= MAX_CLIENTES) {
        strcpy(response, "ERRO: Índice de cliente inválido.");
        return;
    }
    if (!clientes[client_idx].autenticado) {
        strcpy(response, "ERRO: Deve estar autenticado para entrar num canal.");
        return;
    }

    if (strlen(canal_nome) == 0) {
        strcpy(response, "ERRO: Nome do canal não pode estar vazio.");
        return;
    }

    /* Garante que canal começa com # */
    char canal[50];
    if (canal_nome[0] == '#')
        strcpy(canal, canal_nome);
    else
        sprintf(canal, "#%s", canal_nome);

    strcpy(clientes[client_idx].canal, canal);
    sprintf(response, "JOIN_OK: Entrou no canal %s", canal);
}

/* ============================================================================
 * FUNÇÃO: handle_leave(int client_idx, char* response)
 * Processa comando LEAVE: utilizador sai do canal atual.
 *
 * PARÂMETROS:
 *   client_idx — Índice no array clientes[] (0-49)
 *   response   — Buffer para resposta ao cliente
 *
 * FLUXO:
 *   1. Validações:
 *      • client_idx deve estar entre 0 e MAX_CLIENTES-1
 *      • Cliente deve estar autenticado
 *   2. Limpar canal:
 *      • strcpy(clientes[client_idx].canal, "")
 *      • String vazia significa "não numa sala"
 *      • Cliente não receberá mais BROADCASTSs
 *   3. Responder:
 *      • "LEAVE_OK: Saiu do canal"
 *
 * EFEITO:
 *   Cliente pode agora:
 *   • JOIN a outro canal
 *   • Enviar mensagens privadas
 *   • Só não pode BROADCAST sem estar num canal
 *
 * RETORNO: Nenhum (void) — resposta copiada para buffer
 * ============================================================================
 */
void handle_leave(int client_idx, char* response) {
    if (client_idx < 0 || client_idx >= MAX_CLIENTES) {
        strcpy(response, "ERRO: Índice de cliente inválido.");
        return;
    }
    if (!clientes[client_idx].autenticado) {
        strcpy(response, "ERRO: Deve estar autenticado.");
        return;
    }

    strcpy(clientes[client_idx].canal, "");
    strcpy(response, "LEAVE_OK: Saiu do canal");
}

/* ============================================================================
 * FUNÇÃO: handle_broadcast(int client_idx, const char* msg, char* response)
 * Processa comando BROADCAST: envia mensagem a todos no mesmo canal.
 *
 * PARÂMETROS:
 *   client_idx — Índice do cliente remetente (0-49)
 *   msg        — Texto da mensagem
 *   response   — Buffer para resposta ao cliente remetente
 *
 * FLUXO:
 *   1. Validações:
 *      • client_idx deve estar entre 0 e MAX_CLIENTES-1
 *      • Cliente deve estar autenticado
 *      • Cliente deve estar num canal (strlen(canal) > 0)
 *      • Se falha: responde "BCAST_FAIL: ..."
 *   2. Construir mensagem formatada:
 *      • sprintf(bcast_msg, "[#canal] username: mensagem")
 *      • Ex: "[#geral] admin: Olá pessoal!"
 *   3. ENVIO PARA TODOS:
 *      Loop por todos os clientes:
 *      • if (clientes[i].fd > 0) → slot em uso
 *      • if (i != client_idx) → não re-enviar ao remetente
 *      • if (clientes[i].autenticado) → só quem tá autenticado
 *      • if (strcmp(canais == SAME) → mesmo canal
 *      • Então: send(clientes[i].fd, bcast_msg, ...)
 *   4. Responder ao remetente:
 *      • "BCAST_SENT: Mensagem enviada"
 *
 * NOTA ETAPA 3 CRÍTICA:
 *   Este handler implementa o BROADCAST multiutilizador:
 *   • Socket de remetente fica ABERTO (não fecha após comando)
 *   • Servidor pode enviar BROADCASTSs para MÚLTIPLOS clientes
 *   • Cada cliente recebe instantaneamente (ou próxima select())
 *   • Permite chat em tempo real
 *
 * OTIMIZAÇÃO FUTURA:
 *   Poderia usar select() aqui para non-blocking sends
 *   (evitar travamento se cliente desconectar durante send)
 *
 * RETORNO: Nenhum (void) — resposta copiada para buffer
 * ============================================================================
 */
void handle_broadcast(int client_idx, const char* msg, char* response) {
    if (client_idx < 0 || client_idx >= MAX_CLIENTES) {
        strcpy(response, "ERRO: Índice inválido.");
        return;
    }
    if (!clientes[client_idx].autenticado ||
        strlen(clientes[client_idx].canal) == 0) {
        strcpy(response, "BCAST_FAIL: Nao autenticado ou sem canal.");
        return;
    }

    /* Construir mensagem para broadcast com data/hora */
    char bcast_msg[BUF_SIZE];
    char data_hora[20];
    time_t agora = time(NULL);
    struct tm *t = localtime(&agora);
    strftime(data_hora, sizeof(data_hora), "%H:%M:%S", t);
    
    snprintf(bcast_msg, sizeof(bcast_msg), "[%s][%s] %s: %s",
             data_hora, clientes[client_idx].canal,
             clientes[client_idx].username, msg);

    /* Enviar para todos os clientes no mesmo canal */
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (clientes[i].fd > 0 && i != client_idx && clientes[i].autenticado &&
            strcmp(clientes[i].canal, clientes[client_idx].canal) == 0) {
            send(clientes[i].fd, bcast_msg, strlen(bcast_msg), 0);
        }
    }

    strcpy(response, "BCAST_SENT");
}

/* ============================================================================
 * FUNÇÃO: handle_list_channels(char* response)
 * Lista todos os canais activos com utilizadores presentes em cada um.
 *
 * FUNCIONALIDADE:
 *   Gera relatório formatado de:
 *   • Quantos canais estão ativos
 *   • Quantos utilizadores em cada canal
 *   • Nomes dos utilizadores em cada canal
 *
 * ALGORITMO:
 *   1. Inicializar arrays temporários:
 *      • canais_unicos[MAX_CLIENTES][50] — armazena nomes dos canais
 *      • usuarios_por_canal[MAX_CLIENTES][3000] — lista users por canal
 *      • num_canais = 0 — contador
 *
 *   2. ITERAR CLIENTES:
 *      For i=0 to MAX_CLIENTES-1:
 *      • if (clientes[i].fd > 0) → slot em uso
 *      • if (clientes[i].autenticado) → user autenticado
 *      • if (strlen(canal) > 0) → user numa sala (não vazio)
 *
 *   3. DETETAR CANAL NOVO:
 *      For j=0 to num_canais-1:
 *      • if (strcmp(canal == canais_unicos[j])) → já temos este canal
 *      • Se encontrado=1 → já conhecemos, apenas adicionar user
 *      • Se encontrado=0 (fim do loop) → novo canal, criar entrada
 *
 *   4. ACUMULAR UTILIZADORES:
 *      • sprintf(usuarios_por_canal[idx], "user1, user2, user3, ...")
 *      • Se não é primeiro user: strcat(", ")
 *      • Limite: 3000 bytes por canal (suporta ~100 users)
 *
 *   5. FORMATAR RESPOSTA:
 *      strcpy(response, "CHANNELS: Utilizadores por canal:\n")
 *      For cada canal:
 *      • snprintf("  #canal (3): user1, user2, user3\n", ...)
 *      strcat(response, "Fim da lista de canais.")
 *
 * RESPOSTA EXEMPLO:
 *   CHANNELS: Utilizadores por canal:
 *     #geral (3): admin, user1, user2
 *     #linux (2): admin, user1
 *     #ajuda (1): admin
 *   Fim da lista de canais.
 *
 * BUFFER MANAGEMENT:
 *   • usuarios_por_canal: 3000 bytes (≈100 users * 30 bytes each)
 *   • snprintf() com limite para evitar overflow
 *   • Format: "  #canal (COUNT): user1, user2, ...\n" = 70 bytes aprox
 *
 * COMPLEXIDADE:
 *   O(N²) onde N = MAX_CLIENTES (50)
 *   → 2500 comparações no pior caso (aceitável para 50 clientes)
 *
 * RETORNO: Nenhum (void) — resposta copiada para buffer
 * ============================================================================
 */
void handle_list_channels(char* response) {
    /* Array temporário para rastrear canais únicos */
    char canais_unicos[MAX_CLIENTES][50];
    char usuarios_por_canal[MAX_CLIENTES][3000] = {""};
    int num_canais = 0;

    /* Iterar clientes activos para recolher canais e seus utilizadores */
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (clientes[i].fd > 0 && clientes[i].autenticado &&
            strlen(clientes[i].canal) > 0) {
            /* Procurar se canal já foi visto */
            int encontrado = 0;
            for (int j = 0; j < num_canais; j++) {
                if (strcmp(canais_unicos[j], clientes[i].canal) == 0) {
                    /* Adicionar utilizador à lista deste canal */
                    if (strlen(usuarios_por_canal[j]) > 0) {
                        strcat(usuarios_por_canal[j], ", ");
                    }
                    strcat(usuarios_por_canal[j], clientes[i].username);
                    encontrado = 1;
                    break;
                }
            }

            /* Se novo canal, adicionar à lista */
            if (!encontrado && num_canais < MAX_CLIENTES) {
                strcpy(canais_unicos[num_canais], clientes[i].canal);
                strcpy(usuarios_por_canal[num_canais], clientes[i].username);
                num_canais++;
            }
        }
    }

    /* Formatar resposta */
    if (num_canais == 0) {
        strcpy(response, "CHANNELS: Nenhum canal activo");
    } else {
        strcpy(response, "CHANNELS: Utilizadores por canal:\n");
        for (int i = 0; i < num_canais; i++) {
            char line[800];
            int count = 0;

            /* Contar utilizadores separando por vírgula */
            for (int k = 0; usuarios_por_canal[i][k] != '\0'; k++) {
                if (usuarios_por_canal[i][k] == ',') count++;
            }
            count++; /* Adicionar 1 para o primeiro utilizador */

            snprintf(line, sizeof(line), "  %s (%d): %.500s\n",
                     canais_unicos[i], count, usuarios_por_canal[i]);
            strncat(response, line, BUF_SIZE - strlen(response) - 1);
        }
        strcat(response, "Fim da lista de canais.");
    }
}

/* ============================================================================
 * FUNÇÃO PRINCIPAL: main() - REFATORADO COM SELECT()
 * ============================================================================
 * OBJETIVO: Executar servidor TCP multiplex com select() para Etapa 3
 *
 * ARQUITETURA SELECT() - "O Coração do Servidor":
 *
 *   TRADICIONAL (Etapa 2 - Sequencial):
 *   while(1) {
 *       accept()      ← Bloqueia esperar cliente
 *       read()        ← Bloqueia ler comando
 *       close()       ← Desconecta
 *   }
 *   Problema: Só um cliente por vez!
 *
 *   NOVO (Etapa 3 - Multiplex com select()):
 *   while(1) {
 *       select()      ← Espera atividade em QUALQUER socket
 *       if new client → accept() e adiciona a array
 *       for each client:
 *           if data ready → read() e processa comando
 *           if disconnect → remove de array
 *   }
 *   Vantagem: 50 clientes simultâneos!
 *
 * FLUXO:
 *   1. Inicializar array de 50 slots (clientes[MAX_CLIENTES])
 *   2. Criar socket server (porta 10000)
 *   3. bind() + listen()
 *   4. Loop infinito com select():
 *      a) FD_ZERO() - limpar fd_set
 *      b) FD_SET(server_fd) - adicionar socket servidor
 *      c) FD_SET(cada cliente.fd) - adicionar clientes ativos
 *      d) select(max_fd+1) - esperar até 1 segundo por atividade
 *      e) Se server_fd ativo → novo cliente (accept)
 *      f) Se cliente ativo → processar comando
 *      g) Se recv()<=0 → desconectar cliente
 */
/* ============================================================================
 * FUNÇÃO: main()
 * Ponto de entrada do servidor TCP Etapa 3.
 *
 * RESPONSABILIDADES:
 *   1. Inicializar array de clientes (50 slots)
 *   2. Criar socket TCP listening na porta 10000
 *   3. Loop infinito com select() multiplexing
 *   4. Aceitar novas conexões (accept)
 *   5. Processar comandos de clientes autenticados
 *   6. Gerenciar desconexões
 *   7. Manter ligações persistentes durante sessões
 *
 * ETAPA 3 — MUDANÇA FUNDAMENTAL:
 *   Etapa 2: Sequencial — accept() → read() → close()
 *   Etapa 3: Select() — accept() → mantém socket aberto → múltiplos reads
 *
 *   Isto permite:
 *   • Múltiplos clientes simultaneamente
 *   • Chat em tempo real (BROADCASTSs)
 *   • Ligação persistente durante toda sessão
 *   • Sem bloquear em recv() de um cliente (bloqueia todos)
 *
 * ESTRUTURA DE CLIENTES:
 *   Cliente clientes[50]
 *   Cada slot:
 *   • fd = -1 (vazio) ou socket_fd (em uso)
 *   • autenticado = 0 (não) ou 1 (sim)
 *   • username = "admin", "user1", etc
 *   • canal = "#geral", "#linux", "" (vazio = sem canal)
 *
 * HANDLERS DE COMANDO SUPORTADOS:
 *   • AUTH username password → autenticar
 *   • REGISTER username password → registar
 *   • JOIN #canal → entrar num canal
 *   • BROADCAST #canal msg → enviar para todos
 *   • LEAVE → sair do canal
 *   • LIST_ALL → tabela de users
 *   • LIST_CHANNELS → canais e occupancy
 *   • LIST_PENDING → contas aguardando aprovação
 *   • APPROVE username → admin action
 *   • GET_INFO → info servidor
 *   • LOGOUT → desconectar
 *   + outros suportados (17 total)
 *
 * FLUXO DE CICLO SELECT:
 *   1. FD_ZERO() — limpar conjunto
 *   2. FD_SET(server_fd) — adicionar listening socket
 *   3. Loop clientes: se fd>0 → FD_SET(cliente.fd)
 *   4. select(max_fd+1, &readfds, NULL, NULL, timeout)
 *      → Bloqueia até haver dados OU timeout
 *   5. if (FD_ISSET(server_fd)) → nova conexão
 *      → accept() → preencer slot no array
 *   6. if (FD_ISSET(cliente[i].fd)) → dados do cliente
 *      → recv() → processar comando
 *   7. if (recv()<=0) → desconectar
 *      → fechar socket, marcar fd=-1
 *
 * SIGNAL HANDLING:
 *   signal(SIGPIPE, SIG_IGN)
 *   • Se cliente desconectar durante send()
 *   • Servidor recebe SIGPIPE
 *   • Ignorar para não crashar
 *
 * OTIMIZAÇÕES:
 *   • SO_REUSEADDR: permite restart rápido da porta
 *   • Backlog=5: máximo 5 conexões pendentes
 *   • Timeout=1s: evitar busy-waiting
 *
 * RETORNO:
 *   Loop infinito (nunca retorna normalmente)
 *   Ctrl+C para terminar
 * ============================================================================
 */
int main() {
    int server_fd;
    struct sockaddr_in addr, cli_addr;
    socklen_t addr_size = sizeof(addr);

    start_time = time(NULL);

    /* Ignorar SIGPIPE para evitar que o servidor bloqueie/crashe se um cliente
     * desconectar abruptamente durante um send() */
    signal(SIGPIPE, SIG_IGN);

    /* ===== INICIALIZAR ARRAY DE CLIENTES ===== */
    /* Limpar todos os 50 slots antes de começar */
    for (int i = 0; i < MAX_CLIENTES; i++) {
        clientes[i].fd = -1;         /* -1 = slot vazio */
        clientes[i].autenticado = 0; /* Não autenticado por padrão */
        memset(clientes[i].username, 0, sizeof(clientes[i].username));
        memset(clientes[i].canal, 0, sizeof(clientes[i].canal));
    }

    /* ===== CRIAR SOCKET SERVER (TCP LISTENING) ===== */
    /* AF_INET = IPv4, SOCK_STREAM = TCP */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(-1);
    }

    /* SO_REUSEADDR permite reiniciar servidor rapidamente */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* ===== BIND: Associar socket a porto 10000 ===== */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; /* Aceitar de qualquer IP */
    addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(-1);
    }

    /* ===== LISTEN: Ficar à escuta por conexões ===== */
    /* Backlog de 5 = máximo 5 conexões pendentes antes de reject */
    listen(server_fd, 5);

    desenhar_cabecalho_servidor();
    guardar_log("Servidor v3.0 (Etapa 3 - Select) iniciado e a escuta.", 1);

    /* ========== LOOP PRINCIPAL COM SELECT ==========
     * Este é o coração do servidor multiplex (concorrência).
     * Aqui é onde select() permite atender múltiplos clientes.
     */
    while (1) {
        fd_set readfds;         /* Conjunto de file descriptors a monitorizar */
        struct timeval tv;      /* Timeout */
        int max_fd = server_fd; /* Começar com socket do servidor */

        /* Inicializa a Bitmap List de descritores de ficheiro a zeros */
        FD_ZERO(&readfds);           
        
        /* Adiciona o Master Socket (para vigiar por novas conexões / SYNs) */
        FD_SET(server_fd, &readfds); 

        /* 
         * Iteramos sob o estado guardado para adicionar todos os clientes previamente
         * autenticados ou em sessão no array FD_SET, para monitorizar por eventos.
         */
        for (int i = 0; i < MAX_CLIENTES; i++) {
            if (clientes[i].fd > 0) {
                FD_SET(clientes[i].fd, &readfds);
                if (clientes[i].fd > max_fd) max_fd = clientes[i].fd;
            }
        }

        /* 
         * O SELECT() BLOQUEANTE MAS MULTIPLEXADO:
         * O servidor adormece nesta instrução a nível do Kernel. Só acorda se:
         * a) Passar 1 segundo rigorosamente (tv_sec = 1).
         * b) Um evento ocorrer nos descritores registados no array &readfds.
         * O primeiro argumento deve ser SEMPRE o (Descritor Mais Alto Registado + 1).
         */
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0) {
            perror("select");
            continue;
        }

        /* 
         * VERIFICAÇÃO MASTER SOCKET: 
         * Se o FD_ISSET retornar verdadeiro para o `server_fd`, isso significa matematicamente
         * que um cliente enviou um TCP SYN packet. Chamamos accept() não bloqueante neste cenário.
         */
        if (FD_ISSET(server_fd, &readfds)) {
            int client_fd =
                accept(server_fd, (struct sockaddr*)&cli_addr, &addr_size);
            if (client_fd > 0) {
                /* Procurar slot vazio no array */
                int added = 0;
                for (int i = 0; i < MAX_CLIENTES; i++) {
                    if (clientes[i].fd < 0 || clientes[i].fd == 0) {
                        /* Encontrado slot vazio → preencher */
                        clientes[i].fd = client_fd;
                        clientes[i].autenticado = 0;
                        memset(clientes[i].username, 0,
                               sizeof(clientes[i].username));
                        memset(clientes[i].canal, 0, sizeof(clientes[i].canal));
                        added = 1;
                        printf(
                            " \033[1;32m[OK]\033[0m    | Cliente conectado "
                            "(slot %d)\n",
                            i);
                        break;
                    }
                }
                if (!added) {
                    /* Array cheio (50 slots ocupados) */
                    printf(
                        " \033[1;31m[ERRO]\033[0m  | Servidor cheio, "
                        "rejeitando cliente\n");
                    close(client_fd);
                }
            }
        }

        /* 
         * VERIFICAÇÃO CLIENT SOCKETS:
         * Iteramos pelas nossas ligações persistentes. Se FD_ISSET der MATCH num dos sockets
         * dos clientes, significa que dados PSH, PSH+ACK, ou FIN chegaram do lado deles.
         */
        for (int i = 0; i < MAX_CLIENTES; i++) {
            /* Se slot tem cliente ativo (fd > 0) E tem dados prontos (FD_ISSET)
             */
            if (clientes[i].fd > 0 && FD_ISSET(clientes[i].fd, &readfds)) {
                char buffer[BUF_SIZE] = "";

                /* Como sabemos via select() que o Kernel tem os dados no buffer, recv() aqui
                 * não bloqueia a execução - extrai imediato para a memória de aplicação. */
                int n = recv(clientes[i].fd, buffer, BUF_SIZE - 1, 0);

                if (n <= 0) {
                    /* Se n <= 0, significa que o peer remoto fechou a sessão de forma 
                     * natural (0 bytes - FIN packet) ou com erro (-1 RST packet).
                     * Lógica da Persistência de Estado: Liberta o array e limpa campos! 
                     */
                    printf(
                        " \033[1;36m[INFO]\033[0m  | Cliente desconectado "
                        "(slot %d: %s)\n",
                        i, clientes[i].username);
                    close(clientes[i].fd);
                    clientes[i].fd = -1;
                    clientes[i].autenticado = 0;
                    memset(clientes[i].username, 0,
                           sizeof(clientes[i].username));
                    memset(clientes[i].canal, 0, sizeof(clientes[i].canal));
                    continue;
                }

                /* REMOVER NEWLINES */
                size_t len = strlen(buffer);
                while (len > 0 &&
                       (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
                    buffer[--len] = '\0';

                char response[BUF_SIZE] = "";
                char log_msg[BUF_SIZE] = "";
                int log_type = 0;

                total_pedidos++;

                /* ========== PROCESSAMENTO DE COMANDOS ========== */

                if (strncmp(buffer, "AUTH ", 5) == 0) {
                    char u[50], p[50], r[20] = "";
                    sscanf(buffer + 5, "%49s %49s", u, p);
                    int result = check_auth(u, p, r);

                    if (result == 1) {
                        sprintf(response, "AUTH_SUCCESS:%s", r);
                        clientes[i].autenticado = 1;
                        strcpy(clientes[i].username, u);
                        strcpy(clientes[i].canal, "#geral"); /* Default canal */
                        sprintf(log_msg, "Login OK: '%s' (%s)", u, r);
                        log_type = 1;
                    } else if (result == -1) {
                        strcpy(response, "AUTH_PENDING");
                        sprintf(log_msg, "Login PENDING: '%s'", u);
                        log_type = 3;
                    } else if (result == -2) {
                        strcpy(response, "AUTH_INACTIVE");
                        sprintf(log_msg, "Login INACTIVE: '%s'", u);
                        log_type = 3;
                    } else {
                        strcpy(response, "AUTH_FAIL");
                        sprintf(log_msg, "Login FALHOU: '%s'", u);
                        log_type = 3;
                    }
                }

                else if (strcmp(buffer, "GET_INFO") == 0) {
                    int up = (int)difftime(time(NULL), start_time);
                    sprintf(response,
                            "C-Cord Server v%s | Uptime: %02dh:%02dm:%02ds | "
                            "Pedidos: %d",
                            VERSAO_SERVIDOR, up / 3600, (up % 3600) / 60,
                            up % 60, total_pedidos);
                    sprintf(log_msg, "GET_INFO");
                    log_type = 0;
                }

                else if (strncmp(buffer, "ECHO ", 5) == 0) {
                    sprintf(response, "Servidor Ecoa: %s", buffer + 5);
                    sprintf(log_msg, "ECHO: '%s'", buffer + 5);
                    log_type = 0;
                }

                else if (strcmp(buffer, "LIST_ALL") == 0) {
                    if (!clientes[i].autenticado) {
                        strcpy(response, "ERRO: Nao autenticado.");
                        log_type = 3;
                    } else {
                        list_all(response);
                        sprintf(log_msg, "LIST_ALL");
                        log_type = 0;
                    }
                }

                else if (strcmp(buffer, "LIST_PENDING") == 0) {
                    if (!clientes[i].autenticado) {
                        strcpy(response, "ERRO: Nao autenticado.");
                        log_type = 3;
                    } else {
                        list_pending(response);
                        sprintf(log_msg, "LIST_PENDING");
                        log_type = 0;
                    }
                }

                else if (strncmp(buffer, "CHECK_INBOX ", 12) == 0) {
                    char user[50];
                    sscanf(buffer + 12, "%49s", user);
                    if (!clientes[i].autenticado ||
                        strcmp(clientes[i].username, user) != 0) {
                        strcpy(response,
                               "ERRO: Acesso negado a esta caixa de entrada.");
                    } else {
                        check_inbox(user, response);
                        sprintf(log_msg, "CHECK_INBOX: '%s'", user);
                        log_type = 0;
                    }
                }

                else if (strncmp(buffer, "SEND_MSG ", 9) == 0) {
                    char dest[50], from[50], msg[400];
                    sscanf(buffer + 9, "%49s %49s %399[^\n]", dest, from, msg);
                    if (!clientes[i].autenticado ||
                        strcmp(clientes[i].username, from) != 0) {
                        strcpy(response,
                               "ERRO: Remetente forjado ou sessao invalida.");
                    } else {
                        send_msg(dest, from, msg, response);
                        sprintf(log_msg, "SEND_MSG: de '%s' para '%s'", from,
                                dest);
                        log_type = 1;
                    }
                }

                else if (strncmp(buffer, "REGISTER ", 9) == 0) {
                    char u[50], p[50];
                    sscanf(buffer + 9, "%49s %49s", u, p);
                    register_user(u, p, response);
                    sprintf(log_msg, "REGISTER: '%s'", u);
                    log_type = 1;
                }

                else if (strncmp(buffer, "APPROVE ", 8) == 0) {
                    char target[50];
                    sscanf(buffer + 8, "%49s", target);
                    if (!clientes[i].autenticado) {
                        strcpy(response, "APPROVE_FAIL: Sessao invalida.");
                    } else {
                        approve_user(clientes[i].username, target, response);
                        sprintf(log_msg, "APPROVE: '%s' por '%s'", target,
                                clientes[i].username);
                        log_type = 1;
                    }
                }

                else if (strncmp(buffer, "BAN ", 4) == 0) {
                    char target[50];
                    sscanf(buffer + 4, "%49s", target);
                    if (!clientes[i].autenticado) {
                        strcpy(response, "BAN_FAIL: Sessao invalida.");
                    } else {
                        suspend_user(clientes[i].username, target, response);
                        sprintf(log_msg, "BAN: '%s' por '%s'", target,
                                clientes[i].username);
                        log_type = 1;
                    }
                }

                else if (strncmp(buffer, "REJECT ", 7) == 0) {
                    char target[50];
                    sscanf(buffer + 7, "%49s", target);
                    if (!clientes[i].autenticado) {
                        strcpy(response, "REJECT_FAIL: Sessao invalida.");
                    } else {
                        delete_user(clientes[i].username, target, response);
                        sprintf(log_msg, "REJECT: '%s' por '%s'", target,
                                clientes[i].username);
                        log_type = 1;
                    }
                }

                else if (strncmp(buffer, "DELETE_USER_ADMIN ", 18) == 0) {
                    char target[50];
                    sscanf(buffer + 18, "%49s", target);
                    if (!clientes[i].autenticado) {
                        strcpy(response, "DELETE_FAIL: Sessao invalida.");
                    } else {
                        delete_user(clientes[i].username, target, response);
                        sprintf(log_msg, "DELETE_USER_ADMIN: '%s' por '%s'",
                                target, clientes[i].username);
                        log_type = 1;
                    }
                }

                else if (strncmp(buffer, "VIEW_LOGS ", 10) == 0) {
                    char admin[50];
                    sscanf(buffer + 10, "%49s", admin);
                    view_logs(admin, response);
                    sprintf(log_msg, "VIEW_LOGS: por '%s'", admin);
                    log_type = 0;
                }

                /* ETAPA 3: NOVOS COMANDOS */
                else if (strncmp(buffer, "JOIN ", 5) == 0) {
                    char canal[50];
                    sscanf(buffer + 5, "%49s", canal);
                    handle_join(i, canal, response);
                    sprintf(log_msg, "JOIN: '%s' entrou em %s",
                            clientes[i].username, canal);
                    log_type = 1;
                }

                else if (strcmp(buffer, "LEAVE") == 0) {
                    char old_canal[50];
                    strcpy(old_canal, clientes[i].canal);
                    handle_leave(i, response);
                    sprintf(log_msg, "LEAVE: '%s' saiu de %s",
                            clientes[i].username, old_canal);
                    log_type = 1;
                }

                else if (strncmp(buffer, "BROADCAST ", 10) == 0) {
                    char msg[BUF_SIZE];
                    strcpy(msg, buffer + 10);
                    handle_broadcast(i, msg, response);
                    sprintf(log_msg, "BROADCAST: '%s' em %s",
                            clientes[i].username, clientes[i].canal);
                    log_type = 1;
                }

                else if (strcmp(buffer, "LIST_CHANNELS") == 0) {
                    handle_list_channels(response);
                    sprintf(log_msg, "LIST_CHANNELS: '%s'",
                            clientes[i].username);
                    log_type = 2;
                }

                else if (strcmp(buffer, "LOGOUT") == 0) {
                    strcpy(response, "LOGOUT_OK");
                    sprintf(log_msg, "LOGOUT: '%s'", clientes[i].username);
                    log_type = 1;
                    clientes[i].autenticado = 0;
                    memset(clientes[i].username, 0,
                           sizeof(clientes[i].username));
                    memset(clientes[i].canal, 0, sizeof(clientes[i].canal));
                }

                else {
                    strcpy(response, "CMD_INVALID");
                    snprintf(log_msg, sizeof(log_msg) - 1,
                             "CMD desconhecido: '%.30s'", buffer);
                    log_type = 3;
                }

                guardar_log(log_msg, log_type);

                /* ENVIAR RESPOSTA (SEM FECHAR!) */
                send(clientes[i].fd, response, strlen(response), 0);
                /* CRÍTICO: NÃO FAZER close(clientes[i].fd) AQUI! */
            }
        }
    }

    close(server_fd);
    return 0;
}
