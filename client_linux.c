/*
 * ============================================================================
 * CLIENTE TCP C-CORD — VERSÃO 3.0 (Etapa 3: Select + Dupla Escuta + Canais)
 * ============================================================================
 *
 * Descrição:
 *   Cliente reativo com select() para monitorizar simultaneamente:
 *   - Teclado (stdin): Entrada do utilizador
 *   - Socket TCP: Broadcasts do servidor
 *   
 *   Ligação persistente durante toda a sessão (não fecha após comando).
 *   Suporta JOIN, LEAVE, BROADCAST para comunicação em canais.
 *
 * Compilação: gcc -o client_linux client_etapa3.c
 * Execução  : ./client_linux <IP_SERVIDOR> <PORTO>
 * Exemplo   : ./client_linux 127.0.0.1 10000
 *
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define BUF_SIZE 4096
#define STDIN_FILENO 0

/* ============================================================================
 * VARIÁVEIS GLOBAIS DE ESTADO
 * ============================================================================
 */
char current_user[50] = "";
char current_email[100] = "";
int is_admin = 0;
int autenticado = 0;
char current_canal[50] = "";
time_t login_time = 0;
int server_fd = -1;

/* ============================================================================
 * FUNÇÃO: clear_buffer()
 * ============================================================================
 * Limpa buffer de stdin após scanf
 */
void clear_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* ============================================================================
 * FUNÇÃO: draw_header()
 * ============================================================================
 * Desenha interface TUI
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
        printf("         \033[1;37m[?] BEM-VINDO AO C-CORD (v3.0)\033[0m\n");
    
    if (subtitulo && strlen(subtitulo) > 0) {
        printf("====================================================\n");
        printf("    %s\n", subtitulo);
    }
    printf("====================================================\n");
    
    if (modo >= 1) {
        printf(" UTILIZADOR: [\033[1;33m%s\033[0m] | CANAL: %s\n",
               current_user, strlen(current_canal) > 0 ? current_canal : "(sem canal)");
        if (login_time > 0) {
            int elapsed = (int)difftime(time(NULL), login_time);
            printf(" Ligado desde: %02dh:%02dm:%02ds\n", elapsed / 3600,
                   (elapsed % 3600) / 60, elapsed % 60);
        }
    }
    printf("----------------------------------------------------\n");
}

/* ============================================================================
 * FUNÇÃO: aguardar_enter()
 * ============================================================================
 */
void aguardar_enter() {
    printf("\n >> Pressione ENTER para continuar...");
    getchar();
}

/* ============================================================================
 * FUNÇÃO: print_server_response()
 * ============================================================================
 */
void print_server_response(const char* res) {
    printf("\n\033[1;32m[SERVIDOR]\033[0m\n%s\n", res);
}

/* ============================================================================
 * FUNÇÃO: sugerir_usernames()
 * ============================================================================
 */
void sugerir_usernames(const char* base) {
    printf("\n Sugestões disponíveis:\n");
    printf(" > %s_2026\n", base);
    printf(" > %s_pt\n", base);
    printf(" > %s_ccord\n", base);
}

/* ============================================================================
 * FUNÇÃO: handle_server_message()
 * ============================================================================
 * Processa mensagem recebida do servidor (resposta ou broadcast)
 */
void handle_server_message(const char* msg) {
    if (strlen(msg) == 0) return;
    
    /* Broadcasts começam com [ (canal) */
    if (msg[0] == '[') {
        printf("\n\033[1;33m%s\033[0m\n", msg);
    }
    /* Respostas do servidor */
    else if (strncmp(msg, "AUTH_SUCCESS", 12) == 0) {
        printf("\n\033[1;32m[OK]\033[0m Autenticação bem-sucedida!\n");
    }
    else if (strncmp(msg, "JOIN_OK", 7) == 0) {
        printf("\n\033[1;32m[OK]\033[0m %s\n", msg);
    }
    else if (strncmp(msg, "BCAST_SENT", 10) == 0) {
        printf("\n\033[1;32m[OK]\033[0m Mensagem enviada!\n");
    }
    else if (strncmp(msg, "LEAVE_OK", 8) == 0) {
        printf("\n\033[1;32m[OK]\033[0m Saiu do canal\n");
    }
    else if (strstr(msg, "FAIL") || strstr(msg, "ERRO")) {
        printf("\n\033[1;31m[ERRO]\033[0m %s\n", msg);
    }
    else {
        printf("\n\033[1;36m[INFO]\033[0m %s\n", msg);
    }
}

/* ============================================================================
 * FUNÇÃO: fluxo_login()
 * ============================================================================
 * Autentica utilizador (antes de entrar no select loop)
 */
int fluxo_login(int server_fd) {
    while (1) {
        draw_header(0, "LOGIN / AUTENTICAÇÃO");
        char u[50], p[50], cmd[150], res[BUF_SIZE];
        
        printf(" Nome de Utilizador: ");
        scanf("%49s", u);
        printf(" Palavra-passe: ");
        scanf("%49s", p);
        clear_buffer();
        
        printf("\n [A VERIFICAR CREDENCIAIS...]\n");
        
        sprintf(cmd, "AUTH %s %s", u, p);
        send(server_fd, cmd, strlen(cmd), 0);
        
        memset(res, 0, BUF_SIZE);
        recv(server_fd, res, BUF_SIZE - 1, 0);
        
        if (strncmp(res, "AUTH_SUCCESS", 12) == 0) {
            strcpy(current_user, u);
            is_admin = (strstr(res, "ADMIN") != NULL);
            autenticado = 1;
            login_time = time(NULL);
            strcpy(current_canal, "#geral");
            printf(" \033[1;32m[OK]\033[0m Bem-vindo, %s!\n", u);
            
            /* AUTO-JOIN #geral */
            sprintf(cmd, "JOIN #geral");
            send(server_fd, cmd, strlen(cmd), 0);
            memset(res, 0, BUF_SIZE);
            recv(server_fd, res, BUF_SIZE - 1, 0);
            
            aguardar_enter();
            return 1;
        }
        else if (strcmp(res, "AUTH_PENDING") == 0) {
            printf(" \033[1;33m[!] Conta aguarda aprovação admin.\033[0m\n");
            aguardar_enter();
            return 0;
        }
        else if (strcmp(res, "AUTH_INACTIVE") == 0) {
            printf(" \033[1;31m[!] Conta foi suspensa.\033[0m\n");
            aguardar_enter();
            return 0;
        }
        else {
            printf(" \033[1;31m[!] FALHA NO LOGIN\033[0m\n");
            aguardar_enter();
            return 0;
        }
    }
}

/* ============================================================================
 * FUNÇÃO: fluxo_registo()
 * ============================================================================
 */
int fluxo_registo(int server_fd) {
    while (1) {
        draw_header(0, "CRIAR NOVA CONTA");
        char u[50], p[50], p2[50], email[100], cmd[200], res[BUF_SIZE];
        
        printf(" Nome de Utilizador: ");
        scanf("%49s", u);
        printf(" Palavra-passe: ");
        scanf("%49s", p);
        printf(" Confirmar Palavra-passe: ");
        scanf("%49s", p2);
        printf(" E-mail: ");
        scanf("%99s", email);
        clear_buffer();
        
        if (strcmp(p, p2) != 0) {
            printf(" \033[1;31m[ERRO]\033[0m Palavras-passe não coincidem.\n");
            aguardar_enter();
            continue;
        }
        
        sprintf(cmd, "REGISTER %s %s", u, p);
        send(server_fd, cmd, strlen(cmd), 0);
        
        memset(res, 0, BUF_SIZE);
        recv(server_fd, res, BUF_SIZE - 1, 0);
        
        if (strncmp(res, "REGISTER_OK", 11) == 0) {
            printf(" \033[1;32m[OK]\033[0m Registo bem-sucedido!\n");
            aguardar_enter();
            return 1;
        }
        else {
            printf(" \033[1;31m[ERRO]\033[0m %s\n", res);
            aguardar_enter();
            return 0;
        }
    }
}

/* ============================================================================
 * FUNÇÃO: menu_pre_login()
 * ============================================================================
 * Menu antes de autenticar
 */
void menu_pre_login(int server_fd) {
    while (!autenticado) {
        draw_header(0, "MENU PRINCIPAL");
        printf("\n [ F3 ] Login\n");
        printf(" [ F6 ] Registo\n");
        printf(" [ F0 ] Sair\n\n Escolha: ");
        
        int opt;
        if (scanf("%d", &opt) != 1) opt = 0;
        clear_buffer();
        
        if (opt == 3) {
            if (fluxo_login(server_fd)) break;
        }
        else if (opt == 6) {
            fluxo_registo(server_fd);
        }
        else if (opt == 0) {
            printf("\n \033[1;36m[INFO]\033[0m Até breve!\n");
            close(server_fd);
            exit(0);
        }
    }
}

/* ============================================================================
 * FUNÇÃO PRINCIPAL: main()
 * ============================================================================
 */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <IP_SERVIDOR> <PORTO>\n", argv[0]);
        exit(1);
    }
    
    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    
    /* CONECTAR AO SERVIDOR */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    
    if (inet_aton(server_ip, &addr.sin_addr) <= 0) {
        printf("Endereço IP inválido\n");
        close(server_fd);
        exit(1);
    }
    
    printf(" A verificar servidor no porto %d...\n", server_port);
    if (connect(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf(" \033[1;31m[ERRO]\033[0m Servidor inacessível.\n");
        close(server_fd);
        exit(1);
    }
    printf(" \033[1;32m[OK]\033[0m Servidor encontrado.\n");
    sleep(1);
    
    /* MENU DE LOGIN */
    menu_pre_login(server_fd);
    
    /* ========== LOOP PRINCIPAL COM SELECT ========== */
    draw_header(1, "CHAT — Etapa 3 com Canais");
    printf("\n [Escute broadcasts em tempo real]\n");
    printf(" Escreva comandos ou mensagens abaixo:\n\n");
    
    while (autenticado) {
        fd_set readfds;
        struct timeval tv;
        
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(server_fd, &readfds);
        
        int max_fd = (server_fd > STDIN_FILENO) ? server_fd : STDIN_FILENO;
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        
        if (activity < 0) {
            perror("select");
            break;
        }
        
        /* DADOS DO SERVIDOR (broadcast ou resposta) */
        if (FD_ISSET(server_fd, &readfds)) {
            char buffer[BUF_SIZE] = "";
            int n = recv(server_fd, buffer, BUF_SIZE - 1, 0);
            
            if (n <= 0) {
                printf("\n\033[1;31m[DESCONECTADO]\033[0m Servidor encerrou ligação.\n");
                autenticado = 0;
                break;
            }
            
            /* Remover \n */
            buffer[strcspn(buffer, "\n")] = 0;
            
            handle_server_message(buffer);
            
            /* Reexibir prompt se necessário */
            if (buffer[0] == '[') {
                printf(" > ");
                fflush(stdout);
            }
        }
        
        /* DADOS DO TECLADO (input do utilizador) */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char input[BUF_SIZE] = "";
            
            if (fgets(input, sizeof(input), stdin) == NULL) {
                continue;
            }
            
            input[strcspn(input, "\n")] = 0;
            
            if (strlen(input) == 0) continue;
            
            /* PROCESSAR COMANDOS */
            
            /* Logout */
            if (strcmp(input, "0") == 0 || strcmp(input, "EXIT") == 0) {
                printf(" \033[1;36m[INFO]\033[0m Desconectando...\n");
                send(server_fd, "LEAVE", 5, 0);
                close(server_fd);
                autenticado = 0;
                break;
            }
            
            /* GET_INFO */
            else if (strcmp(input, "1") == 0 || strcmp(input, "GET_INFO") == 0) {
                send(server_fd, "GET_INFO", 8, 0);
            }
            
            /* ECHO */
            else if (strncmp(input, "2", 1) == 0 || strncmp(input, "ECHO ", 5) == 0) {
                char msg[300];
                if (input[0] == '2') {
                    printf(" Mensagem: ");
                    fgets(msg, sizeof(msg), stdin);
                    msg[strcspn(msg, "\n")] = 0;
                    snprintf(input, sizeof(input) - 1, "ECHO %s", msg);
                }
                send(server_fd, input, strlen(input), 0);
            }
            
            /* JOIN #canal */
            else if (strncmp(input, "9", 1) == 0 || strncmp(input, "JOIN ", 5) == 0) {
                char cmd[100];
                if (input[0] == '9') {
                    char canal[50];
                    printf(" Canal (#): ");
                    scanf("%49s", canal);
                    clear_buffer();
                    sprintf(cmd, "JOIN #%s", canal);
                } else {
                    strcpy(cmd, input);
                }
                send(server_fd, cmd, strlen(cmd), 0);
                strcpy(current_canal, strstr(cmd, "#") ? strstr(cmd, "#") : "#geral");
            }
            
            /* BROADCAST */
            else if (strncmp(input, "10", 2) == 0 || strncmp(input, "BROADCAST ", 10) == 0) {
                char cmd[BUF_SIZE];
                if (strncmp(input, "10", 2) == 0) {
                    char msg[300];
                    printf(" Mensagem: ");
                    fgets(msg, sizeof(msg), stdin);
                    msg[strcspn(msg, "\n")] = 0;
                    sprintf(cmd, "BROADCAST %s", msg);
                } else {
                    strcpy(cmd, input);
                }
                send(server_fd, cmd, strlen(cmd), 0);
            }
            
            /* LEAVE */
            else if (strcmp(input, "LEAVE") == 0) {
                send(server_fd, "LEAVE", 5, 0);
                strcpy(current_canal, "");
            }
            
            /* Outros comandos (enviar direto para servidor) */
            else if (strlen(input) > 0) {
                send(server_fd, input, strlen(input), 0);
            }
        }
    }
    
    close(server_fd);
    printf(" \033[1;36m[INFO]\033[0m Até breve!\n");
    return 0;
}
