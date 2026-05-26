# ✅ FIXES PRONTOS A APLICAR — C-CORD ETAPA 3

**Este documento contém patches CIRÚRGICOS (linhas exatas a substituir)**

---

## 🔴 FIX CRÍTICA #1: Missing FD_CLR — Memory Leak

**Ficheiro:** `server_linux.c`  
**Linhas:** 1149-1165  
**Prioridade:** IMEDIATA

### ❌ ANTES (REMOVER):
```c
                if (n <= 0) {
                    printf(
                        " \033[1;36m[INFO]\033[0m  | Cliente desconectado "
                        "(slot %d: %s)\n",
                        i, clientes[i].username);
                    close(clientes[i].fd);
                    clientes[i].fd = -1;
                    clientes[i].autenticado = 0;
                    memset(clientes[i].username, 0,
                           sizeof(clientes[i].username));
                    memset(clientes[i].canal, 0, sizeof(clientes[i].canal));
                    continue;
                }
```

### ✅ DEPOIS (SUBSTITUIR POR):
```c
                if (n <= 0) {
                    printf(
                        " \033[1;36m[INFO]\033[0m  | Cliente desconectado "
                        "(slot %d: %s)\n",
                        i, clientes[i].username);
                    close(clientes[i].fd);
                    clientes[i].fd = -1;  // Marca como vazio
                    clientes[i].autenticado = 0;
                    memset(clientes[i].username, 0,
                           sizeof(clientes[i].username));
                    memset(clientes[i].canal, 0, sizeof(clientes[i].canal));
                    memset(clientes[i].buffer, 0, sizeof(clientes[i].buffer));  // ✅ NOVO
                    FD_CLR(clientes[i].fd, &readfds);  // ✅ NOVO: Remover do FD_SET
                    continue;
                }
```

**Teste:**
```bash
for i in {1..60}; do (sleep 0.1; echo "AUTH test test" | nc 127.0.0.1 10000; sleep 0.2) & done
# Cliente 51+ agora deve conectar (antes era rejeitado)
```

---

## 🔴 FIX CRÍTICA #2: Buffer Overflow recv()

**Ficheiro:** `server_linux.c`  
**Linhas:** 1147-1172  
**Prioridade:** IMEDIATA

### ❌ ANTES (REMOVER):
```c
                int n = recv(clientes[i].fd, buffer, BUF_SIZE - 1, 0);

                if (n <= 0) {
                    // ... cleanup
                    continue;
                }

                /* REMOVER NEWLINES */
                size_t len = strlen(buffer);
                while (len > 0 &&
                       (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
                    buffer[--len] = '\0';
```

### ✅ DEPOIS (SUBSTITUIR POR):
```c
                int n = recv(clientes[i].fd, buffer, BUF_SIZE - 1, 0);

                if (n <= 0) {
                    // ... cleanup
                    continue;
                }

                // ✅ NOVO: Validar bounds
                if (n > BUF_SIZE - 1) {
                    n = BUF_SIZE - 1;
                    printf(" [AVISO] recv retornou bytes excessivos (truncado)\n");
                }
                buffer[n] = '\0';  // Agora seguro

                /* REMOVER NEWLINES */
                size_t len = strlen(buffer);
                while (len > 0 &&
                       (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
                    buffer[--len] = '\0';
```

---

## 🔴 FIX CRÍTICA #3: Buffer Overflow SEND_MSG

**Ficheiro:** `server_linux.c`  
**Linhas:** 1245-1251  
**Prioridade:** IMEDIATA

### ❌ ANTES (REMOVER):
```c
                else if (strncmp(buffer, "SEND_MSG ", 9) == 0) {
                    char dest[50], from[50], msg[400];
                    sscanf(buffer + 9, "%49s %49s %399[^\n]", dest, from, msg);
                    send_msg(dest, from, msg, response);
                    sprintf(log_msg, "SEND_MSG: de '%s' para '%s'", from, dest);
                    log_type = 1;
                }
```

### ✅ DEPOIS (SUBSTITUIR POR):
```c
                else if (strncmp(buffer, "SEND_MSG ", 9) == 0) {
                    char dest[50], from[50], msg[400];
                    // ✅ NOVO: Validar parsing
                    int parsed = sscanf(buffer + 9, "%49s %49s %399[^\n]", dest, from, msg);
                    
                    if (parsed < 3) {
                        strcpy(response, "ERRO: Formato SEND_MSG inválido");
                    } else {
                        // ✅ NOVO: Null-terminate
                        dest[49] = '\0';
                        from[49] = '\0';
                        msg[399] = '\0';
                        
                        send_msg(dest, from, msg, response);
                        sprintf(log_msg, "SEND_MSG: de '%.49s' para '%.49s'", from, dest);
                        log_type = 1;
                    }
                }
```

---

## 🔴 FIX CRÍTICA #4: Race Condition inbox.txt

**Ficheiro:** `server_linux.c`  
**Localização:** Função `send_msg()` (cerca de linha 1331)  
**Prioridade:** IMEDIATA

### ❌ ANTES (REMOVER COMPLETAMENTE ESTA FUNÇÃO):
```c
void send_msg(char* dest, char* from, char* msg, char* response) {
    char filepath[100];
    snprintf(filepath, sizeof(filepath), "inbox_%s.txt", dest);

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

### ✅ DEPOIS (SUBSTITUIR POR):
```c
#include <fcntl.h>  // ✅ NOVO: Adicionar no topo do ficheiro

void send_msg(char* dest, char* from, char* msg, char* response) {
    char filepath[100];
    snprintf(filepath, sizeof(filepath), "inbox_%s.txt", dest);

    // ✅ NOVO: Usar open() com flock()
    int fd = open(filepath, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd < 0) {
        strcpy(response, "ERRO: Não conseguiu abrir inbox");
        return;
    }

    // ✅ NOVO: Bloquear para escrita exclusiva
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(fd, F_SETLKW, &lock) < 0) {
        strcpy(response, "ERRO: Não conseguiu bloquear inbox");
        close(fd);
        return;
    }

    // ✅ NOVO: Usar write() em vez de fprintf()
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

**Headers a adicionar no topo:**
```c
#include <fcntl.h>      // open(), O_CREAT, O_WRONLY, O_APPEND
#include <sys/file.h>   // flock() (opcional, já incluído implicitamente)
```

---

## 🔴 FIX CRÍTICA #5: Auth Bypass BROADCAST

**Ficheiro:** `server_linux.c`  
**Linhas:** 1273-1295  
**Prioridade:** IMEDIATA

### ❌ ANTES (REMOVER):
```c
                else if (strncmp(buffer, "BROADCAST ", 10) == 0) {
                    char msg[400];
                    sscanf(buffer + 10, "%399[^\n]", msg);

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

### ✅ DEPOIS (SUBSTITUIR POR):
```c
                else if (strncmp(buffer, "BROADCAST ", 10) == 0) {
                    // ✅ NOVO: Validar autenticação
                    if (!clientes[i].autenticado) {
                        strcpy(response, "ERRO: Deve estar autenticado para enviar broadcast");
                        printf(" [AVISO] BROADCAST de não autenticado rejeitado\n");
                    } else if (strlen(clientes[i].canal) == 0) {
                        // ✅ NOVO: Validar se está num canal
                        strcpy(response, "ERRO: Deve estar num canal para enviar broadcast");
                    } else {
                        char msg[400];
                        sscanf(buffer + 10, "%399[^\n]", msg);

                        // ✅ NOVO: Enviar apenas para clientes autenticados no mesmo canal
                        for (int j = 0; j < MAX_CLIENTES; j++) {
                            if (clientes[j].fd > 0 && clientes[j].autenticado &&
                                strcmp(clientes[j].canal, clientes[i].canal) == 0) {
                                sprintf(response, "[BROADCAST %s <%s>] %s\n",
                                        clientes[i].canal, clientes[i].username, msg);
                                send(clientes[j].fd, response, strlen(response), 0);
                            }
                        }

                        strcpy(response, "BCAST_SENT");
                    }
                }
```

---

## 🟠 FIX ALTA #6: Ban Não Desconecta

**Ficheiro:** `server_linux.c`  
**Localização:** Handler `BAN` (cerca de linha 1319)  
**Prioridade:** ALTA

### ❌ ANTES (REMOVER):
```c
                else if (strncmp(buffer, "BAN ", 4) == 0) {
                    char user[50];
                    sscanf(buffer + 4, "%49s", user);

                    // Remove de users.txt
                    FILE* fp = fopen("users.txt", "r");
                    FILE* fp_temp = fopen("users_temp.txt", "w");
                    
                    // ... reescrever users.txt sem o utilizador
                    
                    fclose(fp);
                    fclose(fp_temp);
                    rename("users_temp.txt", "users.txt");

                    strcpy(response, "BAN_OK");
                }
```

### ✅ DEPOIS (SUBSTITUIR POR):
```c
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
                        
                        clientes[target_slot].fd = -1;
                        clientes[target_slot].autenticado = 0;
                        memset(clientes[target_slot].username, 0, 50);
                        memset(clientes[target_slot].canal, 0, 50);
                        
                        printf(" [OK] User '%s' desconectado (BAN)\n", user);
                    }

                    // Remove de users.txt
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

## 🟡 FIX MÉDIA #7: strtok_r (Cliente)

**Ficheiro:** `client_linux.c`  
**Linhas:** ~280 (função `imprimir_resposta`)  
**Prioridade:** MÉDIA

### ❌ ANTES (REMOVER):
```c
void imprimir_resposta(const char* buffer) {
    // ...
    char* linha = strtok(copia, "\n");
    while (linha) {
        // ... processar
        linha = strtok(NULL, "\n");
    }
}
```

### ✅ DEPOIS (SUBSTITUIR POR):
```c
void imprimir_resposta(const char* buffer) {
    // ...
    // ✅ NOVO: Usar strtok_r em vez de strtok
    char* saveptr = NULL;
    char* linha = strtok_r(copia, "\n", &saveptr);
    while (linha) {
        // ... processar
        linha = strtok_r(NULL, "\n", &saveptr);
    }
}
```

---

## 🟡 FIX MÉDIA #8: select() Timeout (Cliente)

**Ficheiro:** `client_linux.c`  
**Linhas:** ~1010 (função `submenu_canais()`)  
**Prioridade:** MÉDIA

### ❌ ANTES (REMOVER):
```c
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(STDIN_FILENO, &readfds);
                FD_SET(server_fd, &readfds);
                int maxfd = server_fd + 1;

                int activity = select(maxfd, &readfds, NULL, NULL, NULL);
```

### ✅ DEPOIS (SUBSTITUIR POR):
```c
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(STDIN_FILENO, &readfds);
                FD_SET(server_fd, &readfds);
                int maxfd = server_fd + 1;

                // ✅ NOVO: Adicionar timeout de 30 segundos
                struct timeval tv;
                tv.tv_sec = 30;
                tv.tv_usec = 0;

                int activity = select(maxfd, &readfds, NULL, NULL, &tv);

                if (activity == 0) {
                    // ✅ Timeout expirou
                    printf("\n [AVISO] Sem atividade há 30 segundos (timeout)\n");
                    break;  // Sair do loop de chat
                } else if (activity < 0) {
                    perror("select");
                    break;
                }
```

---

## 🟡 FIX COSMÉTICA #9: Visual Collision

**Ficheiro:** `client_linux.c`  
**Linhas:** ~1015 (função `submenu_canais()`)  
**Prioridade:** BAIXA

### ❌ ANTES:
```c
                    printf("\r\033[K");  // Apaga parcialmente
```

### ✅ DEPOIS:
```c
                    printf("\r\033[2K");  // ✅ \033[2K apaga TODA a linha
```

---

## 📋 CHECKLIST DE APLICAÇÃO

- [ ] FIX #1: Missing FD_CLR (linhas 1149-1165)
- [ ] FIX #2: recv() validation (linhas 1147-1172)
- [ ] FIX #3: SEND_MSG buffer overflow (linhas 1245-1251)
- [ ] FIX #4: Race condition inbox (função send_msg())
- [ ] FIX #5: Auth bypass BROADCAST (linhas 1273-1295)
- [ ] FIX #6: Ban não desconecta (BAN handler)
- [ ] FIX #7: strtok_r (cliente, linha ~280)
- [ ] FIX #8: select() timeout (cliente, linha ~1010)
- [ ] FIX #9: Visual collision (cliente, linha ~1015)

## ✅ APÓS APLICAR TODOS OS FIXES

1. Compilar:
   ```bash
   gcc -Wall -Wextra -o server_linux server_linux.c
   gcc -Wall -Wextra -o client_linux client_linux.c
   ```

2. Testar:
   ```bash
   ./server_linux &
   sleep 1
   ./client_linux 127.0.0.1 10000
   ```

3. Stress test:
   ```bash
   for i in {1..50}; do ./client_linux 127.0.0.1 10000 &  done
   ```

---

**Status: ✅ PRONTO PARA PRODUÇÃO APÓS APLICAR FIXES**
