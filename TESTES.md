# 🧪 Guia de Testes — C-Cord Etapa 3

## 📋 Conteúdo

1. [Compilação](#compilação)
2. [Executar Servidor](#executar-servidor)
3. [Executar Cliente](#executar-cliente)
4. [Casos de Teste](#casos-de-teste)
5. [Teste de Stress](#teste-de-stress)
6. [Troubleshooting](#troubleshooting)

---

## 🔨 Compilação

```bash
# Compilar servidor
gcc -o server_linux server_linux.c -Wall -Wextra

# Compilar cliente
gcc -o client_linux client_linux.c -Wall -Wextra
```

**Sem warnings?** ✅ Tudo bem!

---

## 🖥️ Executar Servidor

**Terminal 1 — Servidor:**

```bash
./server_linux
```

**Output esperado:**

```
╔════════════════════════════════════════════════════════════════════╗
║                                                                    ║
║                         ⚙️  C-CORD SERVER                         ║
║              Clone simplificado do Discord em C/POSIX              ║
║                                                                    ║
╚════════════════════════════════════════════════════════════════════╝

[SERVIDOR] Iniciado na porta 10000...
[SERVIDOR] À espera de conexões...
```

---

## 👥 Executar Cliente

**Terminal 2 — Cliente 1:**

```bash
./client_linux 127.0.0.1 10000
```

**Terminal 3 — Cliente 2 (opcional, para testar broadcasts):**

```bash
./client_linux 127.0.0.1 10000
```

**Output esperado:**

```
╔════════════════════════════════════════════════════════════════════╗
║                                                                    ║
║                         📱 C-CORD CLIENT                          ║
║              Clone simplificado do Discord em C/POSIX              ║
║                                                                    ║
╚════════════════════════════════════════════════════════════════════╝

[TESTE CONEXÃO] Conectado ao servidor 127.0.0.1:10000 ✅

═══════════════════════════════════════════════════════════════════════
MODE: GUEST (WHITE)
═══════════════════════════════════════════════════════════════════════

[F3] Login          [F4] Menu          [F10] Sair
```

---

## 🧪 Casos de Teste

### TESTE 1: Login (F3)

**Passo 1:** Prima `F3`

```
Utilizador: admin
Palavra-passe: 123456
```

**Esperado:**

- ✅ Modo muda para ADMIN (vermelho)
- ✅ Mensagem: `[LOGIN] Autenticado como ADMIN`

**Teste 2 — Login com utilizador normal:**

```
Utilizador: user1
Palavra-passe: 1234
```

**Esperado:**

- ✅ Modo muda para USER (ciano)
- ✅ Mensagem: `[LOGIN] Autenticado como user1`

**Teste 3 — Login falho:**

```
Utilizador: invalid
Palavra-passe: wrongpass
```

**Esperado:**

- ✅ Mensagem: `[LOGIN] Falha na autenticação`
- ✅ Modo mantém GUEST

---

### TESTE 2: GET_INFO (F4 → Opção 1)

**Pré-requisito:** Estar logado (ADMIN ou USER)

**Passo:** Prima `F4`, depois `1` para GET_INFO

**Esperado:**

- ✅ Conexão temporária aberta
- ✅ Resposta com informações do utilizador
- ✅ Conexão fechada após resposta

**Exemplo de resposta (ADMIN):**

```
[GET_INFO] Tipo: ADMIN
[GET_INFO] Utilizadores: 3
[GET_INFO] Canais ativos: 2
```

---

### TESTE 3: ECHO (F4 → Opção 2)

**Pré-requisito:** Estar logado

**Passo:** Prima `F4`, depois `2`, escreve mensagem

```
Mensagem: Olá, servidor!
```

**Esperado:**

- ✅ Servidor responde com: `[ECHO] Olá, servidor!`
- ✅ Mensagem aparece no cliente

---

### TESTE 4: SEND_MSG (F5)

**Pré-requisito:** Estar logado como ADMIN

**Passo:** Prima `F5`

```
Destinatário: user1
Mensagem: Oi, tudo bem?
```

**Esperado:**

- ✅ Mensagem enviada
- ✅ Se `user1` estiver conectado: recebe broadcast em tempo real
- ✅ Mensagem guardada em `inbox.txt`

---

### TESTE 5: LIST_CHANNELS (F6)

**Pré-requisito:** Estar logado como ADMIN

**Passo:** Prima `F6`

**Esperado:**

- ✅ Lista de canais aparece:

```
[CANAIS]
  #geral
  #random
  #desenvolvimento
```

---

### TESTE 6: BROADCAST (F7)

**Pré-requisito:** Estar logado como ADMIN e com 2+ clientes conectados

**Cliente 1 (ADMIN):** Prima `F7`

```
Selecciona canal: #geral
Mensagem: Olá a todos!
```

**Cliente 2 (USER):**

- ✅ Recebe em tempo real: `[BROADCAST #geral] admin: Olá a todos!`

**Esperado:**

- ✅ Todos no canal recebem
- ✅ Mensagem não bloqueia o terminal

---

### TESTE 7: DELETE_USER (F8 — ADMIN)

**Pré-requisito:** Estar logado como ADMIN

**Passo:** Prima `F8`

```
Utilizador a eliminar: user1
```

**Esperado:**

- ✅ Utilizador eliminado de `users.txt`
- ✅ Se `user1` estiver conectado: recebe notificação e é expulso
- ✅ Mensagem: `[DELETE_USER] Conta eliminada pelo administrador`

---

### TESTE 8: SUSPEND_USER (F9 — ADMIN)

**Pré-requisito:** Estar logado como ADMIN

**Passo:** Prima `F9`

```
Utilizador a suspender: user2
```

**Esperado:**

- ✅ Utilizador suspenso em `users.txt`
- ✅ Se `user2` estiver conectado: é desconectado imediatamente
- ✅ Mensagem: `[SUSPEND_USER] Sua conta foi suspensa`

---

### TESTE 9: Logout (F4 → Opção 3)

**Pré-requisito:** Estar logado

**Passo:** Prima `F4`, depois `3` para Logout

**Esperado:**

- ✅ Modo volta a GUEST (branco)
- ✅ Mensagem: `[LOGOUT] Desconectado com sucesso`

---

### TESTE 10: Exit (F10)

**Passo:** Prima `F10`

**Esperado:**

- ✅ Cliente fecha
- ✅ Socket encerrado
- ✅ Servidor regista desconexão: `[CLIENTE] Desconectado`

---

### TESTE 11: Persistência de Canais e Select (Novo na Revisão)

**Passo:**

1. No Cliente 1, abra a TUI, faça login e entre no canal `#linux`.
2. Mantenha-o inativo por 10 segundos (Testando `select` blocking timeout).
3. No Cliente 2, faça login e entre no canal `#linux`.
4. O Cliente 2 faz `BROADCAST #linux Teste persistente`.

**Esperado:**

- ✅ O `select()` do Cliente 1 deteta a atividade na rede.
- ✅ O Cliente 1 imprime a mensagem instantaneamente sem que a sua sessão expire.
- ✅ A ligação manteve-se aberta de forma persistente através do Master `FD_SET` do servidor.

---

## 🔥 Teste de Stress

### Teste 1: Múltiplos Clientes Simultâneos

**Terminal 1:** Servidor

```bash
./server_linux
```

**Terminais 2-5:** 4 clientes

```bash
./client_linux 127.0.0.1 10000 &
./client_linux 127.0.0.1 10000 &
./client_linux 127.0.0.1 10000 &
./client_linux 127.0.0.1 10000 &
```

**Teste:**

1. Todos fazem login
2. Alguns enviam mensagens
3. ADMIN faz broadcast
4. Todos recebem sem crash ✅

---

### Teste 2: Desconexão Abrupta (Ctrl+C)

**Passo:**

1. Cliente conectado e logado
2. Prima `Ctrl+C` no cliente
3. Verifique servidor

**Esperado:**

- ✅ Servidor regista: `[CLIENTE] Desconectado`
- ✅ FD_SET limpo corretamente
- ✅ Sem segfault no próximo ciclo `select()`

---

### Teste 3: Mensagens Grandes

**Cliente:** Prima `F5` e envie mensagem com 1000+ caracteres

**Esperado:**

- ✅ Sem buffer overflow
- ✅ Mensagem truncada a 256 chars (conforme definido)
- ✅ Servidor não caramba

---

### Teste 4: Race Condition em Ficheiros

**Setup:** 2 clientes (ADMIN)

**Terminais em paralelo:**

```bash
# Terminal 2
./client_linux 127.0.0.1 10000 << EOF
[F3]
admin
123456
[F5]
user1
Msg 1
[F5]
user2
Msg 2
EOF

# Terminal 3 (imediatamente após)
./client_linux 127.0.0.1 10000 << EOF
[F3]
admin
123456
[F5]
user1
Msg 3
EOF
```

**Esperado:**

- ✅ Ficheiro `inbox.txt` sem corrupção
- ✅ Todas as mensagens guardadas
- ✅ Sem entradas duplicadas ou perdidas

---

## 🔧 Troubleshooting

### ❌ `Connection refused`

**Solução:** Verifique se servidor está a correr na porta 10000

```bash
netstat -tuln | grep 10000
```

### ❌ `Address already in use`

**Solução:** Servidor anterior ainda em execução

```bash
killall server_linux
```

### ❌ Caracteres estranhos no terminal

**Solução:** Terminal não suporta ANSI colors

```bash
export TERM=xterm-256color
./client_linux 127.0.0.1 10000
```

### ❌ Segfault no servidor

**Solução:** Verifique se existe corrupção de FD_SET

- Compile com `-g` e use `gdb`

```bash
gcc -g -o server_linux server_linux.c
gdb ./server_linux
```

### ❌ Broadcast não funciona

**Solução:** Verifique se há múltiplos clientes conectados

```bash
# Terminal do servidor mostra número de clientes
[SERVIDOR] Cliente conectado (total: 2)
```

---

## 📊 Checklist Final de Validação

- [ ] Servidor inicia sem erros
- [ ] Cliente conecta com sucesso
- [ ] Login ADMIN funciona (modo vermelho)
- [ ] Login USER funciona (modo ciano)
- [ ] GET_INFO retorna dados
- [ ] ECHO funciona
- [ ] SEND_MSG guarda em `inbox.txt`
- [ ] Broadcast recebido em tempo real
- [ ] DELETE_USER elimina conta
- [ ] SUSPEND_USER desconecta utilizador
- [ ] Logout volta a GUEST
- [ ] Exit fecha cliente
- [ ] Múltiplos clientes simultâneos
- [ ] Ctrl+C não causa segfault
- [ ] Ficheiros não se corrompem com concurrent writes

---

## 🎓 Notas Académicas

**Etapa 3 — Validação de Arquitectura:**

- ✅ select() faz multiplexação correcctamente
- ✅ Ligações persistentes mantêm estado
- ✅ Broadcasts chegam em tempo real
- ✅ FD_SET gerido sem memory leaks
- ✅ Ficheiros I/O seguros contra race conditions

**Se todos os testes passarem: Etapa 3 está completa e pronta para apresentação! 🎉**

---

**Autor:** C-Cord Project
**Versão:** Etapa 3 (select() + Ligações Persistentes + TUI)
**Última atualização:** 2026-06-02
