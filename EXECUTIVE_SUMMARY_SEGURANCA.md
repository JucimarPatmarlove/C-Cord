# 🔒 AUDIT EXECUTIVO — C-CORD ETAPA 3

**Data:** 2025  
**Escopo:** server_linux.c (1,355 linhas) + client_linux.c (1,757 linhas)  
**Auditor:** Senior POSIX Network Engineer + Cybersecurity Auditor  

---

## 📊 RESUMO DE ACHADOS

| Severidade | Contagem | Status |
|-----------|----------|--------|
| 🔴 CRÍTICA | 5 | ⚠️ REQUIRES IMMEDIATE FIX |
| 🟠 ALTA | 2 | ⚠️ REQUIRES MEDIUM PRIORITY FIX |
| 🟡 MÉDIA | 1 | ℹ️ NON-BREAKING, IMPROVE ROBUSTNESS |
| 🟢 BAIXA | 1 | ℹ️ COSMETIC/UX IMPROVEMENT |
| **TOTAL** | **9** | ⚠️ **NOT PRODUCTION READY** |

**Estimado Tempo de Remediação:** 2-4 horas (aplica fixes + testa + valida)

---

## 🔴 VULNERABILIDADES CRÍTICAS (5)

### V1: FD Memory Leak — Ctrl+C Disconnect
**Impacto:** Memory exhaustion → Denial of Service (DoS)  
**Como acontece:** Após 50 Ctrl+C, servidor rejeita novos clientes  
**Raiz:** Cleanup incompleto no select() loop  
**Remediação:** Add `FD_CLR()` + `memset()` após disconnect  

### V2: recv() Buffer Overflow
**Impacto:** Stack smashing → Crash/RCE  
**Como acontece:** recv() retorna bytes > BUF_SIZE-1  
**Raiz:** Validação missing antes de `buffer[n] = '\0'`  
**Remediação:** Add bounds check: `if (n > BUF_SIZE-1) n = BUF_SIZE-1`

### V3: Race Condition File I/O (inbox)
**Impacto:** Data corruption/Message loss  
**Como acontece:** 2 clientes escrevem inbox_*.txt simultaneamente  
**Raiz:** Etapa 3 (select, múltiplos clientes) não é thread-safe  
**Remediação:** Add fcntl() locks (F_WRLCK) around write operations

### V4: SEND_MSG Buffer Overflow
**Impacto:** Stack corruption  
**Como acontece:** sscanf sem validação de bounds  
**Raiz:** Parsing inseguro  
**Remediação:** Add `if (parsed < 3)` check + null-terminate fields

### V5: Auth Bypass BROADCAST
**Impacto:** Unauthorized message injection  
**Como acontece:** Non-auth client pode enviar BROADCAST  
**Raiz:** Falta `if (!autenticado)` check  
**Remediação:** Add authentication validation antes de processar BROADCAST

---

## 🟠 VULNERABILIDADES ALTAS (2)

### V6: Ban Doesn't Kick
**Impacto:** Ghost user → User can still chat while banned  
**Como acontece:** BAN remove users.txt mas não fecha socket ativo  
**Raiz:** Falta loop para encontrar e desconectar user  
**Remediação:** Add user lookup + socket close

### V7: strtok() Non-Reentrant
**Impacto:** Parsing errors under signal interruption  
**Como acontece:** strtok() modifica buffer estático  
**Raiz:** Função não reentrante  
**Remediação:** Use strtok_r() com saveptr

---

## 🟡 VULNERABILIDADES MÉDIAS (1)

### V8: select() Missing Timeout
**Impacto:** Indefinite hang if server fails  
**Como acontece:** select(NULL) com NULL timeout  
**Raiz:** Sem timeout configurado  
**Remediação:** Add `struct timeval tv = {30, 0}`

---

## 🟢 ISSUES COSMÉTICA (1)

### V9: Visual Collision (Terminal)
**Impacto:** Glitch cosmético  
**Como acontece:** Inadequate line clearing  
**Raiz:** \033[K instead of \033[2K  
**Remediação:** Use `\033[2K` for full line clear

---

## ⚖️ MATRIZ DE RISCO

Risk matrix: 5 critical vulns require immediate patching

---

## 📋 ORDEM DE REMEDIAÇÃO RECOMENDADA

**FASE 1 (Priority High, ~1h):**
1. V1 — FD Memory Leak
2. V2 — recv() validation
3. V5 — Auth bypass BROADCAST

**FASE 2 (Priority High, ~1.5h):**
4. V3 — Race condition (fcntl locks)
5. V4 — SEND_MSG overflow
6. V6 — Ban no kick

**FASE 3 (Priority Medium, ~1h):**
7. V7 — strtok_r
8. V8 — select() timeout
9. V9 — Visual collision

---

## ✅ POST-REMEDIATION VALIDATION

Após aplicar todos os fixes:

```bash
# Compilar
gcc -Wall -Wextra -Werror -o server_linux server_linux.c
gcc -Wall -Wextra -Werror -o client_linux client_linux.c

# Testes básicos
# - Memory cleanup test (60 connections with Ctrl+C)
# - Buffer overflow test (send 10000 bytes)
# - Auth bypass test (BROADCAST as guest)
# - Race condition test (20 concurrent SEND_MSG)
```

---

## 📌 STATUS FINAL

**ANTES de fixes:** 🔴 NOT PRODUCTION READY  
**DEPOIS de fixes:** ✅ PRODUCTION READY

**Ficheiro de remediação:** `FIXES_PRONTOS_APLICAR.md`

