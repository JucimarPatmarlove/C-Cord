# C-CORD v3.0 — Documentação Técnica Completa

**Versão:** 3.0 (Etapa 3 Completamente Revista)  
**Data:** Atualizado  
**Linguagem:** Português Europeu (pt_PT)

---

## 📑 Índice

1. [Arquitectura Geral](#arquitectura-geral)
2. [Estrutura de Código](#estrutura-de-código)
3. [Protocolo TCP](#protocolo-tcp)
4. [Funções Principais](#funções-principais)
5. [Fluxos de Utilizador](#fluxos-de-utilizador)
6. [Base de Dados](#base-de-dados)
7. [Tratamento de Erros](#tratamento-de-erros)
8. [Testes e Validação](#testes-e-validação)

---

## Arquitectura Geral

### Componentes do Sistema

```
┌──────────────────────────────────────────────────────┐
│                   CLIENTE (client_linux.c)           │
├──────────────────────────────────────────────────────┤
│  Responsável por:                                    │
│  • Rendering da TUI (Terminal User Interface)       │
│  • Captura de input do utilizador                   │
│  • Comunicação TCP com servidor                     │
│  • Gestão de menus hierárquicos                      │
│  • Apresentação de respostas coloridas              │
└──────────────────────────────────────────────────────┘
                        ↕
              Ligação TCP Persistente
                   Porto 10000
                        ↕
┌──────────────────────────────────────────────────────┐
│                   SERVIDOR (server_linux.c)          │
├──────────────────────────────────────────────────────┤
│  Responsável por:                                    │
│  • Escuta de conexões TCP                           │
│  • Processamento de comandos                        │
│  • Gestão de canais e utilizadores                  │
│  • Broadcast de mensagens                           │
│  • Persistência de dados (users.txt)                │
│  • Multiplexing com select()                        │
└──────────────────────────────────────────────────────┘
```

### Fluxo de Dados

```
Utilizador digita input
         ↓
Cliente captura (scanf/fgets)
         ↓
Valida input
         ↓
Envia comando TCP ao servidor
         ↓
Servidor recebe e processa
         ↓
Servidor envia resposta
         ↓
Cliente recebe resposta
         ↓
Cliente imprime com cores ANSI
         ↓
Utilizador vê resultado
```

---

## Estrutura de Código

### Ficheiro: client_linux.c (1,185 linhas)

#### Secções Principais

##### 1. **Includes e Definições**

```c
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#define BUF_SIZE 4096
#define STDIN_FILENO 0
```

- `arpa/inet.h` — Funções de rede (htons, inet_aton)
- `netdb.h` — DNS resolution (gethostbyname)
- `sys/socket.h` — Sockets TCP/IP
- `sys/select.h` — Multiplexing (select())
- `unistd.h` — POSIX utilities (close, sleep)

##### 2. **Variáveis Globais (Estado da Sessão)**

```c
char   current_user[50]      /* Utilizador autenticado */
int    is_admin_flag          /* 0=USER, 1=ADMIN */
int    autenticado            /* 0=GUEST, 1=AUTENTICADO */
char   current_canal[50]     /* Canal actual (#geral, etc) */
time_t login_time             /* Timestamp do login */
int    server_fd              /* File descriptor do socket */
```

##### 3. **Utilitários**

```c
void clear_buffer()           /* Limpar stdin após scanf */
void aguardar_enter()         /* Pausar até ENTER */
void draw_header(int modo, const char *subtitulo)
                              /* Renderizar cabeçalho com logo */
```

##### 4. **Comunicação TCP**

```c
int enviar_e_receber(const char *cmd, char *resp, int resp_sz)
/* Enviar comando bloqueante e aguardar resposta */

void imprimir_resposta(const char *buffer)
/* Imprimir resposta linha por linha com cores */
```

##### 5. **Fluxos de Autenticação**

```c
int fluxo_login()             /* Menu login interactivo */
int fluxo_registo()           /* Menu registo com validação */
```

##### 6. **Menus**

```c
void menu_pre_login()         /* Menu GUEST (antes login) */
void menu_user()              /* Menu utilizador normal */
void menu_admin()             /* Menu administrador */
```

##### 7. **Submenus (6 funções)**

```c
void submenu_perfil()              /* Dados da conta */
void submenu_contactos()           /* Lista ONLINE/OFFLINE */
void submenu_mensagens()           /* Caixa de entrada */
void submenu_canais()              /* Chat tempo real */
void submenu_gestao_utilizadores() /* Admin: aprovar/rejeitar */
void submenu_gestao_canais()       /* Admin: criar/remover */
void submenu_seguranca()           /* Admin: logs/auditoria */
```

##### 8. **Função Main**

```c
int main(int argc, char *argv[])
/* Ponto de entrada:
   1. Validar argumentos
   2. Resolver hostname com gethostbyname()
   3. Conectar ao servidor
   4. Iniciar TUI (menu_pre_login)
   5. Despachar para menu_user() ou menu_admin()
*/
```

### Ficheiro: server_linux.c (1,128 linhas)

#### Secções Principais

##### 1. **Handlers de Comando**

```c
void handle_auth()            /* AUTH username password */
void handle_register()        /* REGISTER username password */
void handle_join()            /* JOIN #canal */
void handle_broadcast()       /* BROADCAST #canal message */
void handle_leave()           /* LEAVE */
void handle_list_all()        /* LIST_ALL (tabela utilizadores) */
void handle_list_channels()   /* LIST_CHANNELS (canais + contagem) */
void handle_list_pending()    /* LIST_PENDING (contas aguardando) */
void handle_approve()         /* APPROVE username */
void handle_reject()          /* REJECT username */
void handle_ban()             /* BAN username */
```

##### 2. **Multiplexing**

```c
select(maxfd+1, &readfds, NULL, NULL, NULL)
/* Monitorizar:
   - accept() para novas conexões
   - stdin para input
   - sockets de clientes para dados
*/
```

##### 3. **Armazenamento**

```c
usuarios_activos[MAX_CLIENTES]  /* Array de clientes conectados */
channels_database[]             /* Canais pré-definidos */
```

---

## Protocolo TCP

### Especificação de Mensagens

#### Formato Geral

```
[COMANDO] [ARG1] [ARG2] ... [ARGN]
```

#### Exemplos de Comunicação

##### LOGIN Bem-sucedido

```
CLIENT: AUTH admin admin123
SERVER: AUTH_SUCCESS:ADMIN
    ou: AUTH_SUCCESS:USER
```

##### LOGIN com Falha

```
CLIENT: AUTH admin wrongpass
SERVER: AUTH_FAIL

CLIENT: AUTH admin admin123
SERVER: AUTH_PENDING       /* Conta aguardando aprovação */

CLIENT: AUTH admin admin123
SERVER: AUTH_INACTIVE      /* Conta suspensa/banida */
```

##### BROADCAST em Canal

```
CLIENT: BROADCAST #geral Olá pessoal!
SERVER: BCAST_SENT
         (todos no canal recebem):
         [#geral] client: Olá pessoal!
```

##### LIST_ALL (Multi-linha)

```
CLIENT: LIST_ALL
SERVER: === UTILIZADORES REGISTADOS ===
        ID | Utilizador | Função | Estado
        1  | admin      | ADMIN  | ACTIVE
        2  | user1      | USER   | ACTIVE
        3  | user2      | USER   | PENDING
        Total: 3 utilizadores.
```

---

## Funções Principais

### Cliente

#### draw_header(int modo, const char \*subtitulo)

**Propósito:** Renderizar cabeçalho visual com logo ASCII

**Parâmetros:**

- `modo` — 0=GUEST, 1=USER, 2=ADMIN
- `subtitulo` — Título adicional do ecrã

**Comportamento:**

```
system("clear")                    /* Limpar ecrã */
printf("\033[1;31m")               /* Cor conforme modo */
[Logo ASCII de 6 linhas]
printf("\033[0m")                  /* Reset cor */
[Informações de sessão]
```

**Cores ANSI:**

- GUEST: `\033[1;37m` (Branco)
- USER: `\033[1;36m` (Ciano)
- ADMIN: `\033[1;31m` (Vermelho)

---

#### enviar_e_receber(const char *cmd, char *resp, int resp_sz)

**Propósito:** Enviar comando bloqueante e aguardar resposta

**Parâmetros:**

- `cmd` — Comando a enviar (ex: "AUTH admin admin123")
- `resp` — Buffer para resposta
- `resp_sz` — Tamanho do buffer

**Retorno:**

- Número de bytes recebidos (>0 sucesso)
- ≤0 em caso de erro

**Implementação:**

```c
send(server_fd, cmd, strlen(cmd), 0)     /* Enviar */
recv(server_fd, resp, resp_sz-1, 0)      /* Receber */
resp[n] = '\0'                           /* Null-terminar */
```

---

#### fluxo_login()

**Propósito:** Fluxo completo de autenticação interactivo

**Passos:**

```
1. Draw header GUEST
2. Input: utilizador + password
3. Enviar AUTH username password
4. Analisar resposta:
   - AUTH_SUCCESS:ADMIN → is_admin_flag=1, autenticado=1
   - AUTH_SUCCESS:USER → is_admin_flag=0, autenticado=1
   - AUTH_FAIL → Voltar ao passo 2
   - AUTH_PENDING → Mostrar mensagem de espera
   - AUTH_INACTIVE → Mostrar mensagem de bloqueio
5. Return 1 (sucesso) ou 0 (cancelado)
```

**Retorno:**

- 1 — Login bem-sucedido
- 0 — Cancelado ou falhou

---

#### submenu_canais()

**Propósito:** Interface de chat em tempo real

**Funcionalidade:**

```
1. Mostrar lista de canais (#geral, #linux, #ajuda, personalizado)
2. Utilizador escolhe canal
3. Enviar JOIN #canal
4. Loop de chat:
   - Mostrar prompt: [username] Sua mensagem:
   - Capturar input
   - Se "/quit" → sair
   - Senão → BROADCAST #canal mensagem
   - Mostrar resposta (BCAST_SENT ou erro)
5. Enviar LEAVE ao sair
```

**Nota especial:** Este é o único menu que permite **input multi-linhas** (fgets em vez de scanf).

---

### Servidor

#### handle_broadcast()

**Propósito:** Enviar mensagem para todos no canal

**Pseudocódigo:**

```c
1. Extrair #canal e mensagem do comando
2. Para cada cliente conectado:
   - Se está no canal:
     - Enviar "[#canal] username: mensagem"
3. Enviar "BCAST_SENT" ao cliente origem
```

---

#### handle_list_channels()

**Propósito:** Retornar lista de canais com utilizadores

**Comportamento:**

```
Para cada canal:
  - Contar utilizadores presentes
  - Listar nomes (separados por vírgula)
  - Formato: "  #canal (3): user1, user2, user3"
```

**Exemplo de Resposta:**

```
=== CANAIS ACTIVOS ===
  #geral (2): admin, user1
  #linux (1): user2
  #ajuda (0): (vazio)
Fim da lista de canais.
```

---

## Fluxos de Utilizador

### Fluxo 1: Novo Utilizador

```
START
  ↓
Menu Pré-Login
  ↓
[2] Registar Utilizador
  ↓
Inserir username + password (2x)
  ↓
Validar passwords coincidem
  ↓
Enviar REGISTER username password
  ↓
Servidor valida (username único?)
  ↓
REGISTER_OK → "Conta em PENDING, aguarde aprovação"
            → Voltar Menu Pré-Login
ou
REGISTER_FAIL → "Username já existe" → Sugestões → Retry
  ↓
END
```

### Fluxo 2: Admin Aprova Conta

```
START
  ↓
Login ADMIN (admin/admin123)
  ↓
Menu Admin → [5] Gestão Utilizadores
  ↓
LIST_PENDING → Mostrar contas aguardando
  ↓
[1] Aprovar nova conta
  ↓
Inserir username
  ↓
Enviar APPROVE username
  ↓
Servidor muda estado: PENDING → ACTIVE
  ↓
"APPROVE_OK" → Novo utilizador pode fazer login
  ↓
END
```

### Fluxo 3: Chat em Tempo Real

```
START
  ↓
Menu USER → [4] Chat em Canais
  ↓
Escolher #geral
  ↓
Enviar JOIN #geral
  ↓
"JOIN_OK" → Entrar loop de chat
  ↓
LOOP:
  [username] Sua mensagem: <input>
  ↓
  Se "/quit":
    Enviar LEAVE
    Sair do loop
  Senão:
    Enviar BROADCAST #geral <mensagem>
    Mostrar "BCAST_SENT"
  ↓
END
```

---

## Base de Dados

### users.txt

**Formato:**

```
ID:Utilizador:Password:Função:Estado
```

**Exemplo:**

```
1:admin:admin123:ADMIN:ACTIVE
2:user1:pass1:USER:ACTIVE
3:user2:pass2:USER:PENDING
4:user3:pass3:USER:INACTIVE
5:user4:pass4:USER:BANNED
```

**Campos:**

| Campo      | Valores Possíveis                 | Descrição                                |
| ---------- | --------------------------------- | ---------------------------------------- |
| ID         | Inteiro único                     | Identificador de utilizador              |
| Utilizador | String                            | Nome de login (único)                    |
| Password   | String                            | Senha (plaintext Etapa 3; Etapa 4: hash) |
| Função     | USER, ADMIN                       | Nível de permissões                      |
| Estado     | ACTIVE, PENDING, INACTIVE, BANNED | Situação da conta                        |

---

## Tratamento de Erros

### Erros Comuns e Resoluções

| Erro                                         | Causa Provável               | Solução                         |
| -------------------------------------------- | ---------------------------- | ------------------------------- |
| "ERRO: Não conseguiu ligar-se ao servidor"   | Servidor offline             | `./server_linux` em Terminal 1  |
| "ERRO: Não foi possível resolver 'hostname'" | DNS falha                    | Usar IP directo (127.0.0.1)     |
| "AUTH_FAIL"                                  | Credenciais incorrectas      | Verificar users.txt             |
| "Opção inválida"                             | Input fora do intervalo      | Digitar número entre 0 e máximo |
| "Buffer overflow"                            | Compilação sem -Wall -Wextra | Recompilar com flags            |

### Validações de Input

**scanf() retorno:**

```c
if (scanf("%49s", buffer) != 1) {
    clear_buffer();
    continue;  /* Retry */
}
```

**Buffer limits:**

```c
snprintf(buffer, sizeof(buffer), "%.50s", input);  /* Max 50 chars */
```

---

## Testes e Validação

### Teste de Compilação

```bash
gcc -Wall -Wextra -o client_linux client_linux.c -lm
```

**Verificar:**

- Sem warnings (0 mencionados)
- Sem erros (compilation bem-sucedida)

### Teste Funcional

#### Teste 1: Login

```bash
(echo "1"; echo "admin"; echo "admin123"; sleep 1; echo "0") | \
  ./client_linux 127.0.0.1 10000
```

Esperado: Menu USER ou ADMIN apresentado

#### Teste 2: List All

```bash
(echo "1"; echo "admin"; echo "admin123"; sleep 1; echo "2"; \
  sleep 1; echo "0"; sleep 1; echo "0") | \
  ./client_linux 127.0.0.1 10000
```

Esperado: Tabela de utilizadores com múltiplas linhas

#### Teste 3: Chat

```bash
(echo "1"; echo "admin"; echo "admin123"; sleep 1; echo "4"; \
  echo "1"; echo "test"; echo "/quit") | \
  ./client_linux 127.0.0.1 10000
```

Esperado: Mensagem de broadcast enviada com sucesso

---

## Conclusões e Notas

### Implementação Bem-sucedida ✅

- ✅ TUI com 3 modos visuais
- ✅ 13 menus + 6 submenus
- ✅ Chat em tempo real (select())
- ✅ Ligação persistente
- ✅ 0 warnings de compilação
- ✅ Documentação 100% pt_PT
- ✅ **Código Totalmente Revisto** e estabilizado (ver `ETAPA3_EXPLICACAO.md`)

### Limitações (Etapa 4)

- ⏳ Sem encriptação (plaintext)
- ⏳ Sem 2FA
- ⏳ Sem histórico persistente
- ⏳ Sem autenticação TLS

---

**Versão:** 3.0 | **Data:** 2026-05-25 | **Linguagem:** Português Europeu
