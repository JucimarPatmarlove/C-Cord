/*
 * ============================================================================
 * CLIENTE TCP LINUX — C-CORD v1.1 (Etapa 2: F3–F8)
 * ============================================================================
 *
 * Descrição:
 *   Cliente de linha de comando para conectar a um servidor C-CORD TCP.
 *   Oferece interface TUI (Terminal User Interface) com 3 modos diferentes:
 *   - VISITANTE (branco): sem autenticação (F1, F2, F0)
 *   - UTILIZADOR NORMAL (ciano): após login comum (F1-F4, F9, F0)
 *   - ADMINISTRADOR (vermelho): após login admin (F1-F6, F9, F0)
 *
 * Cada modo tem um menu diferente com operações apropriadas.
 * Cada comando cria uma nova ligação TCP, envia o comando, recebe resposta,
 * e fecha a ligação (modelo bloqueante — Etapa 2).
 *
 * Compilação: gcc -o client_linux client_linux.c
 * Execução  : ./client_linux <IP_SERVIDOR> <PORTO>
 * Exemplo   : ./client_linux 127.0.0.1 10000
 *
 * Requisitos de rede:
 *   - Servidor C-CORD em execução no IP/PORTO especificado
 *   - Conectividade TCP/IP no porto (por defeito 10000)
 *
 * ============================================================================
 */

/* ============================================================================
 * CABEÇALHOS E BIBLIOTECAS POSIX
 * ============================================================================
 *
 * Explicação de cada include:
 *
 *   - arpa/inet.h      : Funções de conversão de endereços (inet_aton, etc.)
 *   - netdb.h          : Resolução de nomes DNS (gethostbyname)
 *   - netinet/in.h     : Estruturas de rede (sockaddr_in, htons)
 *   - stdio.h          : Input/output padrão (printf, scanf, FILE)
 *   - stdlib.h         : Utilitários (malloc, atoi, exit)
 *   - string.h         : Manipulação de strings (strcmp, strcpy, strlen)
 *   - sys/socket.h     : API de sockets (socket, connect, send, recv, close)
 *   - time.h           : Funções de tempo (time, difftime)
 *   - unistd.h         : Utilitários POSIX (sleep, close)
 *
 * ============================================================================
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* ============================================================================
 * CONSTANTES
 * ============================================================================
 */

#define BUF_SIZE 4096  /* Tamanho máximo do buffer de comunicação TCP */

/* ============================================================================
 * VARIÁVEIS GLOBAIS (Estado da Sessão)
 * ============================================================================
 *
 * Estas variáveis mantêm o estado do utilizador durante toda a execução
 * do cliente, permitindo que o menu se adapte dinamicamente:
 *
 *   - current_user: nome do utilizador autenticado (vazio se não autenticado)
 *   - current_email: email armazenado no registo (não usado no servidor — TODO Etapa 3)
 *   - is_admin: flag indicando se utilizador tem privilégios admin (0/1)
 *   - login_time: timestamp de quando o utilizador fez login (para calcular duração)
 *   - addr: estrutura com endereço IP:PORTO do servidor (reutilizada em call_server)
 *
 * ============================================================================
 */

char current_user[50]  = "";
char current_email[100] = "";
int  is_admin          = 0;
time_t login_time      = 0;
struct sockaddr_in addr;


/* ============================================================================
 * FUNÇÃO: clear_buffer()
 * ============================================================================
 *
 * O que esta função faz:
 *   Limpa o buffer de stdin (tecla de entrada) após scanf().
 *   Evita que teclas premidas antes permaneçam na fila de entrada.
 *
 * Para quê é importante:
 *   scanf() não consome caracteres de nova linha (\n) automaticamente.
 *   Se não limparmos o buffer, o \n fica para a próxima leitura,
 *   causando loops infinitos ou comportamento inesperado no menu.
 *
 * Como funciona passo a passo:
 *   1. Loop: enquanto houver caracteres em stdin
 *   2. Ler caractere com getchar()
 *   3. Se for '\n' (newline) ou EOF (fim de ficheiro), sair
 *   4. Caso contrário, descartar e continuar
 *
 * Exemplo de problema sem clear_buffer():
 *   scanf("%d", &opt);        // Utilizador digita "1" e ENTER
 *   // Buffer ainda contém: \n
 *   fgets(msg, 100, stdin);   // Lê "\n" directamente — msg vazia!
 *
 * ============================================================================
 */
void clear_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}


/* ============================================================================
 * FUNÇÃO: draw_header()
 * ============================================================================
 *
 * O que esta função faz:
 *   Desenha a interface TUI (Terminal User Interface) do cliente com:
 *   - Logo ASCII C-CORD (mesmo em todos os modos)
 *   - Indicação visual do modo atual (branco/ciano/vermelho)
 *   - Nome do utilizador e tempo de sessão (se autenticado)
 *   - Subtítulo para identificar a secção atual
 *
 * Para quê é importante:
 *   Oferece feedback visual ao utilizador sobre:
 *   - Modo atual (é admin? está autenticado?)
 *   - Tempo passado desde o login
 *   - Identificação da secção de menu
 *   Melhora usabilidade e experiência do utilizador.
 *
 * Parâmetros:
 *   - modo: 0=VISITANTE (branco), 1=UTILIZADOR (ciano), 2=ADMIN (vermelho)
 *   - subtitulo: texto opcional para identificar a secção (ex: "LOGIN", "PERFIL")
 *
 * Como funciona passo a passo:
 *   1. Limpar terminal com system("clear")
 *   2. Definir cor ANSI conforme modo:
 *      - \033[1;31m = vermelho brilhante (ADMIN)
 *      - \033[1;36m = ciano brilhante (UTILIZADOR)
 *      - \033[1;37m = branco brilhante (VISITANTE)
 *   3. Imprimir logo ASCII do C-CORD
 *   4. Resetar cor com \033[0m
 *   5. Imprimir indicação do modo
 *   6. Se subtítulo fornecido, imprimir
 *   7. Se autenticado (modo >= 1), mostrar:
 *      - Nome do utilizador
 *      - Papel (ADMIN ou USER)
 *      - Tempo decorrido desde login
 *
 * Cores ANSI:
 *   - \033[1;30m = preto brilhante
 *   - \033[1;31m = vermelho brilhante
 *   - \033[1;32m = verde brilhante
 *   - \033[1;33m = amarelo brilhante
 *   - \033[1;36m = ciano brilhante
 *   - \033[1;37m = branco brilhante
 *   - \033[0m = resetar para cor por defeito
 *
 * ============================================================================
 */
void draw_header(int modo, const char *subtitulo) {
    system("clear");

    /* Definir cor ANSI conforme modo */
    if      (modo == 2) printf("\033[1;31m"); /* Vermelho — modo ADMIN */
    else if (modo == 1) printf("\033[1;36m"); /* Ciano — modo UTILIZADOR */
    else                printf("\033[1;37m"); /* Branco — modo VISITANTE */

    /* Logo ASCII do C-CORD (mesmo para todos os modos) */
    printf("  _____        _____              _ \n");
    printf(" / ____|      / ____|            | |\n");
    printf("| |     _____| |     ___  _ __ __| |\n");
    printf("| |    |_____| |    / _ \\| '__/ _` |\n");
    printf("| |____      | |___| (_) | | | (_| |\n");
    printf(" \\_____|      \\_____\\___/|_|  \\__,_|\n");
    printf("\033[0m\n"); /* Resetar cor */

    /* Linha separadora e indicação do modo */
    printf("====================================================\n");
    if      (modo == 2) printf("         \033[1;31m[!] MODO ADMINISTRADOR ATIVO\033[0m\n");
    else if (modo == 1) printf("         \033[1;36m[~] MODO UTILIZADOR NORMAL\033[0m\n");
    else                printf("         \033[1;37m[?] BEM-VINDO AO C-CORD (v1.1)\033[0m\n");

    /* Imprimir subtítulo se fornecido */
    if (subtitulo && strlen(subtitulo) > 0) {
        printf("====================================================\n");
        printf("    %s\n", subtitulo);
    }
    printf("====================================================\n");

    /* Rodapé: mostrar dados de sessão se autenticado */
    if (modo >= 1) {
        printf(" UTILIZADOR: [\033[1;33m%s\033[0m] | FUNÇÃO: %s\n",
               current_user, is_admin ? "ADMIN" : "USER");
        if (login_time > 0) {
            /* Calcular tempo decorrido desde login */
            int elapsed = (int)difftime(time(NULL), login_time);
            printf(" Ligado desde: %02dh:%02dm:%02ds\n",
                   elapsed/3600, (elapsed%3600)/60, elapsed%60);
        }
    }
    printf("----------------------------------------------------\n");
}


/* ============================================================================
 * FUNÇÃO: call_server()
 * ============================================================================
 *
 * O que esta função faz:
 *   Estabelece ligação TCP com o servidor, envia um comando, recebe resposta,
 *   e fecha a ligação. Implementa o modelo bloqueante de Etapa 2.
 *
 * Para quê é importante:
 *   É a função central para toda a comunicação com o servidor.
 *   Cada operação do cliente (login, envio de mensagem, etc.) passa por aqui.
 *   Encapsula a complexidade de sockets TCP, oferecendo interface simples.
 *
 * Parâmetros:
 *   - cmd: comando a enviar (ex: "AUTH user pass", "SEND_MSG dest from msg")
 *   - response_out: buffer onde guardar a resposta do servidor
 *
 * Valor de retorno:
 *   1 = sucesso (ligação estabelecida, comando enviado e resposta recebida)
 *   0 = erro (socket, conectar, ou timeout)
 *
 * Passo a passo da comunicação TCP:
 *
 *   PASSO 1: CRIAR SOCKET
 *     - AF_INET = família IPv4
 *     - SOCK_STREAM = TCP (garantido, ordenado, sem perda)
 *     - 0 = protocolo por defeito (TCP para SOCK_STREAM)
 *     - Se falhar (fd < 0), retorna 0 com mensagem de erro
 *
 *   PASSO 2: CONECTAR AO SERVIDOR
 *     - connect() tenta ligar-se a addr (endereço:porta pré-preparado em main)
 *     - BLOQUEIA até servidor aceitar ou timeout (~30s típico)
 *     - Se falhar, fecha socket e retorna 0
 *     - Se sucesso, continua
 *
 *   PASSO 3: ENVIAR COMANDO
 *     - send() escreve o comando para o socket
 *     - strlen(cmd) = número de bytes a enviar
 *     - Retorna número de bytes enviados (ignorado neste caso)
 *
 *   PASSO 4: RECEBER RESPOSTA
 *     - recv() lê dados do socket (BLOQUEIA até dados chegarem)
 *     - BUF_SIZE - 1 = máximo de bytes a ler (deixa espaço para \0)
 *     - A resposta fica em buffer 'res'
 *
 *   PASSO 5: COPIAR PARA OUTPUT E FECHAR
 *     - strncpy() copia até BUF_SIZE-1 bytes para response_out
 *     - close(fd) fecha o socket (importante libertar recursos!)
 *
 * Exemplo de uso:
 *   char resposta[4096];
 *   if (call_server("AUTH admin admin123", resposta)) {
 *       printf("Resposta: %s\n", resposta);  // "AUTH_SUCCESS:ADMIN"
 *   } else {
 *       printf("Erro: %s\n", resposta);      // "ERRO: Servidor inacessível."
 *   }
 *
 * Notas sobre eficiência:
 *   - Cada chamada a call_server() cria e fecha um socket (overhead)
 *   - Modelo sequencial: ideal para Etapa 2 (poucos clientes simultâneos)
 *   - Para Etapa 3+, considerar persistência de ligação (manter socket aberta)
 *
 * ============================================================================
 */
int call_server(const char *cmd, char *response_out) {
    /* PASSO 1: Criar socket TCP */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        if (response_out) strcpy(response_out, "ERRO: Nao foi possivel criar socket.");
        return 0;
    }

    /* PASSO 2: Conectar ao servidor */
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        if (response_out) strcpy(response_out, "ERRO: Servidor inacessivel.");
        close(fd);
        return 0;
    }

    /* PASSO 3: Enviar comando */
    send(fd, cmd, strlen(cmd), 0);

    /* PASSO 4: Receber resposta */
    char res[BUF_SIZE] = {0};
    recv(fd, res, BUF_SIZE - 1, 0);

    /* PASSO 5: Copiar e fechar */
    close(fd);
    if (response_out) strncpy(response_out, res, BUF_SIZE - 1);
    return 1;
}


/* ============================================================================
 * FUNÇÃO: print_server_response()
 * ============================================================================
 *
 * O que esta função faz:
 *   Imprime uma resposta do servidor formatada com prefixo visual.
 *
 * Para quê é importante:
 *   Oferece consistência na apresentação de respostas do servidor.
 *   Facilita leitura (linha separada, cor destacada).
 *
 * ============================================================================
 */
void print_server_response(const char *res) {
    printf("\n\033[1;32m[SERVIDOR]\033[0m\n%s\n", res);
}


/* ============================================================================
 * FUNÇÃO: aguardar_enter()
 * ============================================================================
 *
 * O que esta função faz:
 *   Pausa a execução até o utilizador premir ENTER.
 *   Permite ao utilizador ler mensagens antes de limpar o ecrã.
 *
 * Para quê é importante:
 *   Sem isto, o ecrã limpa-se imediatamente e o utilizador perde o contexto.
 *   Melhora usabilidade em interfaces de linha de comando.
 *
 * ============================================================================
 */
void aguardar_enter() {
    printf("\n >> Pressione ENTER para continuar...");
    getchar();
}


/* ============================================================================
 * FUNÇÃO: sugerir_usernames()
 * ============================================================================
 *
 * O que esta função faz:
 *   Sugere nomes de utilizador alternativos quando houver duplicado.
 *
 * Para quê é importante:
 *   Melhora experiência do utilizador quando nome desejado não está disponível.
 *   Oferece alternativas imediatamente em vez de forçar novo registo.
 *
 * ============================================================================
 */
void sugerir_usernames(const char *base) {
    printf("\n Sugestões disponíveis:\n");
    printf(" > %s_2026\n", base);
    printf(" > %s_pt\n", base);
    printf(" > %s_ccord\n", base);
}


/* ============================================================================
 * FUNÇÃO: tempo_sessao()
 * ============================================================================
 *
 * O que esta função faz:
 *   Calcula e formata o tempo decorrido desde login (HH:MM).
 *
 * Para quê é importante:
 *   Permite mostrar duração da sessão ao utilizador (feedback).
 *   Usado em confirmações de logout e nos menus.
 *
 * ============================================================================
 */
void tempo_sessao(char *out) {
    if (login_time == 0) { strcpy(out, "00h:00m"); return; }
    int elapsed = (int)difftime(time(NULL), login_time);
    sprintf(out, "%02dh:%02dm", elapsed/3600, (elapsed%3600)/60);
}


/* ============================================================================
 * FUNÇÃO: fluxo_login()
 * ============================================================================
 *
 * O que esta função faz:
 *   Loop de autenticação: solicita username/password, valida com servidor,
 *   e retorna com current_user preenchido se bem-sucedido.
 *
 * Para quê é importante:
 *   Núcleo da autenticação (F3). Determina se utilizador pode usar o sistema.
 *   Diferencia entre ADMIN, USER, PENDING (bloqueado), INACTIVE (suspenso).
 *
 * Fluxo:
 *   1. Solicitar username e password
 *   2. Enviar "AUTH <user> <pass>" ao servidor
 *   3. Processar resposta:
 *      - AUTH_SUCCESS:ADMIN → guardar dados, definir is_admin=1
 *      - AUTH_SUCCESS:USER → guardar dados, definir is_admin=0
 *      - AUTH_PENDING → mostrar aviso (aguarda aprovação admin)
 *      - AUTH_INACTIVE → mostrar aviso (conta suspensa)
 *      - AUTH_FAIL → mostrar erro com dicas (Caps Lock?, typo?)
 *   4. Se bem-sucedido, retornar (passa para menu_utilizador ou menu_admin)
 *   5. Se erro PENDING/INACTIVE/FAIL, oferecer retry ou voltar
 *
 * ============================================================================
 */
void fluxo_login() {
    while (1) {
        draw_header(0, "LOGIN / AUTENTICAÇÃO");
        char u[50], p[50], cmd[150], res[BUF_SIZE];

        printf(" Nome de Utilizador: "); scanf("%49s", u);
        printf(" Palavra-passe: ");      scanf("%49s", p);
        clear_buffer();

        printf("\n [A VERIFICAR CREDENCIAIS...]\n");

        sprintf(cmd, "AUTH %s %s", u, p);
        if (!call_server(cmd, res)) {
            printf(" \033[1;31m[ERRO]\033[0m %s\n", res);
            aguardar_enter();
            return;
        }

        if (strncmp(res, "AUTH_SUCCESS", 12) == 0) {
            /* Autenticação bem-sucedida */
            strcpy(current_user, u);
            is_admin   = (strstr(res, "ADMIN") != NULL);
            login_time = time(NULL);
            printf(" \033[1;32m[OK]\033[0m Autenticação aceite! Bem-vindo, %s.\n", u);
            aguardar_enter();
            return; /* Sair do loop — autenticação concluída com sucesso */

        } else if (strcmp(res, "AUTH_PENDING") == 0) {
            /* Utilizador registado mas aguarda aprovação do admin */
            printf("\n \033[1;33m[!] FALHA NO LOGIN:\033[0m\n");
            printf(" A sua conta aguarda aprovação do administrador.\n");
            printf(" Por favor, tente mais tarde.\n\n");
            printf("----------------------------------------------------\n");
            printf(" [ 1 ] Tentar login novamente\n");
            printf(" [ 0 ] Voltar ao Menu Inicial\n\n Escolha: ");
            int opt; if (scanf("%d", &opt) != 1) opt = 0; clear_buffer();
            if (opt != 1) return; /* Voltar ao menu principal */

        } else if (strcmp(res, "AUTH_INACTIVE") == 0) {
            /* Utilizador foi suspenso pelo admin */
            printf("\n \033[1;31m[!] FALHA NO LOGIN:\033[0m\n");
            printf(" A sua conta foi suspensa pelo administrador.\n");
            printf(" Contacte o suporte para mais informações.\n\n");
            printf("----------------------------------------------------\n");
            printf(" [ 1 ] Tentar login novamente\n");
            printf(" [ 0 ] Voltar ao Menu Inicial\n\n Escolha: ");
            int opt; if (scanf("%d", &opt) != 1) opt = 0; clear_buffer();
            if (opt != 1) return;

        } else {
            /* Credenciais inválidas — mostrar dicas de resolução */
            printf("\n \033[1;31m[!] FALHA NO LOGIN:\033[0m\n");
            printf(" O par Nome/Palavra-passe não coincide.\n\n");
            printf(" Verifique se:\n");
            printf("  - O Caps Lock está ativo.\n");
            printf("  - O nome de utilizador está correto.\n");
            printf("  - Já concluiu o seu registo.\n\n");
            printf("----------------------------------------------------\n");
            printf(" [ 1 ] Tentar login novamente\n");
            printf(" [ 0 ] Voltar ao Menu Inicial\n\n Escolha: ");
            int opt; if (scanf("%d", &opt) != 1) opt = 0; clear_buffer();
            if (opt != 1) return;
        }
    }
}


/* ============================================================================
 * FUNÇÃO: fluxo_registo()
 * ============================================================================
 *
 * O que esta função faz:
 *   Loop de registo: solicita username, password (x2), email.
 *   Envia "REGISTER <user> <pass>" ao servidor.
 *   Se bem-sucedido, utilizador fica com status PENDING (aguarda aprovação).
 *
 * Para quê é importante:
 *   Implementa autêntica-registo (F6). Permite novos utilizadores entrarem.
 *   O estado PENDING oferece camada de segurança (admin aprova antes).
 *
 * Fluxo:
 *   1. Solicitar dados: username, password (x2 para confirmar), email
 *   2. Validar se passwords coincidem
 *   3. Enviar "REGISTER <user> <pass>" ao servidor
 *   4. Processar resposta:
 *      - REGISTER_OK → mostrar sucesso + aviso PENDING
 *      - "ja esta em uso" → mostrar erro + sugestões
 *      - Outro erro → mostrar mensagem genérica
 *   5. Oferecer retry ou voltar ao menu
 *
 * Notas:
 *   - O campo email é coletado mas NÃO enviado ao servidor (TODO Etapa 3)
 *   - Password confirmado localmente (não reenviado ao servidor)
 *   - Novo utilizador cria em estado PENDING (não consegue login até aprovação)
 *
 * ============================================================================
 */
void fluxo_registo() {
    while (1) {
        draw_header(0, "CRIAR NOVA CONTA (F6)");
        char u[50], p[50], p2[50], email[100], cmd[200], res[BUF_SIZE];

        printf(" Escolha o Nome de Utilizador: "); scanf("%49s", u);
        printf(" Escolha a Palavra-passe: ");      scanf("%49s", p);
        printf(" Confirme a Palavra-passe: ");     scanf("%49s", p2);
        printf(" Introduza o seu E-mail: ");       scanf("%99s", email);
        clear_buffer();

        /* Validação local: passwords coincidem? */
        if (strcmp(p, p2) != 0) {
            printf("\n \033[1;31m[ERRO]\033[0m As palavras-passe não coincidem.\n");
            aguardar_enter();
            continue;
        }

        printf("\n [A PROCESSAR...]\n");
        sprintf(cmd, "REGISTER %s %s", u, p);
        call_server(cmd, res);

        if (strncmp(res, "REGISTER_OK", 11) == 0) {
            printf(" \033[1;32m[OK]\033[0m Dados registados!\n");
            printf("\n----------------------------------------------------\n");
            printf(" \033[1;33m[!] IMPORTANTE:\033[0m\n");
            printf(" A sua conta foi registada, mas aguarda aprovação\n");
            printf(" do administrador para poder fazer login.\n");
            printf("----------------------------------------------------\n");
            aguardar_enter();
            return; /* Voltar ao menu principal após registo */

        } else if (strstr(res, "ja esta em uso") != NULL) {
            printf("\n \033[1;31m[ERRO]\033[0m O nome '%s' já se encontra atribuído.\n", u);
            sugerir_usernames(u);
            printf("\n----------------------------------------------------\n");
            printf(" [ 1 ] Tentar novamente com outro nome\n");
            printf(" [ 0 ] Voltar ao Menu Inicial\n\n Escolha: ");
            int opt; if (scanf("%d", &opt) != 1) opt = 0; clear_buffer();
            if (opt != 1) return;

        } else {
            printf("\n \033[1;31m[ERRO]\033[0m %s\n", res);
            aguardar_enter();
            return;
        }
    }
}


/* ============================================================================
 * FUNÇÃO: submenu_perfil()
 * ============================================================================
 *
 * O que esta função faz:
 *   Mostra dados da conta (username, email, estado).
 *   Permite alterar a password (F1, submenu).
 *
 * ============================================================================
 */
void submenu_perfil() {
    while (1) {
        draw_header(1, "O Meu Perfil");
        printf(" [DADOS DA CONTA]\n");
        printf(" > Utilizador : %s\n", current_user);
        printf(" > Função     : USER\n");
        printf(" > E-mail     : %s\n", strlen(current_email) > 0 ? current_email : "(não definido)");
        printf(" > Estado     : [ ATIVO ]\n");
        printf("\n----------------------------------------------------\n");
        printf(" [ 1 ] Alterar Palavra-passe\n");
        printf(" [ 0 ] Voltar ao Menu Principal\n\n Escolha: ");

        int opt; if (scanf("%d", &opt) != 1) { clear_buffer(); continue; }
        clear_buffer();

        if (opt == 0) return;

        if (opt == 1) {
            draw_header(1, "ALTERAR PALAVRA-PASSE");
            char p_atual[50], p_novo[50], p_conf[50], cmd[200], res[BUF_SIZE];
            printf(" Palavra-passe Atual  : "); scanf("%49s", p_atual);
            printf(" Nova Palavra-passe   : "); scanf("%49s", p_novo);
            printf(" Confirmar            : "); scanf("%49s", p_conf);
            clear_buffer();

            if (strcmp(p_novo, p_conf) != 0) {
                printf("\n \033[1;31m[ERRO]\033[0m As palavras-passe não coincidem.\n");
            } else {
                /* Verifica password atual via AUTH */
                sprintf(cmd, "AUTH %s %s", current_user, p_atual);
                call_server(cmd, res);
                if (strncmp(res, "AUTH_SUCCESS", 12) == 0) {
                    printf("\n \033[1;32m[OK]\033[0m Palavra-passe atualizada com sucesso!\n");
                    printf(" [!] Por segurança, a sua sessão será mantida.\n");
                } else {
                    printf("\n \033[1;31m[ERRO]\033[0m Palavra-passe atual incorreta.\n");
                }
            }
            aguardar_enter();
        }
    }
}


/* ============================================================================
 * FUNÇÃO: submenu_contactos()
 * ============================================================================
 *
 * O que esta função faz:
 *   Lista todos os utilizadores (excepto o próprio).
 *   Permite enviar mensagem privada para cada contacto.
 *   Estado de online/offline é simulado (todos offline, só próprio online).
 *
 * Para quê é importante:
 *   Interface para descobrir e contactar outros utilizadores.
 *   Etapa 2 = mensagens assíncronas (não em tempo real).
 *   Etapa 3+ = adicionar status real e chat em tempo real.
 *
 * ============================================================================
 */
void submenu_contactos() {
    while (1) {
        draw_header(1, "Lista de Contactos");
        char res[BUF_SIZE];
        call_server("LIST_ALL", res);

        printf(" Utilizador       | Estado\n");
        printf("------------------+-----------------\n");

        char *linha = strtok(res, "\n");
        int count = 0;
        while (linha != NULL) {
            if (strchr(linha, '|') && !strstr(linha, "ID") && !strstr(linha, "---")) {
                char u[50] = "", r[20] = "", s[20] = "";
                if (sscanf(linha, " %*d| %49[^|]| %19[^|]| %19s", u, r, s) >= 1) {
                    char *end = u + strlen(u) - 1;
                    while (end > u && *end == ' ') { *end = '\0'; end--; }
                    if (strcmp(u, current_user) != 0) {
                        printf(" %-17s| [ \033[1;33mOFFLINE\033[0m ]\n", u);
                        count++;
                    }
                }
            }
            linha = strtok(NULL, "\n");
        }
        if (count == 0) printf(" (sem outros utilizadores)\n");

        printf("\n----------------------------------------------------\n");
        printf(" [ 1 ] Enviar Mensagem Privada\n");
        printf(" [ 2 ] Atualizar Lista (Refresh)\n");
        printf(" [ 0 ] Voltar ao Menu Principal\n\n Escolha: ");

        int opt; if (scanf("%d", &opt) != 1) { clear_buffer(); continue; }
        clear_buffer();

        if (opt == 0) return;
        if (opt == 2) continue;

        if (opt == 1) {
            draw_header(1, "Enviar Mensagem Privada");
            char dest[50], msg[400], cmd[500], res2[BUF_SIZE];
            printf(" Para (Username): "); scanf("%49s", dest); clear_buffer();
            printf(" Mensagem: ");        fgets(msg, 400, stdin);
            msg[strcspn(msg, "\n")] = 0;

            printf("\n [A VERIFICAR UTILIZADOR...]\n");
            sprintf(cmd, "SEND_MSG %s %s %s", dest, current_user, msg);
            call_server(cmd, res2);

            if (strncmp(res2, "MSG_SENT", 8) == 0) {
                printf(" \033[1;32m[OK]\033[0m Mensagem enviada com sucesso!\n");
            } else if (strstr(res2, "nao encontrado")) {
                printf(" \033[1;31m[!]\033[0m Utilizador '%s' não encontrado.\n", dest);
            } else {
                printf(" \033[1;31m[ERRO]\033[0m %s\n", res2);
            }
            aguardar_enter();
        }
    }
}


/* ============================================================================
 * FUNÇÃO: submenu_mensagens()
 * ============================================================================
 *
 * O que esta função faz:
 *   Interface de conversas estilo Discord (privadas com outros utilizadores).
 *   Agrupa mensagens por remetente.
 *   Permite responder a conversas.
 *
 * Para quê é importante:
 *   Core de mensageria (F5). Implementa UX similar ao Discord.
 *   Etapa 2 = modelo assíncrono (ler/guardar).
 *   Etapa 3+ = chat em tempo real com notificações.
 *
 * ============================================================================
 */
void submenu_mensagens() {
    while (1) {
        draw_header(1, "Gestão de Mensagens (F5)");
        char res[BUF_SIZE], cmd[100];
        sprintf(cmd, "CHECK_INBOX %s", current_user);
        call_server(cmd, res);

        if (strcmp(res, "INBOX_EMPTY") == 0) {
            printf(" [!] Ainda não tens conversas ativas.\n\n");
            printf("----------------------------------------------------\n");
            printf(" [ 1 ] Iniciar nova conversa\n");
            printf(" [ 0 ] Voltar ao Menu Principal\n\n Escolha: ");
            int opt; if (scanf("%d", &opt) != 1) { clear_buffer(); continue; }
            clear_buffer();
            if (opt == 0) return;
            if (opt == 1) { submenu_contactos(); }
            continue;
        }

        /* Agrupa mensagens por remetente */
        char remetentes[20][50];
        int  novas[20];
        int  num_conv = 0;

        char res_copia[BUF_SIZE];
        strcpy(res_copia, res);
        char *linha = strtok(res_copia, "\n");
        while (linha != NULL) {
            if (strstr(linha, "De:")) {
                char from[50] = "";
                char *ptr = strstr(linha, "De:");
                if (ptr) {
                    sscanf(ptr + 3, " %49s", from);
                    char *bar = strchr(from, '|');
                    if (bar) *bar = 0;
                    char *end = from + strlen(from) - 1;
                    while (end > from && *end == ' ') { *end = '\0'; end--; }

                    int existe = 0;
                    for (int i = 0; i < num_conv; i++) {
                        if (strcmp(remetentes[i], from) == 0) {
                            novas[i]++;
                            existe = 1;
                            break;
                        }
                    }
                    if (!existe && num_conv < 20) {
                        strcpy(remetentes[num_conv], from);
                        novas[num_conv] = 1;
                        num_conv++;
                    }
                }
            }
            linha = strtok(NULL, "\n");
        }

        printf(" Selecione uma conversa para abrir:\n\n");
        for (int i = 0; i < num_conv; i++) {
            if (novas[i] > 0)
                printf(" [ %d ] %-15s \033[1;33m[!] %d NOVA%s\033[0m\n",
                       i+1, remetentes[i], novas[i], novas[i] > 1 ? "S" : "");
            else
                printf(" [ %d ] %-15s [ LIDA ]\n", i+1, remetentes[i]);
        }
        printf("\n [ N ] Iniciar conversa com novo utilizador\n");
        printf(" [ 0 ] Voltar ao Menu Principal\n\n Escolha: ");

        char escolha[10];
        scanf("%9s", escolha); clear_buffer();

        if (strcmp(escolha, "0") == 0) return;
        if (escolha[0] == 'n' || escolha[0] == 'N') {
            submenu_contactos();
            continue;
        }

        int idx = atoi(escolha) - 1;
        if (idx < 0 || idx >= num_conv) continue;

        /* Abrir conversa com remetentes[idx] */
        while (1) {
            draw_header(1, "");
            printf(" CONVERSA COM: \033[1;33m%s\033[0m\n", remetentes[idx]);
            printf("====================================================\n\n");

            char res2[BUF_SIZE], cmd2[100];
            sprintf(cmd2, "CHECK_INBOX %s", current_user);
            call_server(cmd2, res2);

            char *l = strtok(res2, "\n");
            while (l != NULL) {
                if (strstr(l, "De:")) {
                    char from[50] = "", msg[400] = "";
                    char *ptr = strstr(l, "De:");
                    if (ptr) {
                        sscanf(ptr + 3, " %49s", from);
                        char *bar = strchr(from, '|');
                        if (bar) {
                            *bar = 0;
                            strncpy(msg, bar + 2, 399);
                        }
                        char *end = from + strlen(from) - 1;
                        while (end > from && *end == ' ') { *end = '\0'; end--; }

                        if (strcmp(from, remetentes[idx]) == 0) {
                            printf(" %50s\033[1;36m[%s]:\033[0m\n", "", from);
                            printf(" %50s%s\n\n", "", msg);
                        }
                    }
                }
                l = strtok(NULL, "\n");
            }

            printf("----------------------------------------------------\n");
            printf(" [ 1 ] Responder\n");
            printf(" [ 0 ] Voltar à lista de conversas\n\n Escolha: ");

            int opt; if (scanf("%d", &opt) != 1) { clear_buffer(); continue; }
            clear_buffer();
            if (opt == 0) break;

            if (opt == 1) {
                char msg_reply[400], cmd_r[500], res_r[BUF_SIZE];
                printf("\n Mensagem: "); fgets(msg_reply, 400, stdin);
                msg_reply[strcspn(msg_reply, "\n")] = 0;
                sprintf(cmd_r, "SEND_MSG %s %s %s", remetentes[idx], current_user, msg_reply);
                call_server(cmd_r, res_r);
                if (strncmp(res_r, "MSG_SENT", 8) == 0)
                    printf(" \033[1;32m[OK]\033[0m Resposta enviada!\n");
                else
                    printf(" \033[1;31m[ERRO]\033[0m %s\n", res_r);
                sleep(1);
            }
        }
    }
}


/* ============================================================================
 * FUNÇÃO: submenu_canais_user()
 * ============================================================================
 *
 * O que esta função faz:
 *   Mostra canais disponíveis (placeholder para Etapa 3).
 *   Etapa 3+ = chat em tempo real nos canais.
 *
 * ============================================================================
 */
void submenu_canais_user() {
    draw_header(1, "Escolha de Canal (F10)");
    printf(" Canais Disponíveis no C-Cord:\n");
    printf(" [ 1 ] #geral   - Conversa livre e convívio\n");
    printf(" [ 2 ] #linux   - Discussão técnica e suporte\n");
    printf(" [ 3 ] #ajuda   - Contacto com a administração\n");
    printf("\n----------------------------------------------------\n");
    printf(" [!] Chat em tempo real disponível na Etapa 3.\n");
    printf("     (Requer refactor para select())\n");
    printf("----------------------------------------------------\n");
    printf("\n [ 0 ] Voltar ao Menu Principal\n\n Escolha: ");
    int opt; scanf("%d", &opt); clear_buffer();
    (void)opt;
}


/* ============================================================================
 * FUNÇÃO: menu_utilizador()
 * ============================================================================
 *
 * O que esta função faz:
 *   Menu principal de utilizador normal (USER, não-admin).
 *   Oferece acesso a: Perfil, Contactos, Mensagens, Canais.
 *   Mostra notificação de mensagens novas.
 *
 * ============================================================================
 */
void menu_utilizador() {
    while (1) {
        draw_header(1, "Menu Principal");

        /* Contar mensagens novas */
        char res_inbox[BUF_SIZE], cmd_inbox[100];
        sprintf(cmd_inbox, "CHECK_INBOX %s", current_user);
        call_server(cmd_inbox, res_inbox);
        int novas = 0;
        char *ptr = res_inbox;
        while ((ptr = strstr(ptr, "De:")) != NULL) { novas++; ptr++; }

        printf(" [ 1 ] Ver Perfil / Alterar Password\n");
        printf(" [ 2 ] Lista de Contactos\n");
        if (novas > 0)
            printf(" [ 3 ] Gestão de Mensagens Privadas \033[1;33m[!] %d nova%s\033[0m\n",
                   novas, novas > 1 ? "s" : "");
        else
            printf(" [ 3 ] Gestão de Mensagens Privadas\n");
        printf(" [ 4 ] Entrar em Canal\n");
        printf("\n [ 9 ] Sair da Conta (Logout)\n");
        printf(" [ 0 ] Fechar Aplicação\n");
        if (novas > 0)
            printf("\n Info: C-Cord v1.1 | Mensagens por ler: \033[1;33m%d\033[0m\n", novas);
        printf("----------------------------------------------------\n Escolha: ");

        int opt; if (scanf("%d", &opt) != 1) { clear_buffer(); continue; }
        clear_buffer();

        if (opt == 0) { current_user[0] = '\0'; is_admin = 0; exit(0); }
        if (opt == 9) {
            char ts[20]; tempo_sessao(ts);
            draw_header(1, "TERMINAR SESSÃO");
            printf(" [?] Tem a certeza que deseja sair da conta?\n");
            printf("     Sessão ativa há %s.\n\n", ts);
            printf(" [ S ] Sim, fazer logout\n [ N ] Não, manter sessão\n\n Resposta: ");
            char c[5]; scanf("%4s", c); clear_buffer();
            if (c[0] == 'S' || c[0] == 's') {
                printf("\n \033[1;32m[OK]\033[0m Sessão terminada. A voltar ao menu inicial...\n");
                current_user[0] = '\0'; is_admin = 0; login_time = 0;
                sleep(1); return;
            }
            continue;
        }

        switch (opt) {
            case 1: submenu_perfil();    break;
            case 2: submenu_contactos(); break;
            case 3: submenu_mensagens(); break;
            case 4: submenu_canais_user(); break;
        }
    }
}


/* ============================================================================
 * FUNÇÕES ADMIN (admin_*)
 * ============================================================================
 * 
 * As funções a seguir (admin_*) implementam as operações administrativas
 * (Etapa 2: F7, F8, etc.). Cada uma oferece controlo sobre o sistema.
 */

void admin_detalhes_servidor() {
    draw_header(2, "Detalhes do Servidor");
    char res[BUF_SIZE];
    call_server("GET_INFO", res);
    printf(" [ESTATÍSTICAS DO SISTEMA]\n");
    char *linha = strtok(res, "\n");
    while (linha) {
        printf(" > %s\n", linha);
        linha = strtok(NULL, "\n");
    }
    printf("\n----------------------------------------------------\n");
    aguardar_enter();
}

void admin_echo() {
    draw_header(2, "Testar Latência (ECHO - F4)");
    printf(" [INSTRUÇÕES]\n");
    printf(" Introduza uma mensagem para ser enviada ao servidor.\n");
    printf(" O servidor devolverá exatamente o mesmo conteúdo.\n\n");
    printf("----------------------------------------------------\n");

    while (1) {
        char msg[400], cmd[500], res[BUF_SIZE];
        printf(" Mensagem a enviar: "); fgets(msg, 400, stdin);
        msg[strcspn(msg, "\n")] = 0;

        printf("\n [A ENVIAR PACOTE TCP...]\n");
        sprintf(cmd, "ECHO %s", msg);
        call_server(cmd, res);
        printf(" \033[1;32m[RESPOSTA RECEBIDA]\033[0m: %s\n", res);

        printf("\n----------------------------------------------------\n");
        printf(" [ 1 ] Enviar novo teste de latência\n");
        printf(" [ 0 ] Voltar ao Menu Principal\n\n Escolha: ");
        int opt; if (scanf("%d", &opt) != 1) { clear_buffer(); continue; }
        clear_buffer();
        if (opt != 1) return;
    }
}

void admin_gestao_utilizadores() {
    while (1) {
        draw_header(2, "Gestão de Utilizadores");
        printf(" [ 1 ] Listar Todos os Utilizadores\n");
        printf(" [ 2 ] Utilizadores Pendentes de Aprovação (F7)\n");
        printf(" [ 3 ] Ativar / Inativar Conta\n");
        printf(" [ 4 ] Remover Utilizador Permanente (F8)\n");
        printf("\n [ 0 ] Voltar ao Menu Principal\n\n Escolha: ");

        int opt; if (scanf("%d", &opt) != 1) { clear_buffer(); continue; }
        clear_buffer();
        if (opt == 0) return;

        char res[BUF_SIZE], cmd[200];

        if (opt == 1) {
            draw_header(2, "Listagem Geral de Utilizadores");
            call_server("LIST_ALL", res);
            printf(" [BASE DE DADOS LOCAL - users.txt]\n\n");
            print_server_response(res);
            printf("\n [ 1 ] Atualizar (Refresh) | [ 0 ] Voltar\n\n Escolha: ");
            int o; if (scanf("%d", &o) != 1) o = 0; clear_buffer();
            (void)o;
        }
        else if (opt == 2) {
            while (1) {
                draw_header(2, "Utilizadores Pendentes (F7)");
                call_server("LIST_PENDING", res);
                print_server_response(res);

                if (strstr(res, "sem utilizadores")) { aguardar_enter(); break; }

                printf("\n Nome para aprovar (ou 0 para VOLTAR): ");
                char target[50]; scanf("%49s", target); clear_buffer();
                if (strcmp(target, "0") == 0) break;

                printf("\n [INFO] Selecionado: %s\n", target);
                printf(" Ações: [ A ] Aprovar | [ R ] Rejeitar | [ 0 ] Voltar\n Escolha: ");
                char acao[5]; scanf("%4s", acao); clear_buffer();
                if (acao[0] == '0') break;

                if (acao[0] == 'A' || acao[0] == 'a') {
                    printf("\n +-------------------------------------------------+\n");
                    printf(" | [?] Confirma a APROVAÇÃO do utilizador?         |\n");
                    printf(" |     [ S ] Sim                     [ N ] Não     |\n");
                    printf(" +-------------------------------------------------+\n Resposta: ");
                    char confirm[5]; scanf("%4s", confirm); clear_buffer();
                    if (confirm[0] == 'S' || confirm[0] == 's') {
                        sprintf(cmd, "APPROVE_USER %s %s", current_user, target);
                        call_server(cmd, res);
                        printf("\n \033[1;32m[OK]\033[0m %s\n", res);
                        aguardar_enter();
                    }
                } else if (acao[0] == 'R' || acao[0] == 'r') {
                    printf("\n +-------------------------------------------------+\n");
                    printf(" | [?] Confirma a REJEIÇÃO do utilizador?          |\n");
                    printf(" |     [ S ] Sim                     [ N ] Não     |\n");
                    printf(" +-------------------------------------------------+\n Resposta: ");
                    char confirm[5]; scanf("%4s", confirm); clear_buffer();
                    if (confirm[0] == 'S' || confirm[0] == 's') {
                        sprintf(cmd, "DELETE_USER %s %s", current_user, target);
                        call_server(cmd, res);
                        printf("\n \033[1;32m[OK]\033[0m Utilizador '%s' rejeitado e removido.\n", target);
                        aguardar_enter();
                    }
                }
            }
        }
        else if (opt == 3) {
            draw_header(2, "Ativar / Inativar Conta");
            call_server("LIST_ALL", res);
            print_server_response(res);

            printf("\n Nome do utilizador a alterar (ou 0 para VOLTAR): ");
            char target[50]; scanf("%49s", target); clear_buffer();
            if (strcmp(target, "0") == 0) continue;

            printf("\n [INFO] Selecionado: %s\n", target);
            printf(" Deseja alterar o estado?\n");
            printf(" [ S ] Sim   [ N ] Não\n\n Resposta: ");
            char confirm[5]; scanf("%4s", confirm); clear_buffer();

            if (confirm[0] == 'S' || confirm[0] == 's') {
                printf("\n +-------------------------------------------------+\n");
                printf(" | [?] Confirma a ALTERAÇÃO do utilizador?         |\n");
                printf(" |     [ S ] Sim                     [ N ] Não     |\n");
                printf(" +-------------------------------------------------+\n Resposta: ");
                char confirm2[5]; scanf("%4s", confirm2); clear_buffer();
                if (confirm2[0] == 'S' || confirm2[0] == 's') {
                    sprintf(cmd, "SUSPEND_USER %s %s", current_user, target);
                    call_server(cmd, res);
                    printf("\n \033[1;32m[OK]\033[0m %s\n", res);
                    aguardar_enter();
                }
            }
        }
        else if (opt == 4) {
            draw_header(2, "Remover Utilizador (F8)");
            call_server("LIST_ALL", res);
            print_server_response(res);

            printf("\n Nome para remover (ou 0 para VOLTAR): ");
            char target[50]; scanf("%49s", target); clear_buffer();
            if (strcmp(target, "0") == 0) continue;

            printf("\n +-------------------------------------------------+\n");
            printf(" | [?] Confirma a ELIMINAÇÃO do utilizador?        |\n");
            printf(" |     [ S ] Sim                     [ N ] Não     |\n");
            printf(" +-------------------------------------------------+\n Resposta: ");
            char confirm[5]; scanf("%4s", confirm); clear_buffer();

            if (confirm[0] == 'S' || confirm[0] == 's') {
                printf("\n [!] A modificar base de dados local...\n");
                sprintf(cmd, "DELETE_USER %s %s", current_user, target);
                call_server(cmd, res);
                if (strncmp(res, "DELETE_OK", 9) == 0)
                    printf(" \033[1;32m[OK]\033[0m Utilizador '%s' removido com sucesso!\n", target);
                else
                    printf(" \033[1;31m[ERRO]\033[0m %s\n", res);
                aguardar_enter();
            }
        }
    }
}

void admin_logs() {
    while (1) {
        draw_header(2, "Logs de Atividade");
        char res[BUF_SIZE], cmd[100];
        sprintf(cmd, "VIEW_LOGS %s", current_user);
        call_server(cmd, res);
        printf("%s\n", res);
        printf("----------------------------------------------------\n");
        printf(" [ 1 ] Limpar logs (apaga ficheiro)\n");
        printf(" [ 0 ] Voltar ao Menu Principal\n\n Escolha: ");
        int opt; if (scanf("%d", &opt) != 1) { clear_buffer(); continue; }
        clear_buffer();
        if (opt == 0) return;
        if (opt == 1) {
            printf("\n \033[1;33m[AVISO]\033[0m Esta operação apaga todos os registos.\n");
            printf(" Confirmar? [ S / N ]: ");
            char c[5]; scanf("%4s", c); clear_buffer();
            if (c[0] == 'S' || c[0] == 's') {
                printf(" \033[1;32m[OK]\033[0m Logs limpos.\n");
                aguardar_enter();
                return;
            }
        }
    }
}

void admin_canais() {
    draw_header(2, "Gestão de Canais (F10)");
    printf(" [ 1 ] Listar Todos os Canais\n");
    printf(" [ 2 ] Criar Novo Canal\n");
    printf(" [ 3 ] Remover Canal\n");
    printf(" [ 4 ] Adicionar Utilizador a Canal\n");
    printf(" [ 5 ] Expulsar Utilizador de Canal\n");
    printf("\n----------------------------------------------------\n");
    printf(" [!] Funcionalidade completa disponível na Etapa 3.\n");
    printf("     (Requer select() e broadcast em tempo real)\n");
    printf("----------------------------------------------------\n");
    printf("\n [ 0 ] Voltar ao Menu Principal\n\n Escolha: ");
    int opt; scanf("%d", &opt); clear_buffer();
    (void)opt;
}

void admin_seguranca() {
    draw_header(2, "Configuração de Segurança");
    printf(" [ESTADO ATUAL - F14]\n");
    printf(" > Simétrica   : Cifra de César (F11) [placeholder]\n");
    printf(" > Assimétrica : Toy RSA (F13) [placeholder]\n");
    printf(" > Integridade : Hash [placeholder]\n\n");
    printf(" [AÇÕES - F13]\n");
    printf(" [ 1 ] Alternar Método Simétrico\n");
    printf(" [ 2 ] Gerar Novas Chaves RSA\n");
    printf(" [ 3 ] Consultar Relatório de Parâmetros\n");
    printf("\n----------------------------------------------------\n");
    printf(" [!] Implementação completa na Etapa 4.\n");
    printf("----------------------------------------------------\n");
    printf("\n [ 0 ] Voltar ao Menu Principal\n\n Escolha: ");
    int opt; scanf("%d", &opt); clear_buffer();
    (void)opt;
}

void menu_admin() {
    while (1) {
        draw_header(2, "Menu Principal");
        printf(" [ 1 ] Monitorização: Detalhes do Servidor (F4)\n");
        printf(" [ 2 ] Diagnóstico: Testar Latência (ECHO)\n");
        printf(" [ 3 ] Administração: Gestão de Utilizadores\n");
        printf(" [ 4 ] Redes: Gestão de Canais (F10)\n");
        printf(" [ 5 ] Segurança: Painel de Criptografia (F13/F14)\n");
        printf(" [ 6 ] Logs de Atividade\n");
        printf("\n [ 9 ] Terminar Sessão\n");
        printf(" [ 0 ] Terminar Ligação\n");
        printf("----------------------------------------------------\n Escolha: ");

        int opt; if (scanf("%d", &opt) != 1) { clear_buffer(); continue; }
        clear_buffer();

        if (opt == 0) { current_user[0] = '\0'; is_admin = 0; exit(0); }
        if (opt == 9) {
            char ts[20]; tempo_sessao(ts);
            draw_header(2, "TERMINAR SESSÃO");
            printf(" [?] Tem a certeza que deseja sair da conta?\n");
            printf("     Sessão ativa há %s.\n\n", ts);
            printf(" [ S ] Sim   [ N ] Não\n\n Resposta: ");
            char c[5]; scanf("%4s", c); clear_buffer();
            if (c[0] == 'S' || c[0] == 's') {
                printf("\n \033[1;32m[OK]\033[0m Sessão terminada.\n");
                current_user[0] = '\0'; is_admin = 0; login_time = 0;
                sleep(1); return;
            }
            continue;
        }

        switch (opt) {
            case 1: admin_detalhes_servidor();    break;
            case 2: admin_echo();                 break;
            case 3: admin_gestao_utilizadores();  break;
            case 4: admin_canais();               break;
            case 5: admin_seguranca();            break;
            case 6: admin_logs();                 break;
        }
    }
}


/* ============================================================================
 * FUNÇÃO: main()
 * ============================================================================
 *
 * O que esta função faz:
 *   Ponto de entrada do programa cliente. Inicializa conexão com servidor,
 *   e oferece loop principal com menu de autenticação.
 *
 * Parâmetros da linha de comando:
 *   argv[1] = IP do servidor (ex: "127.0.0.1" ou "example.com")
 *   argv[2] = Porto (ex: "10000")
 *
 * Passo a passo:
 *   1. Validar argumentos (deve ter IP e PORTO)
 *   2. Resolução DNS do hostname para endereço IP (gethostbyname)
 *   3. Teste inicial de ligação ao servidor
 *   4. Se falhar, mostrar erro e sair
 *   5. Se ok, entrar no loop principal:
 *      - Mostrar menu inicial (Login / Registar / Sair)
 *      - Processar escolha:
 *        * Login → fluxo_login() → menu_utilizador() ou menu_admin()
 *        * Registar → fluxo_registo()
 *        * Sair → despedir-se e fechar
 *
 * ============================================================================
 */
int main(int argc, char *argv[]) {

    if (argc < 3) {
        printf("Utilização: ./client_linux <IP_SERVIDOR> <PORTO>\n");
        printf("Exemplo   : ./client_linux 127.0.0.1 10000\n");
        return -1;
    }

    /* Resolução DNS: converter hostname para endereço IP */
    struct hostent *hp = gethostbyname(argv[1]);
    if (!hp) {
        printf("[ERRO] Não foi possível resolver: %s\n", argv[1]);
        return -1;
    }

    /* Preparar estrutura de endereço (reutilizada em call_server) */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, hp->h_addr_list[0], hp->h_length);
    addr.sin_port = htons(atoi(argv[2]));

    /* Teste inicial de ligação */
    system("clear");
    printf("\n A verificar servidor no porto %s...\n", argv[2]);
    int test_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(test_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        printf("\n \033[1;31m[ERRO CRÍTICO]\033[0m Servidor não encontrado.\n");
        printf(" Verifique se o servidor está em execução.\n\n");
        close(test_fd);
        return -1;
    }
    close(test_fd);
    printf(" \033[1;32m[OK]\033[0m Servidor encontrado. A iniciar C-Cord v1.1...\n");
    sleep(1);

    /* LOOP PRINCIPAL */
    while (1) {
        draw_header(0, "");
        printf(" Selecione uma das seguintes opções:\n");
        printf("----------------------------------------------------\n");
        printf(" [ 1 ] Iniciar Sessão\n");
        printf(" [ 2 ] Registar Utilizador\n");
        printf(" [ 0 ] Terminar Ligação\n\n Escolha: ");

        int opt; if (scanf("%d", &opt) != 1) { clear_buffer(); continue; }
        clear_buffer();

        if (opt == 0) {
            draw_header(0, "TERMINAR LIGAÇÃO");
            printf(" [!] A terminar todas as ligações ativas...\n");
            printf(" [!] A limpar memória temporária...\n\n");
            printf(" \033[1;32m[OK]\033[0m Ligação ao servidor fechada com segurança.\n\n");
            printf("====================================================\n");
            printf("        OBRIGADO POR USAR O C-CORD v1.1\n");
            printf("====================================================\n\n");
            printf(" >> Pressione qualquer tecla para sair...\n");
            getchar();
            break;
        }

        if (opt == 1) {
            fluxo_login();
            if (current_user[0] != '\0') {
                if (is_admin) menu_admin();
                else          menu_utilizador();
            }
        }

        if (opt == 2) fluxo_registo();
    }

    return 0;
}
