# EVOLUÇÃO DA ARQUITETURA — C-CORD

**Projeto:** C-Cord (Clone de Chat TCP)  
**Fase Atual:** Etapa 3 (I/O Multiplexing & Canais)  
**Autor:** [O Teu Nome/Mestrado em Cibersegurança]

---

## 1. O Paradigma da Fase 1 e 2: Stop-and-Wait

Nas etapas iniciais do C-Cord, a arquitetura de rede baseava-se num modelo **Sequencial (Stop-and-Wait) e Bloqueante**.
Neste modelo, o servidor processava os clientes de forma perfeitamente síncrona:

1. O servidor bloqueava na chamada `accept()`.
2. Um cliente conectava-se, o servidor aceitava e bloqueava em `recv()`.
3. O cliente enviava a mensagem, recebia a resposta, e o servidor **fechava a ligação** (`close(client_fd)`).

**Problema de Escalabilidade:**  
A "Conexão Efémera" obrigava ao custo constante de realizar o _Three-way Handshake_ do TCP para cada comando individual (ex: listar utilizadores, enviar mensagem). Além disso, enquanto o servidor processava o Cliente A, o Cliente B ficava em espera. A comunicação assíncrona bidirecional (como um chat em tempo real) era arquiteturalmente impossível.

## 2. A Revolução da Fase 3: I/O Multiplexing com `select()`

Para ultrapassar os bloqueios sem recorrer a complexidade de paralelismo puro (como _Multi-Threading_ ou _Multi-Processing_ com `fork()`, que introduzem difíceis _Race Conditions_), a Etapa 3 implementou a **Multiplexação de Entrada/Saída com `select()`**.

### O Paradigma da Ligação Persistente

Os sockets dos clientes deixaram de ser fechados após o primeiro comando. O `server_fd` do cliente mantém-se aberto durante toda a navegação pelos menus (Sessão).

### O Modelo de Concorrência

O `select()` funciona como um "vigilante" central. O servidor regista todos os sockets abertos numa lista (`fd_set`). Quando chama `select()`, o servidor "dorme" apenas até que **qualquer um** dos sockets tenha dados prontos a ser lidos, ou que ocorra uma nova conexão no socket passivo.

Isto permite gerir dezenas de clientes simultaneamente num único processo e numa única thread (_Single-threaded Asynchronous I/O_).

### Gestão de Estado no Servidor

Para sustentar estas conexões longas, foi introduzida a estrutura estática global:

```c
Cliente clientes[MAX_CLIENTES];
```

Esta estrutura atua como uma tabela de sessões. Quando o `recv()` retorna `0` ou `-1` (sinalizando a desconexão ou quebra do TCP), o servidor liberta o slot (colocando `fd = -1`), higieniza os campos e assegura que não ocorrem _Memory Leaks_ ou fugas de descritores (embora o processo de limpeza rigorosa do `FD_SET` deva ser uma prioridade na auditoria de segurança).

## 3. O Desafio do Cliente: "A Dupla Escuta"

A funcionalidade de Chat em Canais introduziu um enorme desafio arquitetural do lado do cliente: **Como escutar o teclado (para digitar mensagens) e a rede (para receber Broadcasts) ao mesmo tempo?**

No modelo clássico, funções como `scanf()` ou `fgets()` bloqueiam totalmente o programa no terminal (em `STDIN_FILENO`), impedindo o cliente de ver mensagens recebidas (TCP Socket) enquanto não carregar no ENTER.

**A Solução Técnica:**
Foi aplicado o `select()` também no lado do cliente. O descritor padrão do teclado (`STDIN_FILENO = 0`) foi adicionado ao mesmo `fd_set` que o `server_fd` (Socket).

Desta forma, o cliente bloqueia no `select()`, aguardando por uma de duas coisas:

1. **Atividade no STDIN:** O utilizador digitou algo e pressionou ENTER. O cliente procede para leitura com `fgets()` e envia para o servidor sem risco de bloqueio, retornando instantaneamente.
2. **Atividade no Socket:** Chegou um `BROADCAST` do servidor. O cliente limpa dinamicamente a linha atual do terminal (usando Códigos ANSI `\033[K`), imprime a mensagem recebida e redesenha o _prompt_, criando uma ilusão perfeita de Chat Assíncrono em Tempo Real.
