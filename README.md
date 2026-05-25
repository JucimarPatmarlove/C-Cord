# 💬 C-Cord: Sistema de Comunicação Segura em Tempo Real

O **C-Cord** é uma plataforma de comunicação inspirada no Discord, desenvolvida em linguagem **C**. O projeto foca-se na evolução de um servidor sequencial básico para um sistema de alto desempenho com suporte a múltiplos canais e segurança avançada.

---

## 🚀 Visão Geral

O sistema baseia-se numa arquitetura Cliente-Servidor, progredindo através de quatro fases principais:

1. **Fase 1 e 2:** Implementação de um servidor sequencial utilizando *sockets* bloqueantes para autenticação e gestão de mensagens simples.
2. **Fase 3:** Upgrade para tempo real utilizando multiplexagem (`select()` ou `poll()`), permitindo transmissões instantâneas (*broadcast*) e múltiplos canais simultâneos.
3. **Fase 4:** Implementação de segurança robusta para garantir Confidencialidade, Integridade e Autenticidade (CIA).

---

## 👥 Estrutura da Equipa

| Função | Nome | Responsabilidades |
|:---|:---|:---|
| **Team Manager** | Carlos Martins | Coordenação, planeamento (Gantt) e apresentações |
| **Account Manager** | Miguel Rodrigues | Requisitos funcionais/não-funcionais e mockups |
| **Software Manager** | Ricardo Pereira | Arquitetura do sistema e gestão de ferramentas (GitHub) |
| **Risk & Testing Manager** | Diego França | Plano de mitigação de riscos e protocolos de testes |
| **Quality Manager** | Rui Pina | Estado da arte e garantia de qualidade documental |
| **Dev Team** | David Bunga, Jucimar Cabral, Ivo Pinela | Desenvolvimento técnico das funcionalidades (F1–F15) |

**Nota:** Em 18/05/2026, David Bunga e Carlos Martins trocaram de posição:
- **David Bunga** passou a fazer parte do Dev Team (desenvolvimento técnico)
- **Carlos Martins** assumiu o cargo de **Team Manager** (coordenação e gestão)

---

## 📅 Datas Críticas (2026)

| Data | Entrega |
|---|---|
| 14/05 | Constituição dos grupos |
| ✅ 16/05 | Defesa Etapa 1 — F1–F4 |
| 29/05 | Defesa Etapa 2 — F5–F8 |
| 12/06 | Etapa 3 — F9–F10 |
| 05/07 | Etapa Final — F11–F15 |

---

## 🏗️ Arquitectura do Sistema

```
┌─────────────┐        TCP (porta 10000)        ┌─────────────┐
│   CLIENTE   │ ──────────────────────────────► │  SERVIDOR   │
│  (client.c) │ ◄────────────────────────────── │  (server.c) │
└─────────────┘                                 └──────┬──────┘
                                                       │
                                              ┌────────┴────────┐
                                              │   users.txt     │
                                              │   inbox.txt     │
                                              │   logs.txt      │
                                              └─────────────────┘
```

**Etapas 1 e 2:** Modelo bloqueante/sequencial — o servidor trata um cliente de cada vez.
**Etapa 3:** Refactor para `select()` — múltiplos clientes simultâneos e broadcast em tempo real.

---

## 📡 Protocolo de Comunicação

Todas as mensagens são texto simples enviado via TCP. O servidor responde e fecha a ligação (modelo sequencial — Etapas 1 e 2).

| Comando | Parâmetros | Resposta | Etapa |
|---|---|---|---|
| `AUTH` | `<user> <pass>` | `AUTH_SUCCESS:ROLE` / `AUTH_FAIL` / `AUTH_PENDING` / `AUTH_INACTIVE` | 1 — F3 |
| `GET_INFO` | — | Versão + uptime + pedidos | 1 — F4 |
| `ECHO` | `<msg>` | `Servidor Ecoa: <msg>` | 1 — F4 |
| `LIST_ALL` | — | Tabela de utilizadores | 2 — F5 |
| `LIST_PENDING` | — | Utilizadores com estado PENDING | 2 — F5 |
| `CHECK_INBOX` | `<user>` | Mensagens recebidas | 2 — F5 |
| `SEND_MSG` | `<dest> <from> <msg>` | `MSG_SENT` / `MSG_FAIL` | 2 — F5 |
| `REGISTER` | `<user> <pass>` | `REGISTER_OK` / `REGISTER_FAIL` | 2 — F6 |
| `APPROVE_USER` | `<admin> <user>` | `APPROVE_OK` / `APPROVE_FAIL` | 2 — F7 |
| `SUSPEND_USER` | `<admin> <user>` | `SUSPEND_OK` / `SUSPEND_FAIL` | 2 — F7 |
| `DELETE_USER` | `<admin> <user>` | `DELETE_OK` / `DELETE_FAIL` | 2 — F8 |
| `VIEW_LOGS` | `<admin>` | Conteúdo de logs.txt | 2 — F8 |
| `JOIN` | `#<canal>` | `JOIN_OK` / `JOIN_FAIL` | 3 — F9 |
| `LEAVE` | — | `LEAVE_OK` / `LEAVE_FAIL` | 3 — F9 |
| `BROADCAST` | `<msg>` | `BCAST_SENT` / `BCAST_FAIL` | 3 — F10 |
| `LIST_CHANNELS` | — | `CHANNELS: #geral (2), #admin (1), ...` | 3 — F11 |

---

## 📁 Estrutura de Ficheiros

```
C-Cord/
├── server_linux.c      ← Servidor TCP Linux/POSIX (v3.0, Etapa 3 com select())
├── client_linux.c      ← Cliente TCP Linux (TUI) (v2.0, Etapa 3 com select())
├── users.txt           ← Base de dados de utilizadores
├── inbox.txt           ← Mensagens armazenadas (gerado em runtime)
├── logs.txt            ← Registo de atividade do servidor (gerado em runtime)
├── .gitignore          ← Exclui binários, logs e inbox
├── README.md           ← Este ficheiro
├── DOCUMENTACAO.md     ← Documentação técnica detalhada
├── TESTE_RAPIDO.sh     ← Script de teste interactivo
└── MELHORIAS_UX.md     ← Changelog e melhorias de UX
```

### Formato do `users.txt` (v1.1 — 5 campos)

```
ID:username:password:ROLE:STATUS
```

| Campo | Valores |
|---|---|
| `ROLE` | `ADMIN`, `USER` |
| `STATUS` | `ACTIVE`, `PENDING`, `INACTIVE` |

```
1:admin:admin123:ADMIN:ACTIVE
2:user1:pass123:USER:ACTIVE
3:jucimar:jucimar123:USER:ACTIVE
4:alice:alice123:USER:PENDING
5:bob:bob123:USER:INACTIVE
```

> ⚠️ Passwords em texto claro — exercício académico. Em produção usar bcrypt ou equivalente.

---

## 🚀 Etapa 3 — Versão 3.0: Select() Multiplex, Canais e Broadcasts em Tempo Real

### Estado Atual (Atualizado 26/05/2026)

✅ **Desenvolvimento Completo** — Select() implementado em cliente e servidor com broadcasts funcionais.

✨ **Melhorias Implementadas (v3.0):**
- ✅ Comando `LIST_CHANNELS` adicionado (listar canais activos com contadores de utilizadores)
- ✅ Remover atalhos numéricos (2, 9, 10) para evitar ambiguidade com mensagens
- ✅ Compilação sem warnings (servidor: 0 warnings, cliente: 0 warnings)
- ✅ Compatibilidade total com Etapas 1 e 2 (todos os comandos preservados)
- ✅ Ligações persistentes (socket permanece aberto durante sessão)

### 🎯 Teste Crítico — "Dupla Escuta" (Confirma sucesso da Etapa 3)

**O que é:** Cliente deve receber broadcasts em tempo real sem enviar comando.

**Como testar (3 terminais):**

```bash
# Terminal 1 — Servidor
$ ./server_linux
[SERVIDOR] Iniciado na porta 10000
[SERVIDOR] À escuta de conexões...

# Terminal 2 — Cliente A (joao)
$ ./client_linux 127.0.0.1 10000
Input: F3
Username: joao
Password: password123
✅ AUTH_SUCCESS (STATUS: USER - CYAN)

Input: JOIN #geral
✅ [JOIN_OK] Entrou no canal #geral

# Terminal 3 — Cliente B (maria)
$ ./client_linux 127.0.0.1 10000
Input: F3
Username: maria
Password: password456
✅ AUTH_SUCCESS (STATUS: USER - CYAN)

Input: JOIN #geral
✅ [JOIN_OK] Entrou no canal #geral

# De volta ao Terminal 2 — Cliente A ENVIA BROADCAST
Input: BROADCAST Olá pessoal, isto é um teste!
✅ [BCAST_SENT] Mensagem enviada ao canal #geral

# 🎯 VERIFICAR NO TERMINAL 3 — Cliente B:
# Deve aparecer SEM fazer nenhum comando:
# [#geral] joao: Olá pessoal, isto é um teste!

# Listar canais activos (em qualquer terminal)
Input: LIST_CHANNELS
✅ CHANNELS: #geral (2)
```

**Resultado esperado:**
- ✅ Se a mensagem apareça no Terminal 3 → **select() funciona e Etapa 3 está completa!**
- ✅ Se `LIST_CHANNELS` mostrar "2" utilizadores → **Contadores funcionam!**
- ❌ Se NÃO apareça → Problema no recv() em tempo real

### Ficheiros da Etapa 3 (v3.0)

```
C-Cord/
├── server_linux.c         ← Servidor TCP v3.0 (1145 linhas, select + LIST_CHANNELS)
├── client_linux.c         ← Cliente TCP v2.0 (775 linhas, select + sem atalhos numéricos)
├── server_linux           ← Binário compilado (30KB)
├── client_linux           ← Binário compilado (22KB)
├── TESTE_RAPIDO.sh        ← Script de teste interativo
├── users.txt              ← Base de dados de utilizadores
├── inbox.txt              ← Arquivo de mensagens
├── logs.txt               ← Registo de atividade (auditoria)
└── README.md              ← Este ficheiro
```

### Novos Comandos (Etapa 3)

| Comando | Descrição | Resposta | Etapa |
|---|---|---|---|
| `JOIN #canal` | Entrar num canal | `JOIN_OK: Entrou no canal #geral` | 3 — F9 |
| `LEAVE` | Sair do canal | `LEAVE_OK` | 3 — F9 |
| `BROADCAST <msg>` | Enviar msg ao canal | `BCAST_SENT` (a outros: `[#canal] user: msg`) | 3 — F10 |

### Compilação Status ✅ (Etapa 3)

**server_linux.c (v3.0):**
```bash
$ gcc -Wall -Wextra -o server_linux server_linux.c
✓ Compilação limpa (0 erros, 0 warnings)
✓ Select multiplex para até 50 clientes simultâneos
✓ Persistência de ligações TCP por sessão
✓ Logging com timestamps e cores ANSI
```

**client_linux.c (v2.0):**
```bash
$ gcc -Wall -Wextra -o client_linux client_linux.c
✓ Compilação limpa (0 erros, 0 warnings)
✓ Select multiplex no STDIN + socket do servidor
✓ Recepção de broadcasts sem bloqueios
✓ "Dupla escuta" — teclado + rede simultâneos
```

### Novidades da Etapa 3

✅ **Select Multiplex (Cliente):**
- Monitoriza simultaneamente STDIN_FILENO (teclado) e server_fd (socket)
- FD_ZERO(), FD_SET(), FD_ISSET(), select() com timeout de 1s
- Sem bloqueios — aplicação responsiva
- Recebe broadcasts em tempo real enquanto aguarda input

✅ **Select Multiplex (Servidor):**
- Array de até 50 clientes: `struct Cliente clientes[MAX_CLIENTES]`
- Campos: fd, username, canal, autenticado
- Monitoriza: server_fd (nova ligação) + clientes[i].fd (cliente existente)
- Recalcula max_fd dinamicamente cada iteração
- Desconexão apenas quando recv() <= 0

✅ **Persistência de Ligações:**
- **Etapa 2:** accept() → read() → close() (socket fecha após cada comando)
- **Etapa 3:** Socket fica aberto durante toda a sessão
- Múltiplos comandos sobre a mesma ligação TCP
- Estado persistente: username, canal, autenticado

✅ **Broadcasts Segmentados por Canal:**
- Cliente A em #geral → recebe msgs de #geral
- Cliente B em #admin → recebe msgs de #admin
- Isolamento automático (não cruza canais)
- Formatação: `[#geral] username: mensagem`

✅ **Documentação Educativa:**
- 1480+ linhas de comentários em português europeu (pt_PT)
- Explicação de TCP sockets (socket(), bind(), listen(), accept(), connect())
- Explicação de select multiplex (FD_ZERO, FD_SET, FD_ISSET, max_fd+1, timeout)
- Protocolo de autenticação explicado
- Tratamento de broadcasts detalhado
- Rácio comentário: 4.3:1 (cliente), 4.15:1 (servidor)

### Números de Compilação (Etapa 3)

| Métrica | Cliente | Servidor |
|---------|---------|----------|
| Linhas totais | 797 | 1033 |
| Linhas código | ~150 | ~200 |
| Linhas comentários | ~650 | ~830 |
| Tamanho compilado | 22KB | 30KB |
| Warnings | 0 | 0 |
| Rácio comentário | 4.3:1 | 4.15:1 |

---

### Formato do `inbox.txt`

```
destinatario:remetente:mensagem
```

### Formato do `logs.txt`

```
[YYYY-MM-DD HH:MM:SS] [OK|INFO|ERRO] | mensagem
```

---

## Compilação e Execução

### Pré-requisitos

- GCC
- Linux (testado em Kali Linux Rolling)

### Compilar

```bash
gcc -Wall -Wextra -o server_linux server_linux.c
gcc -Wall -Wextra -o client_linux client_linux.c
```

### Executar

```bash
# Terminal 1 — servidor
./server_linux

# Terminal 2 — cliente (localhost)
./client_linux 127.0.0.1 10000

# Terminal 2+ — múltiplos clientes (rede)
./client_linux <IP_SERVIDOR> 10000
```

---

## 📋 Funcionalidades por Etapa

### ✅ Etapa 1 — Servidor Sequencial (F1–F4) — *16/05/2026*

- **F1** — Mockups, diagramas de blocos, fluxogramas
- **F2** — Relatório consolidado (requisitos, riscos, testes, Gantt)
- **F3** — Autenticação simples por username/password (ficheiro local)
- **F4** — Comandos `GET_INFO` e `ECHO`

### ✅ Etapa 2 — Interacção por Comandos (F5–F8) — *29/05/2026*

- **F5** — `LIST_ALL`, `CHECK_INBOX`, `SEND_MSG`
- **F6** — `REGISTER` (conta criada com estado PENDING)
- **F7** — `APPROVE_USER`, `SUSPEND_USER` (admin gere contas)
- **F8** — `DELETE_USER`, `VIEW_LOGS`
- Interface TUI com 3 modos visuais (visitante / utilizador / admin)
- Formato `users.txt` actualizado com campo `STATUS` (ACTIVE / PENDING / INACTIVE)

### 🟡 Etapa 3 — Tempo Real com select() (F9–F10) — *12/06/2026* (EM PROGRESSO)

- **F9** — Broadcast em tempo real (múltiplos clientes simultâneos)
- **F10** — Suporte a canais (`JOIN #canal`)
- **Status:** ✅ Implementação completa + Documentação extensiva em pt_PT
- **Select Multiplex:** ✅ Funcionando (cliente + servidor)
- **Persistência de ligações:** ✅ Implementada
- **Comentários:** ✅ 1480+ linhas (4.3:1 rácio)

### 🔲 Etapa 4 — Criptografia (F11–F15) — *05/07/2026*

- **F11** — Cifra de César generalizada (chave simétrica hardcoded)
- **F12** — Troca de chave via Diffie-Hellman
- **F13** — Cifras simétricas, assimétrica (RSA toy), hashes de integridade
- **F14** — Consulta de parâmetros criptográficos pelo admin
- **F15** — Extras (interfaces gráficas, etc.)

---

## 👤 Contribuições Individuais

### Etapa 1 (Concluída 16/05/2026)
| Ficheiro | Autor | Descrição |
|---|---|---|
| `server_linux.c` | Ivo Pinela | Servidor bloqueante com F3 e F4 |
| `client_linux.c` (Windows) | Ivo Pinela | Cliente Windows (Winsock2) |
| `client_linux.c` (Linux) | Jucimar Cabral | Port POSIX para Linux/Kali com TUI |

### Etapa 2 (Concluída 18/05/2026)
| Ficheiro | Autor(es) | Descrição |
|---|---|---|
| `server_linux.c` v2.1 | Jucimar Cabral | F5–F8, gestão de estados, logging, comentários pt_PT |
| `client_linux.c` v1.1 | Jucimar Cabral | Menus completos, 3 modos TUI, comentários extensivos pt_PT |
| Documentação | Dev Team | Atualização de README.md e DOCUMENTACAO.md |

---

## 🔗 Referências

- Beej's Guide to Network Programming — https://beej.us/guide/bgnet/
- POSIX Socket API — `man 7 socket`, `man 2 select`
- RFC 793 — Transmission Control Protocol

---

> **Aviso Académico:** Este projeto é um exercício de aprendizagem. Em ambientes de produção devem ser utilizadas bibliotecas auditadas como OpenSSL ou libsodium.

*Instituto Politécnico da Guarda — Mestrado em Cibersegurança — 2025/2026*
