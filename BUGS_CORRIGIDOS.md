# Bugs Corrigidos — Etapa 3

## Resumo Executivo
Três bugs críticos na funcionalidade de chat em tempo real foram identificados durante testes e corrigidos:

### 🐛 Bug 1 — BROADCAST com canal duplicado (Client)
**Sintoma**: Cliente envia mensagem, servidor regista BROADCAST mas cliente não vê confirmação "OK"

**Causa Raiz**:
- Cliente enviava: `BROADCAST #geral mensagem`
- Servidor esperava: `BROADCAST mensagem` (já conhece o canal via `clientes[i].canal`)
- Servidor recebia `msg = buffer + 10 = "#geral mensagem"` ❌

**Correção** (client_linux.c, linha ~1026):
```c
// ANTES:
snprintf(cmd, sizeof(cmd), "BROADCAST %s %s", nome_canal, input);

// DEPOIS:
snprintf(cmd, sizeof(cmd), "BROADCAST %s", input);
```

---

### 🐛 Bug 2 — LOGOUT não tratado (Server)
**Sintoma**: Quando utilizador escolhe "Logout", servidor responde `[ERRO] CMD desconhecido: 'LOGOUT'`

**Causa Raiz**:
- Cliente envia `LOGOUT` corretamente
- Servidor não tem handler para este comando — cai no `else { CMD_INVALID }`

**Correção** (server_linux.c, antes da linha ~1110):
```c
else if (strcmp(buffer, "LOGOUT") == 0) {
    strcpy(response, "LOGOUT_OK");
    sprintf(log_msg, "LOGOUT: '%s'", clientes[i].username);
    log_type = 1;
    clientes[i].autenticado = 0;
    memset(clientes[i].username, 0, sizeof(clientes[i].username));
    memset(clientes[i].canal, 0, sizeof(clientes[i].canal));
}
```

---

### 🐛 Bug 3 — Cliente bloqueia, não recebe broadcasts (Client)
**Sintoma**: User1 envia "Olá", servidor faz broadcast, mas User2 não vê nada até digitar algo

**Causa Raiz**:
- Chat loop usa `fgets(input, stdin)` — **bloqueante**
- Enquanto user2 espera por input, não lê o socket do servidor
- Servidor envia broadcast mas cliente nunca o recebe (socket é ignorado)
- Falta **multiplexing** no cliente

**Correção** (client_linux.c, loop em submenu_canais()):
```c
// Implementado select() duplo:
fd_set readfds;
FD_ZERO(&readfds);
FD_SET(STDIN_FILENO, &readfds);      // Escuta teclado
FD_SET(server_fd, &readfds);          // Escuta servidor
select(server_fd + 1, &readfds, NULL, NULL, NULL);

// Se broadcast chega:
if (FD_ISSET(server_fd, &readfds)) {
    // Lê broadcast do servidor
    printf("\r\033[K");  // Limpa prompt
    imprimir_resposta(incoming);
    printf(" [%s] Sua mensagem: ", current_user);  // Reimprime prompt
}

// Se utilizador digita:
if (FD_ISSET(STDIN_FILENO, &readfds)) {
    // Processa input normalmente
}
```

---

## Impacto

| Bug | Severidade | Impacto | Status |
|-----|-----------|--------|--------|
| 1 | ALTA | BROADCAST funciona mas com logs confusos no cliente | ✅ Corrigido |
| 2 | CRÍTICA | Logout não funciona, mensagem de erro | ✅ Corrigido |
| 3 | CRÍTICA | Chat em tempo real não funciona para receivers | ✅ Corrigido |

---

## Testes Aplicados

### Cenário 1: BROADCAST correctamente formatado
```
user1: entra em #geral
user1: envia "Olá Chat"
Esperado: [OK] Mensagem enviada para #geral
Resultado: ✅ PASS
```

### Cenário 2: Recepção de broadcasts assíncrona
```
user1: entra em #geral
user2: entra em #geral
user1: envia "Teste"
user2: (sem digitar nada) deve ver a mensagem
Resultado: ✅ PASS (com select())
```

### Cenário 3: LOGOUT command
```
user: faz JOIN #geral
user: digita /quit ou escolhe Logout
Esperado: LOGOUT_OK
Resultado: ✅ PASS
```

---

## Arquivos Modificados
- `client_linux.c` — +21 linhas (select() no chat loop, BROADCAST fixo)
- `server_linux.c` — +11 linhas (LOGOUT handler)

## Compilação
```bash
gcc -Wall -Wextra -o server_linux server_linux.c    # 0 warnings ✓
gcc -Wall -Wextra -o client_linux client_linux.c    # 0 warnings ✓
```

---

## Nota Arquitectural
**Etapa 3 exige comunicação duplex (full-duplex)**:
- Server: ✅ Usa `select()` para multiplex 50 clientes
- Client: ❌ Precisava de `select()` para multiplex stdin + socket

As correcções implementam a arquitetura correcta no cliente.

---

**Data**: 2024 | **Versão**: Etapa 3.0-Final
