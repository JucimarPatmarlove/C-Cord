# 🧠 O Código da Etapa 3 Explicado

**Autor:** Jucimar Cabral Costa — nº 1012639 

Este documento detalha **exclusivamente** os mecanismos de código introduzidos e revistos na **Etapa 3** do projeto C-Cord. O grande salto tecnológico desta etapa foi a transição de um modelo sequencial bloqueante para um modelo de **I/O Multiplexing (Multiplexação)** utilizando a chamada de sistema `select()`.

---

## 1. O Problema das Etapas Anteriores

Nas versões iniciais, o servidor usava um fluxo síncrono:

1. `accept()` (bloqueia até alguém conectar)
2. `recv()` (bloqueia até o cliente enviar dados)
3. `send()` e `close()` (responde e desliga).

Isto impedia que dois clientes conversassem em simultâneo. A solução? O uso da função `select()`.

A evolução da arquitetura do projeto seguiu este fio condutor:

|   Etapa   | Modelo                   | Comando chave                       |
| :-------: | :----------------------- | :---------------------------------- |
| **1 e 2** | Sequencial bloqueante    | `accept()` → `recv()` → `close()`   |
|   **3**   | Multiplexado persistente | `select()` → `recv()` → (sem close) |

---

## 2. A Magia no Servidor (`server_linux.c`)

### A Estrutura de Estado (Ligações Persistentes) 📍 _[server_linux.c : ~Linha 78]_

Ao invés de fechar a ligação a cada comando, o servidor agora **guarda o estado** de cada conexão ativa na estrutura global:

```c
typedef struct {
    int fd;            /* Descritor do socket (Ex: 4, 5, 6). -1 se vazio. */
    char username[50]; /* Quem está autenticado neste socket */
    char canal[50];    /* Canal onde está a ouvir broadcasts (#geral) */
    int autenticado;   /* Flag: 1 = Autenticado */
} Cliente;

Cliente clientes[MAX_CLIENTES]; /* Array com 50 slots disponíveis */
```

Esta estrutura é o que permite o conceito de "Sessão". O socket fica aberto.

### O Loop Principal com `select()` 📍 _[server_linux.c -> Função `main()` : ~Linha 1211]_

O coração da Etapa 3 no servidor reside no loop `while(1)` principal. Começamos por preparar os descritores:

```c
fd_set readfds;         /* Cria um mapa de bits para os descritores */
FD_ZERO(&readfds);      /* Limpa a lista a zeros */
FD_SET(server_fd, &readfds); /* Adiciona o Master Socket à lista */
```

O `server_fd` é o socket que "escuta" a porta 10000. Depois, iteramos o array de clientes e adicionamos os que já estão conectados:

```c
for (int i = 0; i < MAX_CLIENTES; i++) {
    if (clientes[i].fd > 0) {
        FD_SET(clientes[i].fd, &readfds); /* Adiciona os sockets ativos à lista */
    }
}
```

E então, a chamada que faz a "magia" assíncrona:

```c
int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
```

O `select()` **adormece** o servidor. Ele não consome CPU. Ele só acorda se:

1. Alguém tentar uma nova ligação (O `server_fd` reporta atividade).
2. Um cliente ativo enviar um comando ou desconectar (O `client_fd` reporta atividade).

Se for o `server_fd`, fazemos `accept()` e guardamos num slot vazio do array `clientes`. Se for um `client_fd`, fazemos `recv()`, processamos a string recebida, enviamos a resposta e **não fechamos a ligação**!

---

## 3. O Desafio no Cliente (`client_linux.c`)

### A "Dupla Escuta" no Chat em Tempo Real

O maior desafio no cliente é que uma função como `fgets()` bloqueia o programa à espera que o utilizador digite algo. Se o programa está bloqueado, como pode receber a mensagem de outra pessoa que chegou da rede entretanto?

A Etapa 3 resolve isto replicando o modelo `select()` no cliente, na função `submenu_canais()` 📍 _[client_linux.c : ~Linha 1011]_:

```c
fd_set readfds;
FD_ZERO(&readfds);
FD_SET(STDIN_FILENO, &readfds); /* 0 = Teclado (Standard Input) */
FD_SET(server_fd, &readfds);    /* Socket que liga ao servidor */

select(maxfd, &readfds, NULL, NULL, NULL);
```

O cliente fica a escutar o teclado E a placa de rede **ao mesmo tempo**.

### Cenário A: Chega Mensagem da Rede

```c
if (FD_ISSET(server_fd, &readfds)) {
    /* Recebe a mensagem do servidor */
    recv(server_fd, incoming, BUF_SIZE - 1, 0);

    /* O truque visual (Colisão de Ecrã):
       '\r' volta ao início da linha onde o utilizador escrevia.
       '\033[K' apaga essa linha inteira do ecrã. */
    printf("\r\033[K");

    /* Imprime a mensagem que chegou */
    imprimir_resposta(incoming);

    /* Reimprime o prompt para o utilizador continuar a escrever */
    printf(" [%s] Sua mensagem: ", current_user);
}
```

### Cenário B: O Utilizador preme ENTER

```c
if (FD_ISSET(STDIN_FILENO, &readfds)) {
    /* Agora é 100% seguro usar fgets porque SABEMOS que o SO tem dados no buffer do teclado */
    fgets(input, sizeof(input), stdin);
    /* Envia o comando BROADCAST para o servidor... */
}
```

---

## 4. Conclusão da Revisão

Com este código, a Etapa 3 consagra a arquitetura C-Cord como um servidor TCP de escala moderada, eficiente (Single-Threaded Asynchronous I/O), sem _race-conditions_, e que suporta salas de conversação fluídas, superando as pesadas limitações do padrão síncrono POSIX.
