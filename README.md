# C-CORD — Simplified Discord Clone in C

**Versão:** 4.0 (Etapa 4 — E2EE + Criptografia)  
**Status:** ✅ Etapa 4 Completa  
**Data:** Atualizado

---

## 📋 Descrição Geral

**C-Cord** é um clone simplificado do Discord implementado em C puro (POSIX), com suporte a:

- ✅ **Autenticação e Registo** Seguro (Hash DJB2 verificado via Toy RSA)
- ✅ **Canais** de chat em tempo real (#geral, #linux, #ajuda, +personalizados)
- ✅ **Mensagens Privadas** entre utilizadores
- ✅ **TUI Completa** com 3 modos visuais (GUEST, USER, ADMIN)
- ✅ **Gestão Administrativa** (aprovar/rejeitar contas, criar canais, auditoria)
- ✅ **Chat em Tempo Real** com `select()` multiplexing (stdin + socket simultâneos)
- ✅ **Ligação Persistente** durante toda a sessão
- ✅ **Segurança Zero-Trust** (E2EE) com Diffie-Hellman e Cifra de César

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

| Utilizador | Password | Função |
| ---------- | -------- | ------ |
| admin      | admin123 | ADMIN  |
| user1      | pass1    | USER   |
| user2      | pass2    | USER   |

---

## 🎨 Interface (TUI)

### 3 Modos Visuais

**GUEST** (Branco) — Pré-login

```
BEM-VINDO AO C-CORD (v4.0)
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
CLIENT (1,825 linhas)           SERVER (1,681 linhas)
├─ draw_header()                ├─ select()
├─ motor_criptografico()        ├─ motor_criptografico()
├─ menu_pre_login()             ├─ handle_auth_hash()
├─ menu_user()                  ├─ handle_broadcast()
├─ menu_admin()                 ├─ handle_dh_exchange()
├─ submenu_*() [6 menus]        └─ users.txt (BD simples)
└─ enviar_e_receber()

          ↕ TCP Socket (Porto 10000)
          ↕ Ligação Persistente & Segura (E2EE)
```

---

## 🔌 Protocolo TCP

| Comando       | Exemplo                | Resposta             |
| ------------- | ---------------------- | -------------------- |
| AUTH          | `AUTH admin admin123`  | `AUTH_SUCCESS:ADMIN` |
| REGISTER      | `REGISTER user pass`   | `REGISTER_OK`        |
| JOIN          | `JOIN #geral`          | `JOIN_OK`            |
| BROADCAST     | `BROADCAST #geral ola` | `BCAST_SENT`         |
| LEAVE         | `LEAVE`                | `LEAVE_OK`           |
| SEND_MSG      | `SEND_MSG user msg`    | `MSG_SENT`           |
| LIST_ALL      | `LIST_ALL`             | Tabela utilizadores  |
| LIST_CHANNELS | `LIST_CHANNELS`        | Canais + ocupância   |
| GET_INFO      | `GET_INFO`             | Info servidor        |

---

## 📁 Ficheiros

```
/C-Cord/
├── client_linux.c         — Cliente com TUI (1,825 linhas, 0 warnings)
├── server_linux.c         — Servidor com select() (1,681 linhas, 0 warnings)
├── users.txt              — Base de dados (ID:User:Pass:Role:State)
├── README.md              — Este ficheiro
├── DOCUMENTACAO.md        — Guia técnico detalhado
├── FASES_DO_PROJETO.md    — Guia explicativo das 4 Fases Arquiteturais
├── ETAPA3_EXPLICACAO.md   — Explicação detalhada do código da Etapa 3
├── PLANO_RISCOS_TESTES_ETAPA3_FINAL.md — Plano de Riscos e Matriz de Testes
├── EVOLUCAO_ARQUITETURA.md — Overview técnico Etapa 3
└── TESTES.md              — Guia de testes completo
```

---

## ✅ Qualidade de Código

| Métrica                     | Valor          |
| --------------------------- | -------------- |
| Linhas de código (Cliente)  | 1,825          |
| Linhas de código (Servidor) | 1,681          |
| **Total**                   | **3,506**      |
| Funções principais          | 32+            |
| Linhas de comentários       | 800+           |
| Compilação                  | 0 warnings ✅  |
| Testes                      | Passando ✅    |
| Documentação                | 100% pt_PT ✅  |

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

| Ficheiro             | Objetivo                               |
| -------------------- | -------------------------------------- |
| **DOCUMENTACAO.md**  | Guia técnico + arquitetura + protocolo |
| **RESUMO_ETAPA3.md** | Resumo mockups + menus + roadmap       |
| **README.md**        | Setup + quick start (ESTE FICHEIRO)    |

---

## 🔐 Segurança (Etapa 4 - Finalizada)

✅ Encriptação Ponta-a-Ponta E2EE (Cifra de César Simétrica)
✅ Hashing Criptográfico DJB2 das Passwords
✅ Assinaturas Toy RSA para autenticação de Hashes
✅ Acordo de Chaves Diffie-Hellman Automático  
✅ Diferenciação USER/ADMIN e Prevenção de Buffer Overflows

---

## 🔄 Roadmap

| Etapa | Status | Funcionalidade                            |
| ----- | ------ | ----------------------------------------- |
| 1     | ✅     | Cliente TCP + AUTH + GET_INFO             |
| 2     | ✅     | Canais + JOIN/LEAVE/BROADCAST + Mensagens |
| 3     | ✅     | TUI + Menus + Chat tempo real (select)    |
| 4     | ✅     | E2EE + DH + RSA + Hash + Segurança Zero-Trust |

---

## 💻 Requisitos

- **OS:** Linux (x86_64)
- **Compilador:** GCC 9.0+
- **Bibliotecas:** libc, libm
- **Porto:** 10000

---

**Versão:** 4.0 | **Data:** Junho de 2026 | **Status:** ✅ Etapa 4 Completa
