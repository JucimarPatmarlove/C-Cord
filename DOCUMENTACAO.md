# C-Cord 🔒

Clone simplificado do Discord em C, desenvolvido no âmbito da unidade curricular de **Administração e Segurança de Sistemas** (Mestrado em Cibersegurança, IPG).

O projecto implementa um servidor TCP multi-cliente com autenticação, mensagens, gestão de utilizadores e, nas etapas finais, comunicação em tempo real e criptografia.

---

## Equipa — Grupo C-Cord

| Nome | Papel |
|---|---|
| David Bunga | Team Manager |
| Ricardo Pereira | Software Manager |
| *(Account Manager)* | Account Manager |
| *(Risk Manager)* | Risk & Testing Manager |
| *(Quality Manager)* | Quality Manager |
| Carlos Martins | Dev Team |
| Ivo Pinela | Dev Team |
| Jucimar Cabral | Dev Team |

---

## Arquitectura

```
┌─────────────┐        TCP (porta 10000)        ┌─────────────┐
│   CLIENTE   │ ──────────────────────────────► │  SERVIDOR   │
│  (cliente)  │ ◄────────────────────────────── │  (server)   │
└─────────────┘                                 └──────┬──────┘
                                                       │
                                              ┌────────┴────────┐
                                              │   users.txt     │
                                              │   inbox.txt     │
                                              │   logs.txt      │
                                              └─────────────────┘
```

**Modelo actual (Etapas 1 e 2):** Bloqueante/Sequencial — o servidor trata um cliente de cada vez.  
**Etapa 3:** Refactor para `select()` — comunicação em tempo real com múltiplos clientes simultâneos.

---

## Protocolo de Comunicação

Todas as mensagens são texto simples enviado via TCP. O servidor responde e fecha a ligação (modelo sequencial).

### Comandos disponíveis

| Comando | Descrição | Resposta |
|---|---|---|
| `AUTH <user> <pass>` | Autenticação | `AUTH_SUCCESS:ADMIN` / `AUTH_SUCCESS:USER` / `AUTH_FAIL` / `AUTH_PENDING` |
| `GET_INFO` | Informações do servidor | Versão + uptime |
| `ECHO <msg>` | Eco de mensagem | `Servidor Ecoa: <msg>` |
| `LIST_ALL` | Lista todos os utilizadores | Tabela com username, papel e estado |
| `CHECK_INBOX <user>` | Ver mensagens recebidas | Lista de mensagens |
| `SEND_MSG <dest> <from> <msg>` | Enviar mensagem a utilizador | `MSG_SENT` |
| `REGISTER <user> <pass>` | Registar nova conta | `REGISTER_OK` / `REGISTER_FAIL` |
| `APPROVE_USER <admin> <user>` | Aprovar conta pendente (admin) | `APPROVE_OK` / `APPROVE_FAIL` |
| `DELETE_USER <admin> <user>` | Apagar utilizador (admin) | `DELETE_OK` / `DELETE_FAIL` |
| `SUSPEND_USER <admin> <user>` | Suspender utilizador (admin) — v1.1 | `SUSPEND_OK` / `SUSPEND_FAIL` |
| `VIEW_LOGS` | Visualizar logs do servidor (admin) — v1.1 | Conteúdo de logs.txt |

---

## Estrutura de Ficheiros

```
C-Cord/
├── tcp_server.c          # Servidor TCP (Linux/POSIX)
├── tcp_client_linux.c    # Cliente TCP Linux — Jucimar Cabral
├── tcp_client.c          # Cliente TCP Windows (Winsock2) — Ivo Pinela
├── users.txt             # Base de dados de utilizadores
├── inbox.txt             # Mensagens armazenadas (gerado em runtime)
├── logs.txt              # Registo de actividade do servidor (gerado em runtime)
└── README.md             # Este ficheiro
```

### Formato do `users.txt`

```
username:password:ROLE:STATUS
```

| Campo | Valores possíveis |
|---|---|
| `ROLE` | `ADMIN`, `USER` |
| `STATUS` | `ACTIVE`, `PENDING` |

Exemplo:
```
admin:admin123:ADMIN:ACTIVE
user1:pass123:USER:ACTIVE
jucimar:jucimar123:USER:ACTIVE
```

> ⚠️ Em produção nunca guardar passwords em texto simples. Isto é um exercício académico.

---

## Etapa 2 — Versão 1.1: Interface Aprimorada e Comentários pt_PT

### O que foi implementado

✅ **Cliente (client.c):**
- Interface TUI (Terminal User Interface) com 3 modos visuais distintos:
  - **GUEST** (branco): utilizador não autenticado
  - **USER** (ciano): utilizador normal autenticado
  - **ADMIN** (vermelho): utilizador administrativo
- Menu F3-F8 com operações completas:
  - **F3:** Login (AUTH) — suporta ADMIN, USER, PENDING, INACTIVE
  - **F4:** Info/Echo — GET_INFO e ECHO <msg>
  - **F5:** Listar/Mensagens — LIST_ALL e CHECK_INBOX
  - **F6:** Registar — REGISTER <user> <pass> (estado PENDING)
  - **F7:** Gestão Admin — APPROVE_USER, DELETE_USER, SUSPEND_USER, VIEW_LOGS
  - **F8:** Submenu de mensagens — SEND_MSG com validação de destinatário
- Todas as funções comentadas em português europeu (pt_PT)
- Tratamento de erros contextual (e.g., aviso de Caps Lock, sugestões de utilizadores)

✅ **Servidor (server.c):**
- Protocolo expandido (F3-F8):
  - `AUTH <user> <pass>` → responde com `AUTH_SUCCESS:ADMIN/USER`, `AUTH_FAIL`, ou `AUTH_PENDING`
  - `GET_INFO` → versão do servidor e uptime
  - `ECHO <msg>` → ecoa mensagem
  - `LIST_ALL` → tabela formatada com username, ROLE, STATUS
  - `CHECK_INBOX <user>` → lista mensagens para utilizador
  - `SEND_MSG <dest> <from> <msg>` → envia mensagem (novo)
  - `REGISTER <user> <pass>` → cria utilizador com status PENDING
  - `APPROVE_USER <admin> <user>` → admin aprova utilizador
  - `DELETE_USER <admin> <user>` → admin apaga utilizador
  - `SUSPEND_USER <admin> <user>` → admin suspende utilizador (novo)
  - `VIEW_LOGS` → admin visualiza histórico de logs (novo)
- Sistema de logging completo com timestamps:
  - Regista todas as operações em logs.txt
  - Cores ANSI no terminal (OK=verde, ERRO=vermelho, INFO=ciano)
- Gestão de estado de utilizadores:
  - ACTIVE: pode fazer login
  - PENDING: aguarda aprovação de admin
  - INACTIVE: suspenso por admin
- Inbox de mensagens (inbox.txt) com persistência
- Todas as funções com comentários explicativos em pt_PT

### Formato da Base de Dados (v1.1)

#### users.txt (5 campos)
```
ID:username:password:ROLE:STATUS
```

Exemplo:
```
1:admin:admin123:ADMIN:ACTIVE
2:user1:pass123:USER:ACTIVE
3:jucimar:jucimar123:USER:ACTIVE
4:alice:alice123:USER:PENDING
5:bob:bob123:USER:INACTIVE
```

#### inbox.txt (mensagens)
```
recipient:sender:message
```

#### logs.txt (registos de actividade)
```
[2024-01-15 14:30:45] [OK] Login bem-sucedido: user1
[2024-01-15 14:31:12] [ERRO] Tentativa de login falhada: invalid_user
[2024-01-15 14:32:00] [INFO] Utilizador alice aprovado por admin
```

### Mudanças de Código Principais

1. **Cliente:**
   - `draw_header()`: desenha interface com cores conforme modo (GUEST/USER/ADMIN)
   - `call_server()`: estabelece conexão TCP, envia comando, recebe resposta
   - `fluxo_login()`: implementa lógica de autenticação com tratamento de estados
   - `submenu_mensagens()`: permite selecionar destinatário e enviar mensagem
   - `menu_admin()`: submenu exclusivo para administradores

2. **Servidor:**
   - `guardar_log()`: regista operações com timestamp e tipo (OK/INFO/ERRO)
   - `check_auth()`: valida credenciais e retorna ROLE (ADMIN/USER/PENDING/INACTIVE)
   - `is_admin()`: verifica se utilizador tem permissões administrativas
   - `list_all()`: formata resposta com todos os utilizadores
   - `check_inbox()`: retorna mensagens pendentes para utilizador
   - `send_msg()`: armazena mensagem em inbox.txt
   - `register_user()`: cria novo utilizador com status PENDING
   - `approve_user()`: admin aprova utilizador PENDING → ACTIVE
   - `delete_user()`: admin apaga utilizador permanentemente
   - `suspend_user()`: admin suspende utilizador → INACTIVE

### Compilação (v1.1)

```bash
gcc -Wall -Wextra -o client client.c
gcc -Wall -Wextra -o server server.c
```

Warnings esperados (não críticos):
- `sprintf` overflow warning em `SEND_MSG` — buffer suficiente para dados práticos

### Execução

```bash
# Terminal 1: Iniciar servidor
./server

# Terminal 2+: Iniciar cliente(s)
./client 127.0.0.1 10000
./client 192.168.1.100 10000  # outro servidor
```

---

## Compilação e Execução

### Pré-requisitos

- GCC instalado
- Linux (testado em Kali Linux Rolling)

### Compilar

```bash
gcc tcp_server.c -o server
gcc tcp_client_linux.c -o client
```

### Executar

```bash
# Terminal 1 — iniciar servidor
./server

# Terminal 2 — ligar cliente (mesma máquina)
./client 127.0.0.1 10000

# Terminal 2 — ligar cliente (rede local)
./client 192.168.1.133 10000
```

---

## Funcionalidades por Etapa

### ✅ Etapa 1 — Servidor Sequencial (F1–F4)
- **F1** — Mockups e diagramas do sistema
- **F2** — Relatório consolidado
- **F3** — Autenticação por username/password com ficheiro local
- **F4** — Comandos `GET_INFO` e `ECHO`

**Entrega:** 16/05/2026

### ✅ Etapa 2 — Interacção por Comandos (F5–F8)
- **F5** — `LIST_ALL`, `CHECK_INBOX`, `SEND_MSG`
- **F6** — `REGISTER` (conta criada com estado PENDING)
- **F7** — `APPROVE_USER` (admin activa conta PENDING)
- **F8** — `DELETE_USER` (admin remove utilizador)

**Entrega:** 29/05/2026

### 🔲 Etapa 3 — Tempo Real com select() (F9–F10)
- **F9** — Broadcast em tempo real para todos os clientes do canal
- **F10** — Suporte a múltiplos canais (`/join #canal`)

**Entrega:** 12/06/2026

### 🔲 Etapa 4 — Criptografia (F11–F15)
- **F11** — Cifra de César generalizada (chave simétrica hardcoded)
- **F12** — Troca de chave via Diffie-Hellman
- **F13** — Cifras simétricas, assimétrica (RSA toy), hashes de integridade
- **F14** — Consulta de parâmetros criptográficos pelo admin
- **F15** — Extras (interfaces gráficas, etc.)

**Entrega:** 05/07/2026

---

## Contribuições Individuais

### Etapa 1
| Ficheiro | Autor | Descrição |
|---|---|---|
| `tcp_server.c` | Ivo Pinela | Servidor bloqueante com F3 e F4 |
| `tcp_client.c` | Ivo Pinela | Cliente Windows (Winsock2) |
| `tcp_client_linux.c` | Jucimar Cabral | Port POSIX do cliente para Linux/Kali |

### Etapa 2
| Ficheiro | Autor | Descrição |
|---|---|---|
| `tcp_server.c` | Jucimar Cabral | Extensão com F5, F6, F7, F8 |
| `tcp_client_linux.c` | Jucimar Cabral | Menus e comandos F5–F8 no cliente Linux |

---

## Referências

- Beej's Guide to Network Programming — https://beej.us/guide/bgnet/
- POSIX Socket API — `man 7 socket`, `man 2 select`
- MITRE ATT&CK — https://attack.mitre.org
- RFC 793 — Transmission Control Protocol

---

*Instituto Politécnico da Guarda — Mestrado em Cibersegurança — 2025/2026*
