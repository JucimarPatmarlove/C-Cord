/*
 * ============================================================================
 * SERVIDOR TCP C-CORD — VERSÃO 2.0 (Etapa 2: F3, F4, F5, F6, F7, F8)
 * ============================================================================
 *
 * Descrição:
 *   Servidor de rede que implementa um sistema simplificado de chat/forum tipo
 *   Discord. Suporta autenticação, gestão de utilizadores, inbox de mensagens,
 *   e operações administrativas.
 *
 * Compilação: gcc tcp_server_v2.c -o server_v2
 * Execução  : ./server_v2
 * Porto     : 10000
 *
 * Bases de Dados (ficheiros):
 *   - users.txt  → armazena utilizadores (username:password:ROLE:STATUS)
 *   - inbox.txt  → armazena mensagens (destinatario:remetente:mensagem)
 *   - logs.txt   → registo de todas as operações
 *
 * Protocolo de Comandos Suportados:
 *   F3 → AUTH <user> <pass>           : Autentica utilizador
 *   F4 → GET_INFO                     : Obtém informação do servidor
 *   F4 → ECHO <msg>                   : Testa latência (ecoa mensagem)
 *   F5 → LIST_ALL                     : Lista todos os utilizadores
 *   F5 → CHECK_INBOX <user>           : Verifica mensagens de um utilizador
 *   F5 → SEND_MSG <dest> <from> <msg>: Envia mensagem para outro utilizador
 *   F6 → REGISTER <user> <pass>       : Regista novo utilizador (PENDING)
 *   F7 → APPROVE_USER <admin> <user>  : Admin aprova utilizador PENDING
 *   F8 → DELETE_USER <admin> <user>   : Admin apaga utilizador
 *
 * ============================================================================
 */

/* ============================================================================
 * CABEÇALHOS E BIBLIOTECAS
 * ============================================================================
 */

#include <arpa/inet.h>  /* Para inet_ntoa, conversões de endereços */
#include <netinet/in.h> /* Para estruturas sockaddr_in */
#include <stdio.h>      /* Para printf, fprintf, FILE */
#include <stdlib.h>     /* Para malloc, free, exit */
#include <string.h>     /* Para strcmp, strcpy, memset */
#include <sys/socket.h> /* Para socket, bind, listen, accept */
#include <sys/types.h>  /* Para tipos de dados de sistema */
#include <time.h>       /* Para time, localtime, strftime */
#include <unistd.h>     /* Para read, write, close, sleep */

/* ============================================================================
 * CONSTANTES DE CONFIGURAÇÃO
 * ============================================================================
 */

#define SERVER_PORT 10000      /* Porto em que o servidor escuta */
#define BUF_SIZE 2048          /* Tamanho máximo do buffer de dados */
#define USERS_FILE "users.txt" /* Ficheiro de base de dados de utilizadores */
#define INBOX_FILE "inbox.txt" /* Ficheiro de base de dados de mensagens */
#define LOG_FILE "logs.txt"    /* Ficheiro de registos (logs) do servidor */

/* ============================================================================
 * VARIÁVEIS GLOBAIS
 * ============================================================================
 */

const char* VERSAO_SERVIDOR = "2.0-Fase2"; /* Versão actual do servidor */
time_t start_time; /* Momento em que o servidor iniciou */

/* ============================================================================
 * FUNÇÃO: guardar_log()
 * ============================================================================
 *
 * O que esta função faz:
 *   Regista uma mensagem de log no ficheiro de logs (logs.txt) com timestamp
 *   e imprime a mensagem no terminal com cores ANSI conforme o tipo.
 *
 * Para quê é importante:
 *   Permite ao administrador acompanhar todas as operações do servidor em
 *   tempo real (logins, erros, operações administrativas) e manter histórico
 *   persistente das atividades.
 *
 * Parâmetros:
 *   - mensagem: texto a registar
 *   - tipo: 1=SUCESSO (verde), 3=ERRO (vermelho), outro=INFO (ciano)
 *
 * Como funciona passo a passo:
 *   1. Abre ficheiro logs.txt em modo append ("a")
 *   2. Obtém data/hora actual com time() e localtime()
 *   3. Formata a data com strftime() no formato "YYYY-MM-DD HH:MM:SS"
 *   4. Escreve "[TIMESTAMP] mensagem" no ficheiro
 *   5. Fecha o ficheiro para libertar recursos
 *   6. Imprime no terminal com cor:
 *      - Verde ([OK])    se tipo == 1
 *      - Vermelho ([ERRO]) se tipo == 3
 *      - Ciano ([INFO])  para outros valores
 *
 * ============================================================================
 */
void guardar_log(const char* mensagem, int tipo) {
    FILE* f = fopen(LOG_FILE, "a"); /* Abrir ficheiro em modo "append" */
    if (f) {
        char data[64];             /* Buffer para guardar data formatada */
        time_t agora = time(NULL); /* Obter tempo actual em segundos */
        struct tm* t = localtime(&agora); /* Converter para data/hora legível */

        /* Formatar a data no padrão: 2026-05-15 23:53:20 */
        strftime(data, sizeof(data), "%Y-%m-%d %H:%M:%S", t);

        /* Escrever no ficheiro no formato: [TIMESTAMP] mensagem */
        fprintf(f, "[%s] %s\n", data, mensagem);
        fclose(f); /* Fechar ficheiro após escrever */
    }

    /* Imprimir no terminal com cores ANSI */
    if (tipo == 1)
        printf(" \033[1;32m[OK]\033[0m    | %s\n", mensagem);
    else if (tipo == 3)
        printf(" \033[1;31m[ERRO]\033[0m  | %s\n", mensagem);
    else
        printf(" \033[1;36m[INFO]\033[0m  | %s\n", mensagem);
}

/* ============================================================================
 * FUNÇÃO: desenhar_cabecalho_servidor()
 * ============================================================================
 *
 * O que esta função faz:
 *   Desenha a interface de utilizador (TUI) do servidor no terminal com:
 *   - Logo ASCII do C-CORD
 *   - Estado do servidor (ONLINE)
 *   - Porto em que escuta
 *   - Versão do servidor
 *   - Cabeçalho para secção de atividades em tempo real
 *
 * Para quê é importante:
 *   Oferece feedback visual ao operador do servidor sobre o estado e
 *   configuração. Melhora a usabilidade e permite monitorizar atividades.
 *
 * Como funciona passo a passo:
 *   1. Limpa o terminal com system("clear")
 *   2. Define cor ciana com código ANSI \033[1;36m
 *   3. Imprime o logo ASCII do C-CORD
 *   4. Reseta a cor com \033[0m
 *   5. Imprime linhas de separação e informações
 *   6. Define cor verde para [ONLINE]
 *   7. Imprime header da secção de atividade (logs em tempo real)
 *
 * ============================================================================
 */
void desenhar_cabecalho_servidor() {
    system("clear"); /* Limpar terminal */

    printf("\033[1;36m"); /* Definir cor ciana */
    printf("   ____         ____ ___  ____  ____    \n");
    printf("  / ___|       / ___/ _ \\|  _ \\|  _ \\   \n");
    printf(" | |     ____ | |  | | | | |_) | | | |  \n");
    printf(" | |___ |____|| |__| |_| |  _ <| |_| |  \n");
    printf("  \\____|       \\____\\___/|_| \\_\\____/   \n");
    printf("\033[0m\n"); /* Resetar cor */

    printf(
        "======================================================================"
        "==\n");
    printf(
        "         C-CORD SERVER (Etapa 2 — F3/F4/F5/F6/F7/F8)                  "
        " \n");
    printf(
        "======================================================================"
        "==\n");

    /* Mostrar informações do servidor */
    printf(
        " STATUS: \033[1;32mONLINE\033[0m | PORTO: %d | BD: %s | VERSAO: %s\n",
        SERVER_PORT, USERS_FILE, VERSAO_SERVIDOR);

    printf(
        "----------------------------------------------------------------------"
        "--\n");
    printf(" LIVE FEED DE ATIVIDADE:\n");
}

/* ============================================================================
 * FUNÇÃO: check_auth()
 * ============================================================================
 *
 * O que esta função faz:
 *   Valida um par (username, password) contra a base de dados de utilizadores.
 *   Retorna se o utilizador é ADMIN ou USER, ou rejeita se inválido.
 *   Protege utilizadores PENDING contra login.
 *
 * Para quê é importante:
 *   É o núcleo da autenticação (F3). Controla quem pode entrar no sistema.
 *   Diferencia entre ADMIN e USER para aplicar permissões diferentes.
 *   Impede que contas pendentes façam login.
 *
 * Parâmetros:
 *   - username: nome do utilizador a validar
 *   - password: palavra-passe a validar
 *   - role: buffer para devolver o papel (ADMIN ou USER)
 *
 * Valor de retorno:
 *    1 = Autenticação bem-sucedida (STATUS=ACTIVE)
 *   -1 = Utilizador PENDING (bloqueado até aprovação)
 *    0 = Credenciais inválidas
 *
 * Como funciona passo a passo:
 *   1. Abre ficheiro users.txt em modo leitura ("r")
 *   2. Se falhar, registar erro e devolver 0
 *   3. Loop: ler linha por linha (fgets)
 *   4. Parse cada linha com sscanf no formato: "username:password:ROLE:STATUS"
 *   5. Se encontrar match de username E password:
 *      - Verificar se STATUS é "PENDING" → devolver -1
 *      - Copiar ROLE para o buffer de output → devolver 1
 *   6. Se sair do loop sem encontrar → devolver 0
 *
 * Exemplos:
 *   check_auth("admin", "admin123", role_buf);  → Retorna 1, role_buf="ADMIN"
 *   check_auth("novo", "pass123", role_buf);    → Retorna -1 (PENDING)
 *   check_auth("admin", "wrongpass", role_buf); → Retorna 0
 *
 * ============================================================================
 */
int check_auth(const char* username, const char* password, char* role) {
    FILE* f = fopen(USERS_FILE, "r"); /* Abrir base de dados de utilizadores */
    if (!f) {
        guardar_log("users.txt nao encontrado!",
                    3); /* Registar erro se ficheiro não existe */
        return 0;
    }

    char line[256], id[10], u[50], p[50], r[20],
        s[20]; /* Buffers para ler dados */

    /* Loop: ler linha por linha até fim do ficheiro */
    while (fgets(line, sizeof(line), f)) {
        /* Parse da linha no formato: ID:username:password:ROLE:STATUS
           sscanf devolve o número de campos lidos (deve ser 5) */
        if (sscanf(line, "%9[^:]:%49[^:]:%49[^:]:%19[^:]:%19s", id, u, p, r,
                   s) == 5) {
            /* Se encontrou matching do utilizador E password */
            if (strcmp(u, username) == 0 && strcmp(p, password) == 0) {
                fclose(f);

                /* Verificar se utilizador está PENDING (aguardando aprovação)
                 */
                if (strcmp(s, "PENDING") == 0) {
                    return -1; /* Utilizador PENDING — não autoriza login */
                }

                /* Copiar papel para buffer de output */
                strcpy(role, r);
                return 1; /* Autenticação bem-sucedida */
            }
        }
    }
    fclose(f);
    return 0; /* Credenciais não encontradas */
}

/* ============================================================================
 * FUNÇÃO: is_admin()
 * ============================================================================
 *
 * O que esta função faz:
 *   Verifica se um utilizador é ADMINISTRADOR e está ACTIVE.
 *   Usada para proteger operações administrativas críticas.
 *
 * Para quê é importante:
 *   Impede que utilizadores normais executem operações como:
 *   - Aprovar utilizadores PENDING
 *   - Eliminar contas de utilizadores
 *   - Executar comandos administrativos
 *
 * Parâmetros:
 *   - username: nome do utilizador a verificar
 *
 * Valor de retorno:
 *   1 = Utilizador é ADMIN e está ACTIVE
 *   0 = Utilizador não é admin ou não está active
 *
 * Como funciona passo a passo:
 *   1. Abrir users.txt
 *   2. Loop: ler linha por linha
 *   3. Parse cada linha (username:password:ROLE:STATUS)
 *   4. Se encontrar utilizador com ROLE="ADMIN" E STATUS="ACTIVE":
 *      - Fechar ficheiro
 *      - Devolver 1
 *   5. Se sair do loop sem encontrar → devolver 0
 *
 * ============================================================================
 */
int is_admin(const char* username) {
    FILE* f = fopen(USERS_FILE, "r");
    if (!f) return 0; /* Se ficheiro não existe, não é admin */

    char line[256], id[10], u[50], p[50], r[20], s[20];

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%9[^:]:%49[^:]:%49[^:]:%19[^:]:%19s", id, u, p, r,
                   s) == 5) {
            /* Verificar se é o utilizador procurado E tem papel ADMIN E está
             * ACTIVE */
            if (strcmp(u, username) == 0 && strcmp(r, "ADMIN") == 0 &&
                strcmp(s, "ACTIVE") == 0) {
                fclose(f);
                return 1; /* É administrador autorizado */
            }
        }
    }
    fclose(f);
    return 0; /* Não é administrador */
}

/* ============================================================================
 * FUNÇÃO: list_all()
 * ============================================================================
 *
 * O que esta função faz:
 *   Lista todos os utilizadores registados no sistema.
 *   Mostra: username, papel (ADMIN/USER), estado (ACTIVE/PENDING).
 *   Não mostra passwords por razões de segurança.
 *
 * Para quê é importante:
 *   Permite visualizar quem está registado e qual o seu estado.
 *   Útil para administradores gerem o sistema.
 *
 * Parâmetros:
 *   - response: buffer para guardar a resposta a enviar ao cliente
 *
 * Como funciona passo a passo:
 *   1. Abrir users.txt
 *   2. Se não existe, devolver mensagem de erro
 *   3. Inicializar response com cabeçalho da lista
 *   4. Loop: ler linha por linha
 *   5. Parse cada linha (username:password:ROLE:STATUS)
 *   6. Para cada utilizador, construir entrada: "[nº] username | Papel: X |
 * Estado: Y"
 *   7. Concatenar entrada à resposta final
 *   8. Se lista vazia, adicionar "(sem utilizadores)"
 *
 * Exemplo de output:
 *   === UTILIZADORES REGISTADOS ===
 *    [1] admin | Papel: ADMIN | Estado: ACTIVE
 *    [2] user1 | Papel: USER | Estado: ACTIVE
 *    [3] novo | Papel: USER | Estado: PENDING
 *
 * ============================================================================
 */
void list_all(char* response) {
    FILE* f = fopen(USERS_FILE, "r");
    if (!f) {
        strcpy(response, "ERRO: Ficheiro de utilizadores nao encontrado.");
        return;
    }

    strcpy(response, "=== UTILIZADORES REGISTADOS ===\n");
    char line[256], id[10], u[50], p[50], r[20], s[20];
    int count = 0;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%9[^:]:%49[^:]:%49[^:]:%19[^:]:%19s", id, u, p, r,
                   s) == 5) {
            char entry[128];
            /* Nota: não guardamos a password no output por segurança */
            sprintf(entry, " [%s] %s | Papel: %s | Estado: %s\n", id, u, r, s);
            strncat(response, entry, BUF_SIZE - strlen(response) - 1);
        }
    }
    /* Se nenhum utilizador encontrado */
    if (count == 0)
        strncat(response, " (sem utilizadores)\n",
                BUF_SIZE - strlen(response) - 1);
}

/* ============================================================================
 * FUNÇÃO: check_inbox()
 * ============================================================================
 *
 * O que esta função faz:
 *   Lê e devolve todas as mensagens destinadas a um utilizador específico.
 *   As mensagens são lidas do ficheiro inbox.txt.
 *
 * Para quê é importante:
 *   Implementa sistema de mensageria assíncrona entre utilizadores.
 *   Um utilizador pode deixar mensagem para outro mesmo que offline.
 *   Permite consultar a caixa de entrada.
 *
 * Parâmetros:
 *   - username: nome do utilizador cujas mensagens procuramos
 *   - response: buffer para guardar as mensagens encontradas
 *
 * Formato do inbox.txt:
 *   destinatario:remetente:mensagem
 *   Exemplo: user1:admin:Olá user1!
 *
 * Como funciona passo a passo:
 *   1. Tentar abrir ficheiro inbox.txt
 *   2. Se não existe, devolver "(sem mensagens)"
 *   3. Inicializar response com cabeçalho
 *   4. Loop: ler linha por linha
 *   5. Remover '\n' do fim de cada linha
 *   6. Parse no formato: "dest:from:msg"
 *   7. Se destinatário matches username, incluir na resposta
 *   8. Formatação: "[nº] De: remetente → mensagem"
 *   9. Se nenhuma mensagem, adicionar "(sem mensagens novas)"
 *
 * Exemplo de output:
 *   === CAIXA DE ENTRADA DE admin ===
 *    [1] De: user1 → Olá admin!
 *    [2] De: jucimar → Bom dia!
 *
 * ============================================================================
 */
void check_inbox(const char* username, char* response) {
    FILE* f = fopen(INBOX_FILE, "r");
    if (!f) {
        strcpy(response, "A sua caixa de entrada está vazia.");
        return;
    }

    sprintf(response, "=== CAIXA DE ENTRADA DE %s ===\n", username);
    char line[512], dest[50], from[50], msg[400];
    int count = 0;

    while (fgets(line, sizeof(line), f)) {
        /* Remover newline do fim da linha */
        line[strcspn(line, "\n")] = 0;

        /* Parse no formato: dest:from:msg */
        if (sscanf(line, "%49[^:]:%49[^:]:%399[^\n]", dest, from, msg) == 3) {
            /* Se destinatário é o utilizador procurado */
            if (strcmp(dest, username) == 0) {
                char entry[512];
                sprintf(entry, " [%d] De: %s → %s\n", ++count, from, msg);
                strncat(response, entry, BUF_SIZE - strlen(response) - 1);
            }
        }
    }
    fclose(f);

    /* Se nenhuma mensagem encontrada */
    if (count == 0)
        strncat(response, " (sem mensagens novas)\n",
                BUF_SIZE - strlen(response) - 1);
}

/* ============================================================================
 * FUNÇÃO: send_msg()
 * ============================================================================
 *
 * O que esta função faz:
 *   Armazena uma mensagem no ficheiro inbox.txt para entrega futura.
 *   A mensagem fica guardada até o destinatário a ler.
 *
 * Para quê é importante:
 *   Permite mensageria assíncrona entre utilizadores.
 *   Os utilizadores podem trocar mensagens mesmo que não estejam online
 *   simultaneamente.
 *
 * Parâmetros:
 *   - dest: utilizador destinatário
 *   - from: utilizador remetente
 *   - msg: conteúdo da mensagem
 *   - response: buffer para devolver confirmação
 *
 * Como funciona passo a passo:
 *   1. Abrir inbox.txt em modo append ("a") para adicionar nova linha
 *   2. Escrever linha no formato: "dest:from:msg"
 *   3. Fechar ficheiro
 *   4. Se bem-sucedido, devolver: "MSG_SENT: Mensagem entregue..."
 *   5. Se erro, devolver: "ERRO: Nao foi possivel..."
 *
 * Nota: As mensagens nunca são removidas do inbox.txt. Permanecem
 *       guardadas para histórico.
 *
 * ============================================================================
 */
void send_msg(const char* dest, const char* from, const char* msg,
              char* response) {
    FILE* f = fopen(INBOX_FILE, "a"); /* Abrir em append para adicionar */
    if (!f) {
        strcpy(response, "ERRO: Nao foi possivel guardar mensagem.");
        return;
    }

    /* Escrever mensagem no formato: destinatario:remetente:mensagem */
    fprintf(f, "%s:%s:%s\n", dest, from, msg);
    fclose(f);

    sprintf(response, "MSG_SENT: Mensagem entregue na caixa de %s.", dest);
}

/* ============================================================================
 * FUNÇÃO: register_user()
 * ============================================================================
 *
 * O que esta função faz:
 *   Regista um novo utilizador com estado PENDING (aguardando aprovação).
 *   O utilizador PENDING não consegue fazer login até admin o aprovar.
 *
 * Para quê é importante:
 *   Permite qualquer pessoa registar-se, mas protege o sistema requerendo
 *   aprovação de um administrador antes de permitir acesso.
 *   Sistema de segurança para evitar criação descontrolada de contas.
 *
 * Parâmetros:
 *   - username: nome desejado do novo utilizador
 *   - password: palavra-passe
 *   - response: buffer para devolver resultado (sucesso ou erro)
 *
 * Como funciona passo a passo:
 *   1. Verificar se utilizador já existe (lendo users.txt)
 *   2. Se existe, devolver erro: "REGISTER_FAIL: Utilizador ja existe."
 *   3. Se não existe, abrir users.txt em append ("a")
 *   4. Escrever nova linha: "username:password:USER:PENDING"
 *   5. Fechar ficheiro
 *   6. Devolver: "REGISTER_OK: Utilizador registado. Aguarda aprovacao..."
 *
 * Nota: O papel é sempre "USER" para novos utilizadores. Apenas admin
 *       pode ser criado manualmente ou editando users.txt.
 *
 * ============================================================================
 */
void register_user(const char* username, const char* password, char* response) {
    /* Verificar se o utilizador já existe */
    FILE* f = fopen(USERS_FILE, "r");
    if (f) {
        char line[256], u[50];
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "%49[^:]", u) == 1) {
                /* Se encontrar utilizador com mesmo nome */
                if (strcmp(u, username) == 0) {
                    fclose(f);
                    strcpy(response, "REGISTER_FAIL: Utilizador ja existe.");
                    return; /* Sair — não permitir duplicatas */
                }
            }
        }
        fclose(f);
    }

    /* Utilizador não existe — adicionar novo */
    f = fopen(USERS_FILE, "a"); /* Abrir em append */
    if (!f) {
        strcpy(response,
               "ERRO: Nao foi possivel aceder ao ficheiro de utilizadores.");
        return;
    }

    /* Escrever nova linha: username:password:USER:PENDING */
    fprintf(f, "%s:%s:USER:PENDING\n", username, password);
    fclose(f);

    sprintf(response,
            "REGISTER_OK: Utilizador '%s' registado. Aguarda aprovacao do "
            "administrador.",
            username);
}

/* ============================================================================
 * FUNÇÃO: approve_user()
 * ============================================================================
 *
 * O que esta função faz:
 *   Um administrador aprova um utilizador PENDING, alterando estado para
 *   ACTIVE. Só depois o utilizador consegue fazer login.
 *
 * Para quê é importante:
 *   Implementa controlo de aprovação administrativo.
 *   Protege o sistema filtrando utilizadores permitindo acesso controlado.
 *
 * Parâmetros:
 *   - admin_user: nome do admin que aprova
 *   - target: nome do utilizador a aprovar
 *   - response: buffer para devolver resultado
 *
 * Como funciona passo a passo:
 *   1. Verificar se admin_user tem permissões admin (is_admin())
 *   2. Se não é admin, devolver erro
 *   3. Ler todo o ficheiro users.txt para memória (array de linhas)
 *   4. Reabrir users.txt em modo escrita ("w") — apaga conteúdo anterior
 *   5. Loop: reescrever cada linha
 *   6. Se encontrar linha com target E status=PENDING:
 *      - Escrever: "username:password:ROLE:ACTIVE" (mudando PENDING→ACTIVE)
 *      - Marcar como encontrado
 *   7. Outras linhas: reescrever como estavam
 *   8. Fechar ficheiro
 *   9. Se encontrou e modificou, devolver sucesso
 *   10. Se não encontrou, devolver: "Utilizador não encontrado ou já activo"
 *
 * Nota: Esta operação reescreve o ficheiro inteiro (modelo sequencial,
 *       não é eficiente para muitos utilizadores, mas é simples e adequado
 *       para sistemas pequenos).
 *
 * ============================================================================
 */
void approve_user(const char* admin_user, const char* target, char* response) {
    /* Verificar se quem aprova tem permissões admin */
    if (!is_admin(admin_user)) {
        strcpy(response, "APPROVE_FAIL: Sem permissoes de administrador.");
        return;
    }

    FILE* f = fopen(USERS_FILE, "r");
    if (!f) {
        strcpy(response, "ERRO: Ficheiro de utilizadores nao encontrado.");
        return;
    }

    /* Ler todo o ficheiro para memória (máximo 100 utilizadores) */
    char lines[100][256];
    int count = 0;
    int found = 0;

    while (fgets(lines[count], sizeof(lines[count]), f) && count < 100) {
        count++;
    }
    fclose(f);

    /* Reescrever o ficheiro com status actualizado */
    f = fopen(USERS_FILE, "w"); /* Modo "w" apaga o ficheiro anterior */
    if (!f) {
        strcpy(response, "ERRO: Nao foi possivel actualizar ficheiro.");
        return;
    }

    for (int i = 0; i < count; i++) {
        char u[50], p[50], r[20], s[20];

        if (sscanf(lines[i], "%49[^:]:%49[^:]:%19[^:]:%19s", u, p, r, s) == 4) {
            /* Se é o utilizador a aprovar E está PENDING */
            if (strcmp(u, target) == 0 && strcmp(s, "PENDING") == 0) {
                /* Reescrever com ACTIVE em vez de PENDING */
                fprintf(f, "%s:%s:%s:ACTIVE\n", u, p, r);
                found = 1;
            } else {
                /* Reescrever linha como estava */
                fprintf(f, "%s\n", lines[i]);
            }
        } else {
            /* Linha mal formada — preservar como estava */
            fprintf(f, "%s\n", lines[i]);
        }
    }
    fclose(f);

    if (found)
        sprintf(response,
                "APPROVE_OK: Utilizador '%s' aprovado e agora pode autenticar.",
                target);
    else
        sprintf(
            response,
            "APPROVE_FAIL: Utilizador '%s' nao encontrado ou ja esta activo.",
            target);
}

/* ============================================================================
 * FUNÇÃO: delete_user()
 * ============================================================================
 *
 * O que esta função faz:
 *   Um administrador remove/deleta um utilizador do sistema.
 *   O utilizador é removido do users.txt e não consegue mais fazer login.
 *
 * Para quê é importante:
 *   Permite limpeza do sistema e remoção de contas indesejadas.
 *   Fornecida apenas a admin para proteger integridade das contas.
 *
 * Parâmetros:
 *   - admin_user: nome do admin que remove
 *   - target: nome do utilizador a remover
 *   - response: buffer para devolver resultado
 *
 * Protecções:
 *   - Verifica se admin_user é realmente admin
 *   - Impede que admin se delete a si próprio
 *
 * Como funciona passo a passo:
 *   1. Verificar permissões admin
 *   2. Verificar se target não é admin (protecção)
 *   3. Ler todo users.txt para memória
 *   4. Reabrir users.txt em modo "w" (escrever)
 *   5. Loop: reescrever linhas, EXCEPTO a linha do target
 *   6. Se encontrou e removeu, devolver sucesso
 *   7. Se não encontrou, devolver "Utilizador não encontrado"
 *
 * Nota: A remoção é permanente. O utilizador perde acesso e todas as
 *       suas credenciais são eliminadas.
 *
 * ============================================================================
 */
void delete_user(const char* admin_user, const char* target, char* response) {
    /* Verificar se quem remove tem permissões admin */
    if (!is_admin(admin_user)) {
        strcpy(response, "DELETE_FAIL: Sem permissoes de administrador.");
        return;
    }

    /* Protecção: admin não se pode remover a si próprio */
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

    /* Ler todo o ficheiro para memória */
    char lines[100][256];
    int count = 0, found = 0;

    while (fgets(lines[count], sizeof(lines[count]), f) && count < 100) {
        lines[count][strcspn(lines[count], "\n")] = 0;
        count++;
    }
    fclose(f);

    /* Reescrever ficheiro SEM a linha do utilizador a remover */
    f = fopen(USERS_FILE, "w");
    if (!f) {
        strcpy(response, "ERRO: Nao foi possivel actualizar ficheiro.");
        return;
    }

    for (int i = 0; i < count; i++) {
        char u[50];

        if (sscanf(lines[i], "%49[^:]", u) == 1 && strcmp(u, target) == 0) {
            /* Saltar esta linha — é o utilizador a remover */
            found = 1;
        } else {
            /* Reescrever linha (não é a linha a remover) */
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

/* ============================================================================
 * FUNÇÃO PRINCIPAL: main()
 * ============================================================================
 *
 * O que esta função faz:
 *   Motor principal do servidor. Inicializa socket TCP, escuta ligações,
 *   aceita clientes e processa comandos em modo sequencial/bloqueante.
 *
 * Para quê é importante:
 *   Ponto de entrada do programa. Controla todo o ciclo de vida do servidor.
 *
 * Modelo de funcionamento (Etapa 2 — sequencial/bloqueante):
 *   1. Criar socket TCP
 *   2. Associar ao porto 10000 (bind)
 *   3. Escutar para ligações (listen)
 *   4. Loop infinito:
 *      a) Aceitar cliente (accept) — BLOQUEIA até ter ligação
 *      b) Ler comando do cliente (read)
 *      c) Processar comando (IF/ELSE para cada tipo)
 *      d) Enviar resposta (write)
 *      e) Fechar ligação (close)
 *      f) Voltar ao passo a) para aceitar próximo cliente
 *
 * Notas sobre eficiência:
 *   - Modelo sequencial: só trata um cliente de cada vez
 *   - Se tiver 2 clientes simultâneos, 1 fica em espera
 *   - Adequado para Etapa 2 (sistema pequeno)
 *   - Para produção, usar threads ou async (Etapa 3+)
 *
 * Comandos suportados (processados no loop):
 *   - AUTH, GET_INFO, ECHO, LIST_ALL, CHECK_INBOX
 *   - SEND_MSG, REGISTER, APPROVE_USER, DELETE_USER
 *   - Qualquer outro comando → resposta "CMD_INVALID"
 *
 * ============================================================================
 */
int main() {
    int fd, client;          /* fd=socket servidor, client=socket cliente */
    struct sockaddr_in addr; /* Estrutura para endereço do servidor */
    char buffer[BUF_SIZE];   /* Buffer para dados recebidos */

    start_time = time(NULL); /* Registar momento de inicialização */

    /* PASSO 1: Criar socket TCP
     * AF_INET        = IPv4
     * SOCK_STREAM    = TCP (conexão garantida)
     * 0              = protocolo por defeito
     */
    fd = socket(AF_INET, SOCK_STREAM, 0);

    /* SO_REUSEADDR permite reutilizar porta após restart (evita "Address
     * already in use") */
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* PASSO 2: Preparar estrutura de endereço
     * - Família: IPv4 (AF_INET)
     * - Endereço: qualquer interface (INADDR_ANY = 0.0.0.0)
     * - Porto: 10000 (convertido para big-endian com htons)
     */
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SERVER_PORT);

    /* PASSO 3: Associar socket ao porto (bind)
     * Se falhar (retorna < 0), sair com erro
     */
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Erro ao abrir porto");
        exit(-1);
    }

    /* PASSO 4: Colocar socket em modo escuta (listen)
     * 5 = número máximo de ligações pendentes na fila
     */
    listen(fd, 5);

    /* Desenhar interface do servidor */
    desenhar_cabecalho_servidor();
    guardar_log("Servidor v2.0 (Etapa 2) iniciado e a escuta.", 1);

    /* PASSO 5: LOOP PRINCIPAL — aceitar e processar clientes */
    while (1) {
        struct sockaddr_in cli_addr; /* Endereço do cliente */
        socklen_t size = sizeof(cli_addr);

        /* BLOQUEIA até receber ligação de cliente */
        client = accept(fd, (struct sockaddr*)&cli_addr, &size);

        if (client > 0) {
            memset(buffer, 0, BUF_SIZE); /* Limpar buffer */

            /* Ler dados enviados pelo cliente */
            if (read(client, buffer, BUF_SIZE - 1) > 0) {
                /* Remover newline do final do buffer */
                size_t len = strlen(buffer);
                if (len > 0 && buffer[len - 1] == '\n') {
                    buffer[len - 1] = '\0';
                }

                char response[BUF_SIZE] = ""; /* Preparar resposta */
                char log_msg[BUF_SIZE] = "";  /* Preparar mensagem de log */
                int log_type = 0; /* Tipo de log: 1=OK, 3=ERRO, 0=INFO */

                /* ================================================
                 * F3 — AUTH <user> <pass>
                 * Autenticar utilizador
                 * ================================================ */
                if (strncmp(buffer, "AUTH ", 5) == 0) {
                    char u[50], p[50], r[20];
                    sscanf(buffer + 5, "%49s %49s", u,
                           p);                        /* Extrair user e pass */
                    int result = check_auth(u, p, r); /* Validar */

                    if (result == 1) {
                        /* Sucesso */
                        sprintf(response, "AUTH_SUCCESS:%s", r);
                        sprintf(log_msg, "Login OK: '%s' (%s)", u, r);
                        log_type = 1;
                    } else if (result == -1) {
                        /* Utilizador PENDING */
                        strcpy(response, "AUTH_PENDING");
                        sprintf(log_msg, "Login bloqueado (PENDING): '%s'", u);
                        log_type = 3;
                    } else {
                        /* Credenciais inválidas */
                        strcpy(response, "AUTH_FAIL");
                        sprintf(log_msg, "Login FALHOU: '%s'", u);
                        log_type = 3;
                    }
                }

                /* ================================================
                 * F4 — GET_INFO
                 * Devolver informação do servidor
                 * ================================================ */
                else if (strcmp(buffer, "GET_INFO") == 0) {
                    int up = (int)difftime(time(NULL),
                                           start_time); /* Uptime em segundos */
                    sprintf(response,
                            "C-Cord Server v%s | Uptime: %02dh:%02dm:%02ds",
                            VERSAO_SERVIDOR, up / 3600, (up % 3600) / 60,
                            up % 60);
                    sprintf(log_msg, "GET_INFO processado");
                    log_type = 0;
                }

                /* ================================================
                 * F4 — ECHO <msg>
                 * Ecoar mensagem (teste de latência)
                 * ================================================ */
                else if (strncmp(buffer, "ECHO ", 5) == 0) {
                    sprintf(response, "Servidor Ecoa: %s", buffer + 5);
                    sprintf(log_msg, "ECHO: '%s'", buffer + 5);
                    log_type = 0;
                }

                /* ================================================
                 * F5 — LIST_ALL
                 * Listar todos os utilizadores
                 * ================================================ */
                else if (strcmp(buffer, "LIST_ALL") == 0) {
                    list_all(response);
                    sprintf(log_msg, "LIST_ALL executado");
                    log_type = 0;
                }

                /* ================================================
                 * F5 — CHECK_INBOX <user>
                 * Verificar caixa de entrada
                 * ================================================ */
                else if (strncmp(buffer, "CHECK_INBOX ", 12) == 0) {
                    char user[50];
                    sscanf(buffer + 12, "%49s", user);
                    check_inbox(user, response);
                    sprintf(log_msg, "CHECK_INBOX: '%s'", user);
                    log_type = 0;
                }

                /* ================================================
                 * F5 — SEND_MSG <dest> <from> <msg>
                 * Enviar mensagem
                 * ================================================ */
                else if (strncmp(buffer, "SEND_MSG ", 9) == 0) {
                    char dest[50], from[50], msg[400];
                    sscanf(buffer + 9, "%49s %49s %399[^\n]", dest, from, msg);
                    send_msg(dest, from, msg, response);
                    sprintf(log_msg, "SEND_MSG: de '%s' para '%s'", from, dest);
                    log_type = 1;
                }

                /* ================================================
                 * F6 — REGISTER <user> <pass>
                 * Registar novo utilizador (PENDING)
                 * ================================================ */
                else if (strncmp(buffer, "REGISTER ", 9) == 0) {
                    char u[50], p[50];
                    sscanf(buffer + 9, "%49s %49s", u, p);
                    register_user(u, p, response);
                    sprintf(log_msg, "REGISTER: tentativa para '%s'", u);
                    log_type = 1;
                }

                /* ================================================
                 * F7 — APPROVE_USER <admin> <user>
                 * Admin aprova utilizador PENDING
                 * ================================================ */
                else if (strncmp(buffer, "APPROVE_USER ", 13) == 0) {
                    char admin[50], target[50];
                    sscanf(buffer + 13, "%49s %49s", admin, target);
                    approve_user(admin, target, response);
                    sprintf(log_msg, "APPROVE_USER: '%s' por '%s'", target,
                            admin);
                    log_type = 1;
                }

                /* ================================================
                 * F8 — DELETE_USER <admin> <user>
                 * Admin remove utilizador
                 * ================================================ */
                else if (strncmp(buffer, "DELETE_USER ", 12) == 0) {
                    char admin[50], target[50];
                    sscanf(buffer + 12, "%49s %49s", admin, target);
                    delete_user(admin, target, response);
                    sprintf(log_msg, "DELETE_USER: '%s' por '%s'", target,
                            admin);
                    log_type = 1;
                }

                /* ================================================
                 * COMANDO DESCONHECIDO
                 * ================================================ */
                else {
                    strcpy(response, "CMD_INVALID");
                    strcpy(log_msg, "Comando desconhecido recebido");
                    log_type = 3;
                }

                /* Registar a operação e enviar resposta */
                guardar_log(log_msg, log_type);
                write(client, response, strlen(response));
            }

            /* Fechar ligação com cliente (modelo sequencial) */
            close(client);
        }
    }
    return 0;
}
