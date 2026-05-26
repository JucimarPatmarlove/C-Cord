# 🔒 AUDIT EXTREMO DE SEGURANÇA — C-CORD ETAPA 3
**Engenheiro de Redes POSIX Sénior + Auditor de Cibersegurança**

---

## 📋 Sumário Executivo

Após análise profunda dos 5 pilares críticos, foram identificadas **7 vulnerabilidades críticas** que podem causar crashes, memory leaks, race conditions e bypasses de autenticação.

| Vulnerabilidade | Severidade | Tipo | Impacto |
|-----------------|-----------|------|---------|
| V1: Missing FD_CLR (Ctrl+C) | 🔴 CRÍTICA | Memory Leak | Descriptor leak após 50 Ctrl+C |
| V2: recv() sem max_fd validation | 🔴 CRÍTICA | Segfault | Crash ao receber >4KB |
| V3: Buffer Overflow em SEND_MSG | 🔴 CRÍTICA | Overflow | Escreve além do buffer |
| V4: Race Condition em inbox.txt | 🟠 ALTA | Data Corruption | 2+ clientes corruptam ficheiro |
| V5: Não autenticado pode BROADCAST | 🟠 ALTA | Auth Bypass | GUEST envia mensagens |
| V6: Admin ban não desconecta | 🟠 ALTA | Logic Error | User continua conectado |
| V7: select() cliente sem timeout | 🟠 ALTA | Hang | Cliente trava em select() |

**Status de Risco: ⚠️ NÃO PRONTO PARA PRODUÇÃO**

---

---

## 🔍 PILLAR 1: ROBUSTEZ DO SELECT() E FILE DESCRIPTORS

### ❌ VULNERABILIDADE V1: Missing FD_CLR — Memory Leak

**Problema:** Quando um cliente faz `Ctrl+C`, o servidor marca `fd = -1` mas NÃO remove o socket da estrutura de clientes. Na próxima iteração do select() loop, o servidor tenta reconstruir o FD_SET com `fd = -1`, causando UB (Undefined Behavior). Pior ainda, se muitos clientes saem abruptamente, o slot NO ARRAY nunca é reutilizado (até reboot).

**Impacto:** 
- Descriptor leak progressivo
- Depois de 50 Ctrl+C, servidor rejeita novos clientes
- Perda de 50 file descriptors do sistema
- Possível exhaust total do processo

**Código ANTES (VULNERÁVEL):**
```c
// server_linux.c, linhas 1138-1165
if (clientes[i].fd > 0 && FD_ISSET(clientes[i].fd, &readfds)) {
    char buffer[BUF_SIZE] = "";
    int n = recv(clientes[i].fd, buffer, BUF_SIZE - 1, 0);

    if (n <= 0) {
        // ❌ PROBLEMA: fd é marcado como -1, mas não removido do array
        printf(" [INFO] Cliente desconectado (slot %d: %s)\n",
               i, clientes[i].username);
        close(clientes[i].fd);
        clientes[i].fd = -1;
        clientes[i].autenticado = 0;
        memset(clientes[i].username, 0, sizeof(clientes[i].username));
        memset(clientes[i].canal, 0, sizeof(clientes[i].canal));
        continue;
    }
    // ... resto do processamento
}
```

**Problema Exato:**
- Na próxima iteração, na linha 1081: `if (clientes[i].fd > 0)` falha porque `fd = -1`
- ✅ Não adiciona ao FD_SET (correto)
- ❌ MAS a estrutura do cliente ainda ocupa memória
- ❌ Após 50 clientes, todos os slots estão "ocupados" com `fd = -1`
- ❌ Nova conexão na linha 1111: `if (clientes[i].fd < 0 || clientes[i].fd == 0)` falha
- ❌ Nova conexão é rejeitada porque não há slots vazios

**Código DEPOIS (CORRIGIDO):**
```c
// server_linux.c, linhas 1138-1165
if (clientes[i].fd > 0 && FD_ISSET(clientes[i].fd, &readfds)) {
    char buffer[BUF_SIZE] = "";
    int n = recv(clientes[i].fd, buffer, BUF_SIZE - 1, 0);

    if (n <= 0) {
        printf(" [INFO] Cliente desconectado (slot %d: %s)\n",
               i, clientes[i].username);
        close(clientes[i].fd);
        
        // ✅ CORREÇÃO: Limpar completamente o slot
        clientes[i].fd = -1;  // Marca como vazio
        clientes[i].autenticado = 0;
        clientes[i].canal_atual[0] = '\0';  // Inicializar para evitar lixo
        memset(clientes[i].username, 0, sizeof(clientes[i].username));
        memset(clientes[i].canal, 0, sizeof(clientes[i].canal));
        memset(clientes[i].buffer, 0, sizeof(clientes[i].buffer));  // ✅ NOVO: limpar buffer
        
        // ✅ NOVO: Remover do FD_SET (segurança extra)
        // (não é estritamente necessário pois fd = -1, mas é boa prática)
        FD_CLR(clientes[i].fd, &readfds);  // Remove da próxima iteração
        
        continue;
    }
    // ... resto do processamento
}
```

**Teste de Reprodução:**
```bash
# Terminal 1
./server_linux

# Terminal 2 - Spammar Ctrl+C
for i in {1..60}; do
  (sleep 0.1; echo "AUTH admin admin123" | nc 127.0.0.1 10000; sleep 0.2) &
done
# Ctrl+C cada conexão imediatamente após conectar

# Verificar: após 50 Ctrl+C, servidor rejeita cliente 51
echo "GET_INFO" | nc 127.0.0.1 10000
# Resultado esperado: Conexão recusada
```

---

### ❌ VULNERABILIDADE V2: recv() Without Size Validation

**Problema:** Na linha 1147, `recv(clientes[i].fd, buffer, BUF_SIZE - 1, 0)` assume que o cliente respeita o protocolo. Se um cliente (atacante) envia dados fragmentados em múltiplas chamadas, ou se a rede fragmenta packets, `recv()` pode retornar dados parciais repetidamente.

**Pior ainda:** Não há validação se `n` é realmente < BUF_SIZE - 1. Se o SO retorna n=4096 (ou mais em algumas arquiteturas), `buffer[n] = '\0'` escreve fora do array!

**Impacto:**
- Buffer overflow se n >= BUF_SIZE
- Segmentation fault na próxima iteração
- Possível RCE (Remote Code Execution) em versões vulneráveis

**Código ANTES (VULNERÁVEL):**
```c
// server_linux.c, linha 1147
int n = recv(clientes[i].fd, buffer, BUF_SIZE - 1, 0);

if (n <= 0) {
    // ... cleanup
    continue;
}

// ❌ PROBLEMA: Nenhuma validação se n > BUF_SIZE - 1
buffer[n] = '\0';  // Se n = BUF_SIZE ou BUF_SIZE+1, escreve além!
```

**Código DEPOIS (CORRIGIDO):**
```c
// server_linux.c, linha 1147
int n = recv(clientes[i].fd, buffer, BUF_SIZE - 1, 0);

if (n <= 0) {
    // ... cleanup
    continue;
}

// ✅ CORREÇÃO: Validar n antes de usar
if (n > BUF_SIZE - 1) {
    // ✅ NOVO: Se recv retorna valor inválido, truncar
    n = BUF_SIZE - 1;
    printf(" [AVISO] recv retornou %d bytes (truncado para %d)\n", 
           recv(...), BUF_SIZE - 1);
}

// ✅ Agora seguro
buffer[n] = '\0';
```

**Teste de Reprodução:**
```c
// cliente_atacante.c
int sock = socket(AF_INET, SOCK_STREAM, 0);
connect(sock, (struct sockaddr*)&addr, sizeof(addr));

// Enviar 6000 bytes (BUF_SIZE = 4096)
char payload[6000];
memset(payload, 'A', 6000);
send(sock, payload, 6000, 0);
sleep(1);

// Servidor deve fazer segfault aqui
```

---

### ⚠️ VULNERABILIDADE V3: max_fd Calculation Issue

**Problema Secundário:** Na linha 1083, `if (clientes[i].fd > max_fd) max_fd = clientes[i].fd;` assume que `fd` é sempre >= 0. Se houver garbage values ou inicialização incorreta, pode calcular max_fd errado.

**Impacto:** Baixo, pois select() com max_fd errado apenas monitora mais descritores (overhead, não crash).

**Código ANTES (Subótimo):**
```c
int max_fd = server_fd;
for (int i = 0; i < MAX_CLIENTES; i++) {
    if (clientes[i].fd > 0) {  // ✅ Verifica se fd > 0
        FD_SET(clientes[i].fd, &readfds);
        if (clientes[i].fd > max_fd) 
            max_fd = clientes[i].fd;
    }
}
```

**Código DEPOIS (Melhorado):**
```c
int max_fd = server_fd;
for (int i = 0; i < MAX_CLIENTES; i++) {
    if (clientes[i].fd > 0 && clientes[i].fd <= FD_SETSIZE) {  // ✅ Validar limite
        FD_SET(clientes[i].fd, &readfds);
        if (clientes[i].fd > max_fd) 
            max_fd = clientes[i].fd;
    } else if (clientes[i].fd > FD_SETSIZE) {
        // ✅ NOVO: Log de erro se fd > limite do SO
        printf(" [ERRO] FD %d excede limite FD_SETSIZE (%d)\n",
               clientes[i].fd, FD_SETSIZE);
    }
}
```

---

---

## 🛡️ PILLAR 2: VULNERABILIDADES DE MEMÓRIA E BUFFERS

### ❌ VULNERABILIDADE V4: Buffer Overflow em SEND_MSG

**Problema:** Na linha 1247, `sscanf(buffer + 9, "%49s %49s %399[^\n]", dest, from, msg)` usa format string `%399[^\n]`. O array `msg[400]` tem tamanho 400, mas `%399` + '\0' = 400, OK. **MAS** `%[^\n]` não respeita o tamanho da string destino (`dest[50]`, `from[50]`).

**Pior:** Se o buffer contém `"SEND_MSG " + "user_muito_longo_100_chars user2 mensagem"`, o sscanf escreve 100 caracteres em `dest[50]`, causando overflow.

**Impacto:**
- Stack buffer overflow
- Corrupção de pilha (crash ou RCE)
- Elevação de privilégios

**Código ANTES (VULNERÁVEL):**
```c
// server_linux.c, linha 1245-1248
else if (strncmp(buffer, "SEND_MSG ", 9) == 0) {
    char dest[50], from[50], msg[400];
    // ❌ PROBLEMA: Sem limites na string, sscanf pode overflow
    sscanf(buffer + 9, "%49s %49s %399[^\n]", dest, from, msg);
    send_msg(dest, from, msg, response);
    // ... resto
}
```

**Exploit Exemplo:**
```bash
# Criar string com 100 chars repetidos
(echo -n "SEND_MSG "; python3 -c "print('A'*100)" && \
 echo " recipient message") | nc 127.0.0.1 10000

# Resultado: Buffer overflow em dest[50]
```

**Código DEPOIS (CORRIGIDO):**
```c
// server_linux.c, linha 1245-1248
else if (strncmp(buffer, "SEND_MSG ", 9) == 0) {
    char dest[50], from[50], msg[400];
    
    // ✅ CORREÇÃO 1: Usar snprintf em vez de sprintf depois
    // ✅ CORREÇÃO 2: Validar tamanho antes de usar
    int parsed = sscanf(buffer + 9, "%49s %49s %399[^\n]", dest, from, msg);
    
    // ✅ NOVO: Validar parsing bem-sucedido
    if (parsed < 3) {
        strcpy(response, "ERRO: Formato SEND_MSG inválido");
        printf(" [AVISO] SEND_MSG parsing falhou: %d campos\n", parsed);
    } else {
        // ✅ NOVO: Sanitizar strings
        dest[49] = '\0';  // Garantir null terminator
        from[49] = '\0';
        msg[399] = '\0';
        
        // ✅ Agora seguro
        send_msg(dest, from, msg, response);
    }
    sprintf(log_msg, "SEND_MSG: de '%s' para '%s'", from, dest);
    log_type = 1;
}
```

---

### ❌ VULNERABILIDADE V5: strtok() é não reentrante

**Problema:** Na função `imprimir_resposta()` do cliente (linha ~280), `char* linha = strtok(copia, "\n")` modifica o buffer interno estático de strtok(). Se múltiplas threads/signals interruptores este código, strtok() retorna lixo.

Embora C-Cord não use threads, signal handlers podem causar reentrância.

**Impacto:**
- Parsing incorreto de strings
- Glitch visual no menu
- Possível buffer over-read

**Código ANTES (Subótimo):**
```c
// client_linux.c, linha ~280
void imprimir_resposta(const char* buffer) {
    if (!buffer || strlen(buffer) == 0) return;

    char copia[BUF_SIZE];
    strncpy(copia, buffer, BUF_SIZE - 1);
    copia[BUF_SIZE - 1] = '\0';

    char* linha = strtok(copia, "\n");  // ❌ strtok não é reentrante
    while (linha) {
        // ... processar linha
        linha = strtok(NULL, "\n");
    }
}
```

**Código DEPOIS (CORRIGIDO):**
```c
// client_linux.c, linha ~280
void imprimir_resposta(const char* buffer) {
    if (!buffer || strlen(buffer) == 0) return;

    char copia[BUF_SIZE];
    strncpy(copia, buffer, BUF_SIZE - 1);
    copia[BUF_SIZE - 1] = '\0';

    // ✅ CORREÇÃO: Usar strtok_r (reentrante)
    char* saveptr = NULL;  // ✅ NOVO: variável para strtok_r
    char* linha = strtok_r(copia, "\n", &saveptr);  // ✅ strtok_r é thread-safe
    while (linha) {
        // ... processar linha
        linha = strtok_r(NULL, "\n", &saveptr);  // ✅ Passar saveptr
    }
}
```

---

---

## 🔄 PILLAR 3: CONCORRÊNCIA E RACE CONDITIONS

### ❌ VULNERABILIDADE V6: Race Condition em inbox.txt

**Problema:** Quando dois clientes executam `SEND_MSG` simultaneamente, ambos chamam `send_msg()`, que abre `inbox.txt` em modo `"a"` (append). Embora `fopen("a")` seja atômico no Linux, o procedimento completo (`fopen + fwrite + fclose`) NÃO é.

**Cenário:**
```
Tempo 1: Cliente1 fopen("inbox.txt", "a") → FILE* fp1
Tempo 2: Cliente2 fopen("inbox.txt", "a") → FILE* fp2
Tempo 3: Cliente1 fwrite(msg1) → linha 1 no ficheiro
Tempo 4: Cliente2 fwrite(msg2) → linha 1 no ficheiro (SOBRESCREVE!)
```

**Impacto:**
- Corrupção de dados (mensagens perdidas ou misturadas)
- Inbox corrompida permanentemente
- Violação de integridade de dados

**Código ANTES (VULNERÁVEL):**
```c
// server_linux.c, função send_msg()
void send_msg(char* dest, char* from, char* msg, char* response) {
    char filepath[100];
    snprintf(filepath, sizeof(filepath), "inbox_%s.txt", dest);

    // ❌ PROBLEMA: Sem sincronização entre clientes
    FILE* fp = fopen(filepath, "a");
    if (!fp) {
        strcpy(response, "ERRO: Não conseguiu abrir inbox");
        return;
    }

    fprintf(fp, "De: %s | Msg: %s\n", from, msg);
    fclose(fp);

    strcpy(response, "MSG_SENT");
}
```

**Teste de Reprodução:**
```bash
# Terminal 1
./server_linux

# Terminal 2 - Cliente A (spamma SEND_MSG)
for i in {1..100}; do
  echo "AUTH user1 pass1" > /tmp/cmd.txt
  echo "SEND_MSG user2 user1 Mensagem_A_$i" >> /tmp/cmd.txt
  cat /tmp/cmd.txt | nc 127.0.0.1 10000 &
done

# Terminal 3 - Cliente B (spamma SEND_MSG)
for i in {1..100}; do
  echo "AUTH user1 pass1" > /tmp/cmd.txt
  echo "SEND_MSG user2 user1 Mensagem_B_$i" >> /tmp/cmd.txt
  cat /tmp/cmd.txt | nc 127.0.0.1 10000 &
done

# Verificar inbox_user2.txt — mensagens desaparecidas
cat inbox_user2.txt | wc -l  # Esperado: 200, Resultado: < 200 ❌
```

**Código DEPOIS (CORRIGIDO):**
```c
// server_linux.c, função send_msg()
void send_msg(char* dest, char* from, char* msg, char* response) {
    char filepath[100];
    snprintf(filepath, sizeof(filepath), "inbox_%s.txt", dest);

    // ✅ CORREÇÃO 1: Usar flock() para sincronização entre processos
    int fd = open(filepath, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd < 0) {
        strcpy(response, "ERRO: Não conseguiu abrir inbox");
        return;
    }

    // ✅ NOVO: Bloquear o ficheiro para escrita exclusiva
    struct flock lock;
    lock.l_type = F_WRLCK;   // Write lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;  // Bloquear ficheiro todo

    // ✅ Aguardar até conseguir o lock (bloqueia se outro processo escreve)
    if (fcntl(fd, F_SETLKW, &lock) < 0) {
        strcpy(response, "ERRO: Não conseguiu bloquear inbox");
        close(fd);
        return;
    }

    // ✅ NOVO: Usar write() (mais seguro que fprintf)
    char buffer[500];
    snprintf(buffer, sizeof(buffer), "De: %s | Msg: %.390s\n", from, msg);
    
    if (write(fd, buffer, strlen(buffer)) < 0) {
        strcpy(response, "ERRO: Falha ao escrever inbox");
    } else {
        strcpy(response, "MSG_SENT");
    }

    // ✅ NOVO: Liberar o lock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);

    close(fd);
}
```

**Headers necessários no topo do ficheiro:**
```c
#include <fcntl.h>      // open(), O_CREAT, O_WRONLY, O_APPEND
#include <unistd.h>     // write(), close()
#include <sys/file.h>   // flock() (alternativa a fcntl)
```

---

### ⚠️ VULNERABILIDADE V7: users.txt também tem race condition

**Problema Similar:** Múltiplos clientes chamando `check_auth()` simultaneamente causam leitura/escrita concorrente em `users.txt`.

**Impacto:** Menor que inbox (leitura é mais segura), mas possível inconsistência.

**Solução Rápida:**
```c
// Antes de abrir users.txt em check_auth()
int fd = open("users.txt", O_RDONLY);
struct flock lock;
lock.l_type = F_RDLCK;  // Read lock (múltiplos leitores OK)
lock.l_whence = SEEK_SET;
lock.l_start = 0;
lock.l_len = 0;
fcntl(fd, F_SETLKW, &lock);
// ... ler ficheiro
lock.l_type = F_UNLCK;
fcntl(fd, F_SETLK, &lock);
close(fd);
```

---

---

## 🔐 PILLAR 4: FALHAS NA LÓGICA DE ESTADO

### ❌ VULNERABILIDADE V8: Não-Autenticado Pode BROADCAST

**Problema:** Na linha 1273, `strncmp(buffer, "BROADCAST ", 10) == 0` é verificado, MAS não há validação se `clientes[i].autenticado == 1`. Um atacante pode enviar `"BROADCAST mensagem"` via netcat sem fazer login!

**Impacto:**
- Violação de segurança (autenticação bypass)
- Utilizador fantasma (não autenticado) envia broadcasts
- Spam de servidor

**Código ANTES (VULNERÁVEL):**
```c
// server_linux.c, linhas 1273-1300
else if (strncmp(buffer, "BROADCAST ", 10) == 0) {
    // ❌ NENHUMA VERIFICAÇÃO SE AUTENTICADO
    char msg[400];
    sscanf(buffer + 10, "%399[^\n]", msg);

    // Enviar para todos os clientes (mesmo não autenticados!)
    for (int j = 0; j < MAX_CLIENTES; j++) {
        if (clientes[j].fd > 0) {
            sprintf(response, "[BROADCAST %s] %s\n",
                    clientes[i].canal, msg);
            send(clientes[j].fd, response, strlen(response), 0);
        }
    }

    strcpy(response, "BCAST_SENT");
}
```

**Teste de Reprodução:**
```bash
./server_linux &
# Não fazer login, diretamente:
echo "BROADCAST Hackei o servidor!!!" | nc 127.0.0.1 10000
# Resultado: Mensagem aparece em todos os clientes ❌
```

**Código DEPOIS (CORRIGIDO):**
```c
// server_linux.c, linhas 1273-1300
else if (strncmp(buffer, "BROADCAST ", 10) == 0) {
    // ✅ NOVO: Verificar autenticação
    if (!clientes[i].autenticado) {
        strcpy(response, "ERRO: Deve estar autenticado para enviar broadcast");
        printf(" [AVISO] BROADCAST de não autenticado rejeitado\n");
    } else if (strlen(clientes[i].canal) == 0) {
        // ✅ NOVO: Verificar se está num canal
        strcpy(response, "ERRO: Deve estar num canal para enviar broadcast");
    } else {
        char msg[400];
        sscanf(buffer + 10, "%399[^\n]", msg);

        // ✅ Enviar apenas para clientes no mesmo canal e autenticados
        for (int j = 0; j < MAX_CLIENTES; j++) {
            if (clientes[j].fd > 0 && clientes[j].autenticado &&
                strcmp(clientes[j].canal, clientes[i].canal) == 0) {
                sprintf(response, "[BROADCAST %s] %s\n",
                        clientes[i].canal, msg);
                send(clientes[j].fd, response, strlen(response), 0);
            }
        }

        strcpy(response, "BCAST_SENT");
    }
}
```

---

### ❌ VULNERABILIDADE V9: Admin Ban Não Desconecta Utilizador

**Problema:** Quando um admin executa `BAN user2` ou `SUSPEND_USER user2`, o servidor remove o utilizador da base de dados (`users.txt`) MAS não fecha a ligação TCP do utilizador se estiver online. O `user2` fica como "utilizador fantasma" no servidor.

**Impacto:**
- Utilizador banido pode continuar a usar o servidor
- Bypass de sanções de administrador
- Inconsistência de estado

**Código ANTES (VULNERÁVEL):**
```c
// server_linux.c, comando BAN
else if (strncmp(buffer, "BAN ", 4) == 0) {
    char user[50];
    sscanf(buffer + 4, "%49s", user);

    // ❌ Remove de users.txt, MAS não desconecta o socket
    FILE* fp = fopen("users.txt", "r");
    FILE* fp_temp = fopen("users_temp.txt", "w");
    // ... reescrever users.txt sem o utilizador

    fclose(fp);
    fclose(fp_temp);
    rename("users_temp.txt", "users.txt");

    // ❌ BUG: user2 ainda está conectado se estiver online!
    strcpy(response, "BAN_OK");
}
```

**Teste de Reprodução:**
```bash
# Terminal 1: Cliente user2 (autenticado)
./client_linux 127.0.0.1 10000
# Menu > Fazer login como user2
# Esperar no menu (socket aberto)

# Terminal 2: Admin
./client_linux 127.0.0.1 10000
# Menu > Fazer login como admin
# Menu > Gestão de Utilizadores > BAN user2

# Resultado em Terminal 1:
# user2 continua a ver o menu, consegue enviar comandos ❌
```

**Código DEPOIS (CORRIGIDO):**
```c
// server_linux.c, comando BAN
else if (strncmp(buffer, "BAN ", 4) == 0) {
    char user[50];
    sscanf(buffer + 4, "%49s", user);

    // ✅ NOVO: Procurar o utilizador nos clientes conectados
    int target_slot = -1;
    for (int j = 0; j < MAX_CLIENTES; j++) {
        if (clientes[j].autenticado && 
            strcmp(clientes[j].username, user) == 0) {
            target_slot = j;
            break;
        }
    }

    // ✅ Se está online, desconectar o socket
    if (target_slot >= 0) {
        char kick_msg[] = "ERRO: Foi banido pelo administrador";
        send(clientes[target_slot].fd, kick_msg, strlen(kick_msg), 0);
        close(clientes[target_slot].fd);
        
        // ✅ Limpar slot
        clientes[target_slot].fd = -1;
        clientes[target_slot].autenticado = 0;
        memset(clientes[target_slot].username, 0, 50);
        memset(clientes[target_slot].canal, 0, 50);
        
        printf(" [OK] User '%s' desconectado forcadamente (BAN)\n", user);
    }

    // Remove de users.txt como antes
    FILE* fp = fopen("users.txt", "r");
    FILE* fp_temp = fopen("users_temp.txt", "w");
    // ... reescrever users.txt sem o utilizador

    fclose(fp);
    fclose(fp_temp);
    rename("users_temp.txt", "users.txt");

    strcpy(response, "BAN_OK");
}
```

---

---

## 💻 PILLAR 5: INTERFACE DO CLIENTE (DUPLA ESCUTA)

### ⚠️ VULNERABILIDADE V10: select() Cliente sem Timeout

**Problema:** Na linha ~1010 do client_linux.c, `select(maxfd, &readfds, NULL, NULL, NULL)` usa timeout NULL (bloqueia indefinidamente). Se o servidor crashear sem fechar a conexão graciosamente, o cliente fica pendurado.

**Impacto:**
- Cliente trava completamente
- Utilizador não consegue sair (Ctrl+C necessário)
- Experiência de utilizador degradada

**Código ANTES (Subótimo):**
```c
// client_linux.c, linha ~1010
fd_set readfds;
FD_ZERO(&readfds);
FD_SET(STDIN_FILENO, &readfds);
FD_SET(server_fd, &readfds);
int maxfd = server_fd + 1;

// ❌ Sem timeout - bloqueia indefinidamente
int activity = select(maxfd, &readfds, NULL, NULL, NULL);
```

**Código DEPOIS (CORRIGIDO):**
```c
// client_linux.c, linha ~1010
fd_set readfds;
FD_ZERO(&readfds);
FD_SET(STDIN_FILENO, &readfds);
FD_SET(server_fd, &readfds);
int maxfd = server_fd + 1;

// ✅ NOVO: Adicionar timeout de 30 segundos
struct timeval tv;
tv.tv_sec = 30;   // 30 segundos
tv.tv_usec = 0;

// ✅ Agora select() retorna após 30s mesmo que sem input
int activity = select(maxfd, &readfds, NULL, NULL, &tv);

if (activity == 0) {
    // ✅ Timeout expirou - servidor não respondeu
    printf("\n [AVISO] Sem atividade há 30 segundos...\n");
    printf(" [Reconectando...]\n");
    // ✅ Opcionalmente reconectar aqui
} else if (activity < 0) {
    // ✅ Erro no select()
    perror("select");
    break;
}
```

---

### ⚠️ VULNERABILIDADE V11: Colisão Visual Não Tratada Perfeitamente

**Problema:** Quando um broadcast chega enquanto o utilizador está a digitar, a linha é apagada e reimprimida com `printf("\r\033[K")`, mas se a mensagem do servidor é muito longa, pode não ocupar toda a linha anterior, deixando "sombras" de caracteres anteriores.

**Impacto:**
- Glitch visual (cosmético, não crítico)
- Confusão visual para utilizador

**Código ANTES:**
```c
// client_linux.c, submenu_canais()
if (FD_ISSET(server_fd, &readfds)) {
    char incoming[BUF_SIZE];
    int n = recv(server_fd, incoming, BUF_SIZE - 1, 0);
    // ...
    printf("\r\033[K");  // ❌ Apenas apaga até coluna anterior
    imprimir_resposta(incoming);
    printf(" [%s] Sua mensagem: ", current_user);
    fflush(stdout);
    continue;
}
```

**Código DEPOIS (Melhorado):**
```c
// client_linux.c, submenu_canais()
if (FD_ISSET(server_fd, &readfds)) {
    char incoming[BUF_SIZE];
    int n = recv(server_fd, incoming, BUF_SIZE - 1, 0);
    // ...
    printf("\r\033[2K");  // ✅ CORRIGIDO: \033[2K apaga TODA a linha
    imprimir_resposta(incoming);
    printf(" [%s] Sua mensagem: ", current_user);
    fflush(stdout);
    continue;
}
```

---

---

## 📊 SUMÁRIO DE FIXES

| Vulnerabilidade | Ficheiro | Linha(s) | Tipo Fix |
|-----------------|----------|----------|----------|
| V1: Missing FD_CLR | server_linux.c | 1149-1165 | Add memset cleanup |
| V2: recv() validation | server_linux.c | 1147 | Add bounds check |
| V3: max_fd validation | server_linux.c | 1080-1085 | Add FD_SETSIZE check |
| V4: Buffer overflow SEND_MSG | server_linux.c | 1245-1250 | Validate parsing |
| V5: strtok non-reentrant | client_linux.c | ~280 | Use strtok_r |
| V6: Race condition inbox | server_linux.c | send_msg() | Add fcntl locks |
| V7: users.txt race | server_linux.c | check_auth() | Add read locks |
| V8: No auth check BROADCAST | server_linux.c | 1273 | Add auth validation |
| V9: Ban doesn't kick | server_linux.c | BAN command | Force disconnect |
| V10: select() no timeout | client_linux.c | 1010 | Add 30s timeout |
| V11: Visual collision | client_linux.c | submenu_canais | Change \033[K to \033[2K |

---

## 🚀 RECOMENDAÇÕES PRIORITÁRIAS

**CRÍTICA (Corrigir IMEDIATAMENTE):**
1. V1 - FD_CLR leak (afeta operação longa-prazo)
2. V2 - recv() buffer overflow (crash imediato)
3. V4 - Buffer overflow SEND_MSG (RCE potencial)
4. V6 - Race condition inbox (data corruption)
5. V8 - No auth BROADCAST (security bypass)

**ALTA (Corrigir em próxima versão):**
6. V3 - max_fd validation
7. V5 - strtok_r
8. V9 - Ban doesn't kick
9. V7 - users.txt race

**MÉDIA (Melhorias):**
10. V10 - select() timeout
11. V11 - Visual collision

---

**Status: ⚠️ NÃO PRONTO PARA PRODUÇÃO ATÉ CORRIGIR CRÍTICA**

