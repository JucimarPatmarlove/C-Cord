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

---

## 📁 Estrutura de Ficheiros

```
C-Cord/
├── server.c          ← Servidor TCP Linux/POSIX
├── client.c          ← Cliente TCP Linux (TUI) — Jucimar Cabral
├── users.txt         ← Base de dados de utilizadores
├── .gitignore        ← Exclui binários, logs e inbox
└── README.md         ← Este ficheiro
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
```

> ⚠️ Passwords em texto claro — exercício académico. Em produção usar bcrypt ou equivalente.

### Formato do `inbox.txt`

```
destinatario:remetente:mensagem
```

### Formato do `logs.txt`

```
[YYYY-MM-DD HH:MM:SS] [OK|INFO|ERRO] | mensagem
```

---

## ⚙️ Compilação e Execução

### Pré-requisitos

- GCC
- Linux (testado em Kali Linux Rolling)

### Compilar

```bash
gcc -o server server.c
gcc -o client client.c
```

### Executar

```bash
# Terminal 1 — servidor
./server

# Terminal 2 — cliente (localhost)
./client 127.0.0.1 10000

# Terminal 2+ — múltiplos clientes (rede)
./client <IP_SERVIDOR> 10000
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

### 🔲 Etapa 3 — Tempo Real com select() (F9–F10) — *12/06/2026*

- **F9** — Broadcast em tempo real (múltiplos clientes simultâneos)
- **F10** — Suporte a canais (`/join #canal`)

### 🔲 Etapa 4 — Criptografia (F11–F15) — *05/07/2026*

- **F11** — Cifra de César generalizada (chave simétrica hardcoded)
- **F12** — Troca de chave via Diffie-Hellman
- **F13** — Cifras simétricas, assimétrica (RSA toy), hashes de integridade
- **F14** — Consulta de parâmetros criptográficos pelo admin
- **F15** — Extras (interfaces gráficas, etc.)

---

## 👤 Contribuições Individuais

### Etapa 1
| Ficheiro | Autor | Descrição |
|---|---|---|
| `server.c` | Ivo Pinela | Servidor bloqueante com F3 e F4 |
| `client.c` (Windows) | Ivo Pinela | Cliente Windows (Winsock2) |
| `client.c` (Linux) | Jucimar Cabral | Port POSIX para Linux/Kali com TUI |

### Etapa 2
| Ficheiro | Autor | Descrição |
|---|---|---|
| `server.c` | Jucimar Cabral | F5–F8 + SUSPEND_USER + VIEW_LOGS + formato 5 campos |
| `client.c` | Jucimar Cabral | TUI completa seguindo mockups da equipa |

---

## 🔗 Referências

- Beej's Guide to Network Programming — https://beej.us/guide/bgnet/
- POSIX Socket API — `man 7 socket`, `man 2 select`
- RFC 793 — Transmission Control Protocol

---

> **Aviso Académico:** Este projeto é um exercício de aprendizagem. Em ambientes de produção devem ser utilizadas bibliotecas auditadas como OpenSSL ou libsodium.

*Instituto Politécnico da Guarda — Mestrado em Cibersegurança — 2025/2026*
