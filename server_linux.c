/*
 * ============================================================================
 * SERVIDOR TCP C-CORD — VERSÃO 3.0 (Etapa 3: Select + Canais + Broadcast)
 * ============================================================================
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
 * Rastreia estado de cada cliente conectado (ligações persistentes)
 */
typedef struct {
    int fd;            /* Socket file descriptor (-1 = slot vazio) */
    char username[50]; /* Utilizador autenticado (vazio se não autenticado) */
    char canal[50];    /* Canal onde cliente está (#geral, #admin, etc) */
    int autenticado;   /* Flag: 0=não autenticado, 1=autenticado */
} Cliente;

Cliente clientes[MAX_CLIENTES]; /* Array global de clientes */

const char* VERSAO_SERVIDOR = "3.0-Etapa3";
time_t start_time;
int total_pedidos = 0;

/* ============================================================================
 * FUNÇÃO: guardar_log()
 * ============================================================================
 */
void guardar_log(const char* mensagem, int tipo) {
    FILE* f = fopen(LOG_FILE, "a");
    if (f) {
        char data[64];
        time_t agora = time(NULL);
        struct tm* t = localtime(&agora);
        strftime(data, sizeof(data), "%Y-%m-%d %H:%M:%S", t);
        fprintf(f, "[%s] %s\n", data, mensagem);
        fclose(f);
    }
    if (tipo == 1)
        printf(" \033[1;32m[OK]\033[0m    | %s\n", mensagem);
    else if (tipo == 3)
        printf(" \033[1;31m[ERRO]\033[0m  | %s\n", mensagem);
    else
        printf(" \033[1;36m[INFO]\033[0m  | %s\n", mensagem);
}

int proximo_id() {
    FILE* f = fopen(USERS_FILE, "r");
    if (!f) return 1;
    char line[256];
    int max_id = 0, id = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%d:", &id) == 1 && id > max_id) max_id = id;
    }
    fclose(f);
    return max_id + 1;
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

int check_auth(const char* username, const char* password, char* role) {
    FILE* f = fopen(USERS_FILE, "r");
    if (!f) {
        guardar_log("users.txt nao encontrado!", 3);
        return 0;
    }

    char line[256], id[10], u[50], p[50], r[20], s[20];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%9[^:]:%49[^:]:%49[^:]:%19[^:]:%19s", id, u, p, r,
                   s) == 5) {
            if (strcmp(u, username) == 0 && strcmp(p, password) == 0) {
                fclose(f);
                if (strcmp(s, "PENDING") == 0) return -1;
                if (strcmp(s, "INACTIVE") == 0) return -2;
                strcpy(role, r);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
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
                sprintf(entry, " [%d] De: %s → %s\n", ++count, from, msg);
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
    fprintf(f, "%s:%s:%s\n", dest, from, msg);
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
    char line[256];
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

    /* Construir mensagem para broadcast */
    char bcast_msg[BUF_SIZE];
    sprintf(bcast_msg, "[%s] %s: %s", clientes[client_idx].canal,
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
 * FUNÇÃO PRINCIPAL: main() - REFATORADO COM SELECT()
 * ============================================================================
 */
int main() {
    int server_fd;
    struct sockaddr_in addr, cli_addr;
    socklen_t addr_size = sizeof(addr);

    start_time = time(NULL);

    /* INICIALIZAR ARRAY DE CLIENTES */
    for (int i = 0; i < MAX_CLIENTES; i++) {
        clientes[i].fd = -1;
        clientes[i].autenticado = 0;
        memset(clientes[i].username, 0, sizeof(clientes[i].username));
        memset(clientes[i].canal, 0, sizeof(clientes[i].canal));
    }

    /* CRIAR SOCKET SERVER */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(-1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* BIND */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(-1);
    }

    /* LISTEN */
    listen(server_fd, 5);

    desenhar_cabecalho_servidor();
    guardar_log("Servidor v3.0 (Etapa 3 - Select) iniciado e a escuta.", 1);

    /* ========== LOOP PRINCIPAL COM SELECT ========== */
    while (1) {
        fd_set readfds;
        struct timeval tv;
        int max_fd = server_fd;

        /* PREPARAR FD_SET */
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        for (int i = 0; i < MAX_CLIENTES; i++) {
            if (clientes[i].fd > 0) {
                FD_SET(clientes[i].fd, &readfds);
                if (clientes[i].fd > max_fd) max_fd = clientes[i].fd;
            }
        }

        /* SELECT COM TIMEOUT DE 1 SEGUNDO */
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0) {
            perror("select");
            continue;
        }

        /* NOVO CLIENTE? */
        if (FD_ISSET(server_fd, &readfds)) {
            int client_fd =
                accept(server_fd, (struct sockaddr*)&cli_addr, &addr_size);
            if (client_fd > 0) {
                /* Procurar slot vazio */
                int added = 0;
                for (int i = 0; i < MAX_CLIENTES; i++) {
                    if (clientes[i].fd < 0 || clientes[i].fd == 0) {
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
                    printf(
                        " \033[1;31m[ERRO]\033[0m  | Servidor cheio, "
                        "rejeitando cliente\n");
                    close(client_fd);
                }
            }
        }

        /* CLIENTES EXISTENTES ENVIARAM DADOS? */
        for (int i = 0; i < MAX_CLIENTES; i++) {
            if (clientes[i].fd > 0 && FD_ISSET(clientes[i].fd, &readfds)) {
                char buffer[BUF_SIZE] = "";
                int n = recv(clientes[i].fd, buffer, BUF_SIZE - 1, 0);

                if (n <= 0) {
                    /* DESCONEXÃO */
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
                    list_all(response);
                    sprintf(log_msg, "LIST_ALL");
                    log_type = 0;
                }

                else if (strcmp(buffer, "LIST_PENDING") == 0) {
                    list_pending(response);
                    sprintf(log_msg, "LIST_PENDING");
                    log_type = 0;
                }

                else if (strncmp(buffer, "CHECK_INBOX ", 12) == 0) {
                    char user[50];
                    sscanf(buffer + 12, "%49s", user);
                    check_inbox(user, response);
                    sprintf(log_msg, "CHECK_INBOX: '%s'", user);
                    log_type = 0;
                }

                else if (strncmp(buffer, "SEND_MSG ", 9) == 0) {
                    char dest[50], from[50], msg[400];
                    sscanf(buffer + 9, "%49s %49s %399[^\n]", dest, from, msg);
                    send_msg(dest, from, msg, response);
                    sprintf(log_msg, "SEND_MSG: de '%s' para '%s'", from, dest);
                    log_type = 1;
                }

                else if (strncmp(buffer, "REGISTER ", 9) == 0) {
                    char u[50], p[50];
                    sscanf(buffer + 9, "%49s %49s", u, p);
                    register_user(u, p, response);
                    sprintf(log_msg, "REGISTER: '%s'", u);
                    log_type = 1;
                }

                else if (strncmp(buffer, "APPROVE_USER ", 13) == 0) {
                    char admin[50], target[50];
                    sscanf(buffer + 13, "%49s %49s", admin, target);
                    approve_user(admin, target, response);
                    sprintf(log_msg, "APPROVE: '%s' por '%s'", target, admin);
                    log_type = 1;
                }

                else if (strncmp(buffer, "SUSPEND_USER ", 13) == 0) {
                    char admin[50], target[50];
                    sscanf(buffer + 13, "%49s %49s", admin, target);
                    suspend_user(admin, target, response);
                    sprintf(log_msg, "SUSPEND: '%s' por '%s'", target, admin);
                    log_type = 1;
                }

                else if (strncmp(buffer, "DELETE_USER ", 12) == 0) {
                    char admin[50], target[50];
                    sscanf(buffer + 12, "%49s %49s", admin, target);
                    delete_user(admin, target, response);
                    sprintf(log_msg, "DELETE: '%s' por '%s'", target, admin);
                    log_type = 1;
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
