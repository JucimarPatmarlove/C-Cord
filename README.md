# C-CORD — Simplified Discord Clone in C

**Versão:** 3.0 (Etapa 3 — Select + TUI + Canais)  
**Status:** ✅ Etapa 3 Completa  
**Data:** 2026-06-02

---

## 📋 Descrição Geral

**C-Cord** é um clone simplificado do Discord implementado em C puro (POSIX), com suporte a:

- ✅ **Autenticação e Registo** com credenciais (AUTH, REGISTER)
- ✅ **Canais** de chat em tempo real (#geral, #linux, #ajuda, +personalizados)
- ✅ **Mensagens Privadas** entre utilizadores
- ✅ **TUI Completa** com 3 modos visuais (GUEST, USER, ADMIN)
- ✅ **Gestão Administrativa** (aprovar/rejeitar contas, criar canais, auditoria)
- ✅ **Chat em Tempo Real** com `select()` multiplexing (stdin + socket simultâneos)
- ✅ **Ligação Persistente** durante toda a sessão

---

## 🚀 Quick Start

### Compilação

```bash
gcc -Wall -Wextra -o server_linux server_linux.c
gcc -Wall -Wextra -o client_linux client_linux.c
```

**Resultado:** 0 warnings, 0 errors ✅

### Execução

**Terminal 1 — Servidor:**
```bash
./server_linux
```

**Terminal 2 — Cliente:**
```bash
./client_linux 127.0.0.1 10000
```

---

## 🎮 Utilizadores de Teste

| Utilizador | Password  | Função  |
|-----------|-----------|--------|
| admin     | admin123  | ADMIN  |
| user1     | pass1     | USER   |
| user2     | pass2     | USER   |

---

## 🎨 Interface (TUI)

### 3 Modos Visuais

**GUEST** (Branco) — Pré-login
```
BEM-VINDO AO C-CORD (v3.0)
[ 1 ] Iniciar Sessão
[ 2 ] Registar Utilizador
[ 0 ] Terminar Ligação
```

**USER** (Ciano) — Utilizador Normal (5 opções)
```
[~] MODO UTILIZADOR NORMAL
[ 1 ] O Meu Perfil
[ 2 ] Lista de Contactos
[ 3 ] Mensagens Privadas
[ 4 ] Chat em Canais
[ 5 ] Informações do Servidor
[ 0 ] Logout
```

**ADMIN** (Vermelho) — Administrador (8 opções)
```
[!] MODO ADMINISTRADOR ATIVO
[ 1-4 ] Como USER
[ 5 ] Gestão de Utilizadores
[ 6 ] Gestão de Canais
[ 7 ] Segurança e Auditoria
[ 8 ] Informações do Servidor
[ 0 ] Logout
```

---

## 📊 Arquitectura

```
CLIENT (1,704 linhas)           SERVER (1,346 linhas)
├─ draw_header()                ├─ select()
├─ menu_pre_login()             ├─ handle_auth()
├─ menu_user()                  ├─ handle_broadcast()
├─ menu_admin()                 ├─ handle_list_*()
├─ submenu_*() [6 menus]        └─ users.txt (BD simples)
└─ enviar_e_receber()

          ↕ TCP Socket (Porto 10000)
          ↕ Ligação Persistente
```

---

## 🔌 Protocolo TCP

| Comando | Exemplo | Resposta |
|---------|---------|----------|
| AUTH | `AUTH admin admin123` | `AUTH_SUCCESS:ADMIN` |
| REGISTER | `REGISTER user pass` | `REGISTER_OK` |
| JOIN | `JOIN #geral` | `JOIN_OK` |
| BROADCAST | `BROADCAST #geral ola` | `BCAST_SENT` |
| LEAVE | `LEAVE` | `LEAVE_OK` |
| SEND_MSG | `SEND_MSG user msg` | `MSG_SENT` |
| LIST_ALL | `LIST_ALL` | Tabela utilizadores |
| LIST_CHANNELS | `LIST_CHANNELS` | Canais + ocupância |
| GET_INFO | `GET_INFO` | Info servidor |

---

## 📁 Ficheiros

```
/C-Cord/
├── client_linux.c         — Cliente com TUI (1,704 linhas, 0 warnings)
├── server_linux.c         — Servidor com select() (1,346 linhas, 0 warnings)
├── users.txt              — Base de dados (ID:User:Pass:Role:State)
├── README.md              — Este ficheiro
├── DOCUMENTACAO.md        — Guia técnico detalhado
├── EVOLUCAO_ARQUITETURA.md — Overview técnico Etapa 3
└── TESTES.md              — Guia de testes completo
```

---

## ✅ Qualidade de Código

| Métrica | Valor |
|---------|-------|
| Linhas de código (Cliente) | 1,704 |
| Linhas de código (Servidor) | 1,346 |
| **Total** | **3,050** |
| Funções principais | 24 |
| Linhas de comentários | 646+ |
| Compilação | 0 warnings ✅ |
| Testes | 8/8 passing ✅ |
| Documentação | 100% pt_PT ✅ |

---

## 🧪 Testes

### Teste Rápido
```bash
./teste_rapido_etapa3.sh
```
Verifica compilação, binários, estrutura de código.

### Teste Funcional
```bash
./teste_etapa3.sh
```
Testa registo, login, comandos de lista, multi-line parsing.

---

## 📚 Documentação

| Ficheiro | Objetivo |
|----------|----------|
| **DOCUMENTACAO.md** | Guia técnico + arquitetura + protocolo |
| **RESUMO_ETAPA3.md** | Resumo mockups + menus + roadmap |
| **README.md** | Setup + quick start (ESTE FICHEIRO) |

---

## 🔐 Segurança (Etapa 3)

✅ Validação de credenciais  
✅ USER/ADMIN differentiation  
✅ Aprovação de contas (PENDING)  
✅ Buffer overflow fixes (snprintf)

⏳ **Etapa 4:** Encriptação TLS, 2FA, histórico persistente

---

## 🔄 Roadmap

| Etapa | Status | Funcionalidade |
|-------|--------|---|
| 1 | ✅ | Cliente TCP + AUTH + GET_INFO |
| 2 | ✅ | Canais + JOIN/LEAVE/BROADCAST + Mensagens |
| 3 | ✅ | TUI + Menus + Chat tempo real (select) |
| 4 | 📋 | SQLite + 2FA + TLS + Histórico |

---

## 💻 Requisitos

- **OS:** Linux (x86_64)
- **Compilador:** GCC 9.0+
- **Bibliotecas:** libc, libm
- **Porto:** 10000

---

**Versão:** 3.0 | **Data:** 2026-06-02 | **Status:** ✅ Etapa 3 Completa
