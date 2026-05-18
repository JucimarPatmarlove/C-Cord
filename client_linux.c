/*
 * ============================================================================
 * CLIENTE TCP C-CORD — VERSÃO 2.0 (Etapa 2: F3, F4, F5, F6, F7, F8)
 * ============================================================================
 *
 * Descrição:
 *   Cliente de linha de comando para conectar a um servidor C-CORD TCP.
 *   Oferece interface TUI (Terminal User Interface) com 3 modos diferentes:
 *   - VISITANTE (branco): sem autenticação
 *   - ADMIN (vermelho): utilizador com privilégios administrativos
 *   - UTILIZADOR NORMAL (ciano): utilizador comum
 *
 * Cada modo tem um menu diferente com operações apropriadas.
 * Cada comando cria uma nova ligação TCP, envia o comando, recebe resposta,
 * e fecha a ligação (modelo bloqueante).
 *
 * Compilação: gcc -o tcp_client_linux tcp_client_linux.c
 * Execução  : ./tcp_client_linux <IP_SERVIDOR> <PORTO>
 * Exemplo   : ./tcp_client_linux 127.0.0.1 10000
 *
 * ============================================================================
 */

/* ============================================================================
 * CABEÇALHOS E BIBLIOTECAS POSIX
 * ============================================================================
 */

#include <arpa/inet.h>  /* Para inet_aton, htons — conversões de rede */
#include <netdb.h>      /* Para gethostbyname — resolução de nomes DNS */
#include <netinet/in.h> /* Para struct sockaddr_in */
#include <stdio.h>      /* Para printf, scanf, fgets */
#include <stdlib.h>     /* Para malloc, atoi, exit */
#include <string.h>     /* Para strcmp, strcpy, strlen, memset */
#include <sys/socket.h> /* Para socket, connect, send, recv, close */
#include <unistd.h>     /* Para sleep, close */

/* ============================================================================
 * CONSTANTES
 * ============================================================================
 */

#define BUF_SIZE 2048 /* Tamanho máximo do buffer de comunicação */

/* ============================================================================
 * VARIÁVEIS GLOBAIS (Estado da Sessão)
 * ============================================================================
 *
 * Estas variáveis mantêm o estado do utilizador durante toda a execução
 * do cliente, permitindo que o menu se adapte ao estado de autenticação.
 */

int is_admin = 0;           /* Flag: 1=admin, 0=utilizador normal */
char current_user[50] = ""; /* String: nome do utilizador autenticado */
struct sockaddr_in addr;    /* Estrutura: endereço do servidor */

/* ============================================================================
 * FUNÇÃO: clear_buffer()
 * ============================================================================
 *
 * O que esta função faz:
 *   Limpa o buffer de entrada do teclado (stdin) para evitar problemas
 *   com caracteres residuais deixados pelo scanf().
 *
 * Por que é importante:
 *   Quando o utilizador digita algo e pressiona ENTER, o scanf() consome
 *   os números/caracteres mas deixa o '\n' (newline/Enter) no buffer.
 *   Se não limpássemos, o próximo scanf() iria ler esse '\n' em vez da
 *   entrada do utilizador, causando comportamentos inesperados.
 *
 * Como funciona passo a passo:
 *   1. Declarar variável c para guardar cada carácter
 *   2. Loop: enquanto houver caracteres no buffer (getchar())
 *   3. Continuar até encontrar '\n' (Enter) ou EOF (fim do ficheiro)
 *   4. Ao sair do loop, o buffer está limpo e pronto para novo scanf
 *
 * Exemplo de uso:
 *   scanf("%d", &numero);  // Utilizador digita "5" e pressiona Enter
 *   clear_buffer();        // Remove o '\n' deixado no buffer
 *   scanf("%s", nome);     // Agora lê correctamente a próxima entrada
 *
 * ============================================================================
 */
void clear_buffer() {
    int c; /* Variável para guardar cada carácter lido */

    /* Loop: ler caracteres até encontrar newline ou EOF */
    while ((c = getchar()) != '\n' && c != EOF);
}

/* ============================================================================
 * FUNÇÃO: draw_header()
 * ============================================================================
 *
 * O que esta função faz:
 *   Desenha o cabeçalho e menu da interface do cliente no terminal.
 *   A aparência MUDA conforme o estado de autenticação (3 modos diferentes).
 *
 * Os 3 modos de interface:
 *
 *   1. MODO VISITANTE (BRANCO - ANSI 1;37m):
 *      - Utilizador NÃO está autenticado (current_user está vazio)
 *      - Mostra apenas: "Login" e "Registar"
 *      - Para indicar que está "à espera de entrada"
 *
 *   2. MODO ADMINISTRADOR (VERMELHO - ANSI 1;31m):
 *      - Utilizador autenticado com privilégios admin (is_admin = 1)
 *      - Mostra menu completo: GET_INFO, ECHO, LIST_ALL, CHECK_INBOX,
 *        SEND_MSG, APPROVE_USER, DELETE_USER
 *      - Para indicar "poder administrativo"
 *
 *   3. MODO UTILIZADOR NORMAL (CIANO - ANSI 1;36m):
 *      - Utilizador autenticado sem privilégios admin (is_admin = 0)
 *      - Mostra submenu: GET_INFO, ECHO, LIST_ALL, CHECK_INBOX, SEND_MSG
 *      - Sem operações administrativas
 *
 * Como funciona passo a passo:
 *   1. Limpar terminal com system("clear")
 *   2. Verificar estado do utilizador (current_user[0])
 *   3. Escolher cor (branco/vermelho/ciano) conforme estado
 *   4. Definir cor com código ANSI (ex: \033[1;31m = vermelho)
 *   5. Desenhar logo ASCII do C-CORD (sempre igual)
 *   6. Desenhar título específico do modo
 *   7. Mostrar nome do utilizador (ou "[VISITANTE]" se não autenticado)
 *   8. Resetar cor com \033[0m
 *
 * Códigos ANSI usados:
 *   \033[1;37m = branco intenso
 *   \033[1;31m = vermelho intenso
 *   \033[1;36m = ciano intenso
 *   \033[1;33m = amarelo intenso (para nome do utilizador)
 *   \033[0m    = resetar para cor por defeito
 *
 * ============================================================================
 */
void draw_header() {
    system("clear"); /* Limpar terminal */

    /* ---- MODO VISITANTE (sem autenticação) ---- */
    if (current_user[0] == '\0') {
        printf("\033[1;37m"); /* Cor branca */
        printf("  _____        _____              _ \n");
        printf(" / ____|      / ____|            | |\n");
        printf("| |     _____| |     ___  _ __ __| |\n");
        printf("| |    |_____| |    / _ \\| '__/ _` |\n");
        printf("| |____      | |___| (_) | | | (_| |\n");
        printf(" \\_____|      \\_____\\___/|_|  \\__,_|\n");
        printf("\033[0m\n"); /* Resetar cor */

        printf("====================================================\n");
        printf("         \033[1;37m[?] C-CORD (MODO VISITANTE)\033[0m\n");
        printf("====================================================\n");
        printf(" UTILIZADOR: [VISITANTE]\n");
    }
    /* ---- MODO ADMINISTRADOR ---- */
    else if (is_admin) {
        printf("\033[1;31m"); /* Cor vermelha */
        printf("  _____        _____              _ \n");
        printf(" / ____|      / ____|            | |\n");
        printf("| |     _____| |     ___  _ __ __| |\n");
        printf("| |    |_____| |    / _ \\| '__/ _` |\n");
        printf("| |____      | |___| (_) | | | (_| |\n");
        printf(" \\_____|      \\_____\\___/|_|  \\__,_|\n");
        printf("\033[0m\n"); /* Resetar cor */

        printf("====================================================\n");
        printf("         \033[1;31m[!] MODO ADMINISTRADOR ACTIVO\033[0m\n");
        printf("====================================================\n");
        printf(" UTILIZADOR: [\033[1;33m%s\033[0m] | PAPEL: ADMINISTRADOR\n",
               current_user);
    }
    /* ---- MODO UTILIZADOR NORMAL ---- */
    else {
        printf("\033[1;36m"); /* Cor ciana */
        printf("  _____        _____              _ \n");
        printf(" / ____|      / ____|            | |\n");
        printf("| |     _____| |     ___  _ __ __| |\n");
        printf("| |    |_____| |    / _ \\| '__/ _` |\n");
        printf("| |____      | |___| (_) | | | (_| |\n");
        printf(" \\_____|      \\_____\\___/|_|  \\__,_|\n");
        printf("\033[0m\n"); /* Resetar cor */

        printf("====================================================\n");
        printf("         \033[1;36m[~] MODO UTILIZADOR NORMAL\033[0m\n");
        printf("====================================================\n");
        printf(" UTILIZADOR: [\033[1;33m%s\033[0m] | PAPEL: UTILIZADOR\n",
               current_user);
    }

    printf("----------------------------------------------------\n");
}

/* ============================================================================
 * FUNÇÃO: call_server()
 * ============================================================================
 *
 * O que esta função faz:
 *   Envia um comando ao servidor e recebe a resposta.
 *   Cria uma NOVA ligação TCP por cada chamada (modelo bloqueante/simples).
 *
 * Para que é importante:
 *   Implementa a comunicação cliente-servidor.
 *   Cada função de menu chama isto para enviar o seu comando.
 *
 * Parâmetros:
 *   - cmd: comando a enviar ao servidor (ex: "AUTH admin admin123")
 *   - response_out: buffer para guardar a resposta do servidor (pode ser NULL)
 *
 * Valor de retorno:
 *   1 = conseguiu enviar e receber resposta
 *   0 = falhou (conexão, socket, etc.)
 *
 * Como funciona passo a passo:
 *   1. Criar novo socket TCP (AF_INET, SOCK_STREAM)
 *   2. Se falhar, imprimir erro e devolver 0
 *   3. Conectar ao servidor (usar addr global com IP e porto)
 *   4. Se falhar, imprimir erro, fechar socket, devolver 0
 *   5. Enviar comando ao servidor com send()
 *   6. Receber resposta do servidor com recv()
 *   7. Fechar socket com close() para libertar recursos
 *   8. Se response_out não é NULL, guardar resposta num buffer
 *   9. Devolver 1 (sucesso)
 *
 * Nota sobre modelo bloqueante:
 *   - connect() bloqueia até conectar
 *   - recv() bloqueia até receber dados
 *   - Não há timeout, portanto se servidor não responder, fica bloqueado
 *   - Modelo simples adequado para Etapa 2
 *
 * Nota sobre ligações:
 *   - Cada chamada a call_server() cria e fecha uma ligação completa
 *   - Não reutiliza ligações (modelo sem estado/stateless)
 *   - Cada comando é independente
 *
 * ============================================================================
 */
int call_server(const char* cmd, char* response_out) {
    /* PASSO 1: Criar socket TCP
     * AF_INET        = IPv4
     * SOCK_STREAM    = TCP (conexão garantida)
     * 0              = protocolo por defeito
     */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("\n\033[1;31m[ERRO] Nao foi possivel criar socket.\033[0m\n");
        return 0;
    }

    /* PASSO 2: Conectar ao servidor
     * addr = estrutura global com IP e porto do servidor
     * sizeof(addr) = tamanho da estrutura
     * Retorna 0 se bem-sucedido, -1 se falhou
     */
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        printf("\n\033[1;31m[ERRO] Servidor inacessivel.\033[0m\n");
        close(fd);
        return 0;
    }

    /* PASSO 3: Enviar comando ao servidor
     * send(file_descriptor, dados, quantidade_bytes, flags)
     * strlen(cmd) = número de bytes a enviar
     * 0 = flags normais (sem opções especiais)
     */
    send(fd, cmd, strlen(cmd), 0);

    /* PASSO 4: Receber resposta do servidor
     * recv(file_descriptor, buffer, tamanho_máximo, flags)
     * Bloqueia até receber dados
     */
    char res[BUF_SIZE] = {0}; /* Limpar buffer */
    recv(fd, res, BUF_SIZE - 1, 0);

    /* PASSO 5: Fechar socket para libertar recursos da rede */
    close(fd);

    /* PASSO 6: Guardar resposta em buffer (se o chamador pediu) */
    if (response_out) strcpy(response_out, res);

    return 1; /* Sucesso */
}

/* ============================================================================
 * FUNÇÃO: call_server_print()
 * ============================================================================
 *
 * O que esta função faz:
 *   Wrapper simplificado de call_server() que:
 *   1. Envia comando ao servidor
 *   2. Imprime a resposta no ecrã de forma bonita
 *   3. Pede ao utilizador para pressionar ENTER antes de voltar ao menu
 *
 * Para que é importante:
 *   Evita repetir código nos menus (GET_INFO, ECHO, LIST_ALL, etc.)
 *   Oferece UX consistente com pausa antes de voltar ao menu.
 *
 * Parâmetros:
 *   - cmd: comando a enviar
 *
 * Como funciona passo a passo:
 *   1. Declarar buffer para guardar resposta
 *   2. Chamar call_server() passando o comando
 *   3. Se bem-sucedido (retorno 1):
 *      a) Imprimir "\n[SERVIDOR]:" em verde
 *      b) Imprimir a resposta completa
 *      c) Imprimir nova linha
 *   4. Se falhou, apenas continua
 *   5. Imprimir "Pressione ENTER para voltar ao menu..."
 *   6. Esperar que utilizador pressione ENTER com getchar()
 *
 * ============================================================================
 */
void call_server_print(const char* cmd) {
    char res[BUF_SIZE] = {0}; /* Buffer para resposta */

    /* Chamar servidor e guardar resposta */
    if (call_server(cmd, res)) {
        printf("\n\033[1;32m[SERVIDOR]:\033[0m\n%s\n", res);
    }

    /* Esperar que utilizador pressione ENTER antes de voltar ao menu */
    printf("\n Pressione ENTER para voltar ao menu...");
    getchar();
}

/* ============================================================================
 * FUNÇÃO PRINCIPAL: main()
 * ============================================================================
 *
 * O que esta função faz:
 *   Ponto de entrada do programa cliente. Implementa:
 *   1. Validação de argumentos (IP e porto)
 *   2. Resolução de nomes DNS
 *   3. Teste inicial de conexão
 *   4. Loop principal com menus adaptativos (visitante/admin/user)
 *
 * Parâmetros de linha de comando:
 *   argc = número de argumentos
 *   argv = array de strings (argv[0]=nome programa, argv[1]=IP, argv[2]=porto)
 *
 * Valor de retorno:
 *   -1 se erro na inicialização
 *    0 se encerramento normal
 *
 * Como funciona passo a passo:
 *
 * FASE 1: Validação de argumentos
 *   1. Verificar se foram fornecidos pelo menos 3 argumentos
 *   2. Se não, imprimir instruções de uso e sair
 *
 * FASE 2: Resolução DNS
 *   1. Usar gethostbyname() para resolver argv[1] (IP ou nome)
 *   2. Se falhar, imprimir erro e sair
 *
 * FASE 3: Preparar endereço do servidor
 *   1. Limpar estrutura addr com memset
 *   2. Definir família (AF_INET)
 *   3. Copiar endereço IP do resultado de DNS
 *   4. Converter porto (argv[2]) para integer e depois para big-endian (htons)
 *
 * FASE 4: Teste inicial de conexão
 *   1. Limpar terminal
 *   2. Criar socket de teste
 *   3. Tentar conectar ao servidor
 *   4. Se falhar, imprimir erro CRITICO e sair
 *   5. Se sucesso, fechar socket e continuar
 *
 * FASE 5: Loop principal (3 modos)
 *
 *   MODO VISITANTE (current_user[0] == '\0'):
 *   - Menu: [ 1 ] Login  [ 2 ] Registar  [ 0 ] Sair
 *
 *   Opção 1 (LOGIN):
 *     a) Pedir username e password
 *     b) Construir comando: "AUTH <username> <password>"
 *     c) Chamar servidor
 *     d) Se resposta começa com "AUTH_SUCCESS":
 *        - Guardar nome em current_user
 *        - Definir is_admin se resposta contém "ADMIN"
 *     e) Se "AUTH_PENDING": mostrar aviso (aguarda aprovação)
 *     f) Senão: mostrar erro (credenciais inválidas)
 *
 *   Opção 2 (REGISTER):
 *     a) Pedir username, password, confirmar password
 *     b) Se passwords não coincidem, mostrar erro
 *     c) Senão, construir comando: "REGISTER <username> <password>"
 *     d) Chamar servidor e imprimir resposta
 *
 *   Opção 0 (SAIR): quebra o loop while(1), saindo do programa
 *
 *   MODO ADMINISTRADOR (is_admin == 1):
 *   - Menu: [1] GET_INFO  [2] ECHO  [3] LIST_ALL  [4] CHECK_INBOX
 *           [5] SEND_MSG  [6] APPROVE_USER  [7] DELETE_USER
 *           [8] Logout    [0] Sair
 *
 *   Cada opção:
 *     - Construir comando apropriado
 *     - Chamar call_server_print()
 *
 *   Opção 8 (LOGOUT): limpar current_user e is_admin, voltar ao menu visitante
 *
 *   MODO UTILIZADOR NORMAL (is_admin == 0 E current_user[0] != '\0'):
 *   - Menu: [1] GET_INFO  [2] ECHO  [3] LIST_ALL  [4] CHECK_INBOX
 *           [5] SEND_MSG  [6] Logout  [0] Sair
 *
 *   Similar ao admin mas sem operações administrativas
 *
 * ============================================================================
 */
int main(int argc, char* argv[]) {
    /* ====== FASE 1: Validação de argumentos ====== */
    if (argc < 3) {
        printf("Utilização: ./client <IP_SERVIDOR> <PORTO>\n");
        printf("Exemplo   : ./client 127.0.0.1 10000\n");
        return -1;
    }

    /* ====== FASE 2: Resolução DNS ====== */
    struct hostent* hp = gethostbyname(argv[1]); /* Resolver nome/IP */
    if (!hp) {
        printf("[ERRO] Nao foi possivel resolver o endereco: %s\n", argv[1]);
        return -1;
    }

    /* ====== FASE 3: Preparar estrutura de endereço ====== */
    memset(&addr, 0, sizeof(addr)); /* Limpar estrutura */
    addr.sin_family = AF_INET;      /* IPv4 */
    /* Copiar endereço IP do resultado de DNS */
    memcpy(&addr.sin_addr, hp->h_addr_list[0], hp->h_length);
    addr.sin_port = htons(atoi(argv[2])); /* Converter porto para big-endian */

    /* ====== FASE 4: Teste inicial de conexão ====== */
    system("clear");
    printf("\n A verificar servidor no porto %s...\n", argv[2]);

    int test_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(test_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        printf("\n\033[1;31m[ERRO CRITICO]\033[0m Servidor nao encontrado.\n");
        close(test_fd);
        return -1;
    }
    close(test_fd);
    printf(" \033[1;32m[OK]\033[0m Servidor encontrado. A iniciar C-Cord...\n");
    sleep(1);

    /* ====== FASE 5: LOOP PRINCIPAL ====== */
    while (1) {
        draw_header();

        /* ---- MENU VISITANTE (sem autenticação) ---- */
        if (!current_user[0]) {
            printf(" [ 1 ] F3: Autenticação (Login)\n");
            printf(" [ 2 ] F6: Registar Nova Conta\n");
            printf(" [ 0 ] Sair\n\n Escolha: ");

            int opt;
            if (scanf("%d", &opt) != 1) {
                clear_buffer();
                continue;
            }
            clear_buffer();
            if (opt == 0) break; /* Sair do programa */

            /* ---- F3 — LOGIN ---- */
            if (opt == 1) {
                char u[50], p[50], cmd[150], res[BUF_SIZE];

                printf(" Nome de utilizador: ");
                scanf("%49s", u);
                printf(" Palavra-passe: ");
                scanf("%49s", p);
                clear_buffer();

                /* Construir comando de autenticação */
                sprintf(cmd, "AUTH %s %s", u, p);

                /* Enviar ao servidor e receber resposta */
                if (!call_server(cmd, res)) {
                    sleep(1);
                    continue;
                }

                /* Processar resposta */
                if (strncmp(res, "AUTH_SUCCESS", 12) == 0) {
                    /* Login bem-sucedido */
                    strcpy(current_user, u); /* Guardar nome de utilizador */
                    is_admin =
                        (strstr(res, "ADMIN") != NULL); /* Verificar se admin */
                } else if (strcmp(res, "AUTH_PENDING") == 0) {
                    /* Utilizador registado mas aguarda aprovação */
                    printf(
                        "\n\033[1;33m[AVISO]\033[0m Conta pendente de "
                        "aprovacao pelo administrador.\n");
                    sleep(2);
                } else {
                    /* Credenciais inválidas */
                    printf(
                        "\n\033[1;31m[ERRO]\033[0m Credenciais invalidas.\n");
                    sleep(1);
                }
            }

            /* ---- F6 — REGISTER ---- */
            else if (opt == 2) {
                char u[50], p[50], p2[50], cmd[150];

                printf(" Novo nome de utilizador: ");
                scanf("%49s", u);
                printf(" Palavra-passe: ");
                scanf("%49s", p);
                printf(" Confirmar palavra-passe: ");
                scanf("%49s", p2);
                clear_buffer();

                /* Verificar se passwords coincidem */
                if (strcmp(p, p2) != 0) {
                    printf(
                        "\n\033[1;31m[ERRO]\033[0m As palavras-passe nao "
                        "coincidem.\n");
                    sleep(1);
                    continue;
                }

                /* Construir comando de registo */
                sprintf(cmd, "REGISTER %s %s", u, p);
                call_server_print(cmd); /* Enviar e mostrar resposta */
            }
        }

        /* ---- MENU ADMINISTRADOR ---- */
        else if (is_admin) {
            printf(" [ 1 ] F4: Info do Servidor (GET_INFO)\n");
            printf(" [ 2 ] F4: Testar Echo\n");
            printf(" [ 3 ] F5: Listar Utilizadores (LIST_ALL)\n");
            printf(" [ 4 ] F5: Ver Caixa de Entrada\n");
            printf(" [ 5 ] F5: Enviar Mensagem\n");
            printf(" [ 6 ] F7: Aprovar Utilizador Pendente\n");
            printf(" [ 7 ] F8: Apagar Utilizador\n");
            printf(" [ 8 ] Terminar Sessão\n");
            printf(" [ 0 ] Sair\n\n Escolha: ");

            int opt;
            if (scanf("%d", &opt) != 1) {
                clear_buffer();
                continue;
            }
            clear_buffer();
            if (opt == 0) break;

            /* ---- Opção 1: GET_INFO ---- */
            if (opt == 1) {
                call_server_print("GET_INFO");
            }
            /* ---- Opção 2: ECHO ---- */
            else if (opt == 2) {
                char msg[400], cmd[500];
                printf(" Mensagem: ");
                fgets(msg, 400, stdin);
                msg[strcspn(msg, "\n")] = 0; /* Remover newline */
                sprintf(cmd, "ECHO %s", msg);
                call_server_print(cmd);
            }
            /* ---- Opção 3: LIST_ALL ---- */
            else if (opt == 3) {
                call_server_print("LIST_ALL");
            }
            /* ---- Opção 4: CHECK_INBOX ---- */
            else if (opt == 4) {
                char cmd[100];
                sprintf(cmd, "CHECK_INBOX %s", current_user);
                call_server_print(cmd);
            }
            /* ---- Opção 5: SEND_MSG ---- */
            else if (opt == 5) {
                char dest[50], msg[400], cmd[500];
                printf(" Destinatário: ");
                scanf("%49s", dest);
                clear_buffer();
                printf(" Mensagem: ");
                fgets(msg, 400, stdin);
                msg[strcspn(msg, "\n")] = 0; /* Remover newline */
                sprintf(cmd, "SEND_MSG %s %s %s", dest, current_user, msg);
                call_server_print(cmd);
            }
            /* ---- Opção 6: APPROVE_USER (apenas admin) ---- */
            else if (opt == 6) {
                char target[50], cmd[150];
                printf(" Nome do utilizador a aprovar: ");
                scanf("%49s", target);
                clear_buffer();
                sprintf(cmd, "APPROVE_USER %s %s", current_user, target);
                call_server_print(cmd);
            }
            /* ---- Opção 7: DELETE_USER (apenas admin) ---- */
            else if (opt == 7) {
                char target[50], cmd[150];
                printf(" Nome do utilizador a apagar: ");
                scanf("%49s", target);
                clear_buffer();
                printf(
                    "\n\033[1;31m[AVISO]\033[0m Confirmar eliminação de '%s'? "
                    "(s/n): ",
                    target);
                char confirm[5];
                scanf("%4s", confirm);
                clear_buffer();

                /* Se confirmou com 's' ou 'S' */
                if (confirm[0] == 's' || confirm[0] == 'S') {
                    sprintf(cmd, "DELETE_USER %s %s", current_user, target);
                    call_server_print(cmd);
                } else {
                    printf("\n Operação cancelada.\n");
                    sleep(1);
                }
            }
            /* ---- Opção 8: LOGOUT ---- */
            else if (opt == 8) {
                current_user[0] = '\0'; /* Limpar nome */
                is_admin = 0;           /* Remover privilégios */
            }
        }

        /* ---- MENU UTILIZADOR NORMAL ---- */
        else {
            printf(" [ 1 ] F4: Info do Servidor (GET_INFO)\n");
            printf(" [ 2 ] F4: Testar Echo\n");
            printf(" [ 3 ] F5: Listar Utilizadores\n");
            printf(" [ 4 ] F5: Ver Caixa de Entrada\n");
            printf(" [ 5 ] F5: Enviar Mensagem\n");
            printf(" [ 6 ] Terminar Sessão\n");
            printf(" [ 0 ] Sair\n\n Escolha: ");

            int opt;
            if (scanf("%d", &opt) != 1) {
                clear_buffer();
                continue;
            }
            clear_buffer();
            if (opt == 0) break;

            /* ---- Opção 1: GET_INFO ---- */
            if (opt == 1) {
                call_server_print("GET_INFO");
            }
            /* ---- Opção 2: ECHO ---- */
            else if (opt == 2) {
                char msg[400], cmd[500];
                printf(" Mensagem: ");
                fgets(msg, 400, stdin);
                msg[strcspn(msg, "\n")] = 0;
                sprintf(cmd, "ECHO %s", msg);
                call_server_print(cmd);
            }
            /* ---- Opção 3: LIST_ALL ---- */
            else if (opt == 3) {
                call_server_print("LIST_ALL");
            }
            /* ---- Opção 4: CHECK_INBOX ---- */
            else if (opt == 4) {
                char cmd[100];
                sprintf(cmd, "CHECK_INBOX %s", current_user);
                call_server_print(cmd);
            }
            /* ---- Opção 5: SEND_MSG ---- */
            else if (opt == 5) {
                char dest[50], msg[400], cmd[500];
                printf(" Destinatário: ");
                scanf("%49s", dest);
                clear_buffer();
                printf(" Mensagem: ");
                fgets(msg, 400, stdin);
                msg[strcspn(msg, "\n")] = 0;
                sprintf(cmd, "SEND_MSG %s %s %s", dest, current_user, msg);
                call_server_print(cmd);
            }
            /* ---- Opção 6: LOGOUT ---- */
            else if (opt == 6) {
                current_user[0] = '\0'; /* Limpar nome */
                is_admin = 0;           /* Remover qualquer privilégio */
            }
        }
    }

    return 0;
}
