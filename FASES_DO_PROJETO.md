# Fases do Projeto C-Cord (Evolução da Arquitetura)

Este documento descreve detalhadamente a evolução do projeto **C-Cord**, desde a sua concepção básica até à versão final **v4.0**, que implementa segurança criptográfica de Ponta-a-Ponta (E2EE). Cada fase introduziu novos conceitos de Redes de Computadores, Sistemas Operativos e Criptografia.

---

## Fase 1: Conexão Básica TCP (Modelo Request-Reply)
A primeira iteração do projeto focou-se no estabelecimento de uma ponte de comunicação simples e confiável entre duas máquinas.

### O que foi implementado:
- **Sockets TCP Básico**: Uso de `socket()`, `bind()`, `listen()` e `accept()` no servidor, e `connect()` no cliente.
- **Transmissão Única**: O cliente conectava-se, enviava uma única mensagem em texto limpo (`send()`), recebia o eco do servidor (`recv()`), e a conexão era fechada imediatamente (`close()`).
- **Problema Resolvido**: Como transpor dados entre processos independentes numa rede (Modelo OSI - Camada de Transporte).

### Referência no Código Atual:
Embora o modelo de fechar a ligação imediatamente tenha sido removido, a fundação TCP criada nesta fase permanece viva no início da função `main()` de ambos os executáveis, onde o *Master Socket* é inicializado e ligado ao porto `10000`.

---

## Fase 2: Conexões Persistentes e Comandos Sequenciais
Uma aplicação de chat real não pode criar e destruir a conexão a cada mensagem. Nesta fase, o projeto evoluiu para manter as ligações abertas (Persistência).

### O que foi implementado:
- **Loop Infinito de Leitura**: O cliente entrava num ciclo `while(1)`, capturando input via `scanf`/`fgets`, e enviando ao servidor. O servidor processava e enviava a resposta.
- **Protocolo Textual (Protocolo C-Cord)**: Criação de comandos normalizados (ex: `AUTH`, `REGISTER`, `LIST_ALL`).
- **Gestão de Estado Básico**: O servidor começou a usar ficheiros (`users.txt` e `inbox.txt`) para validar logins e guardar mensagens offline.
- **Limitação Crítica**: Sendo um modelo **sequencial bloqueante**, se o servidor estivesse a aguardar um comando (`recv()`) do Cliente A, todos os outros clientes (B, C, D) ficavam bloqueados em espera. O Cliente A monopolizava o servidor.

---

## Fase 3: Multiplexação com `select()` e Canais (Chat Real-Time)
Esta foi a mudança arquitetural mais profunda do projeto. A transição de um modelo bloqueante para um modelo reativo de I/O Multiplexado.

### O que foi implementado:
- **O coração `select()` no Servidor**: O servidor abandonou o bloqueio num único cliente. Agora, regista o *Master Socket* e os *Sockets* de todos os clientes ligados no descritor de ficheiros (`fd_set`), e usa a chamada de sistema `select()` para "adormecer" até que *qualquer* cliente tenha dados prontos a ser lidos.
- **Dupla Escuta no Cliente**: O cliente também adotou o `select()`. Passou a monitorizar simultaneamente o Teclado (`STDIN_FILENO`) e a placa de rede (`server_fd`). Isto permite que o cliente receba mensagens do servidor *enquanto* o utilizador está a escrever a sua própria mensagem, sem interrupções.
- **Canais de Chat e Broadcast**: Com a multiplexação resolvida, foi implementada a lógica de grupos (`#geral`, `#linux`). O comando `BROADCAST` itera pelo array de clientes e envia a mensagem apenas àqueles que estão no mesmo canal.

### Código Relevante:
- `server_linux.c`: O *Loop Principal* da função `main()` (linhas 1257 a 1369) e a função de despacho retransmissor `handle_broadcast()`.
- `client_linux.c`: A função `submenu_canais()` (linhas 997 a 1179), onde ocorre a maravilha técnica de limpar a consola via ANSI (`\r\033[K`) para injetar a mensagem recebida sem quebrar o texto que o utilizador está a digitar.

---

## Fase 4: Segurança de Ponta-a-Ponta (E2EE) e Criptografia
Com as comunicações funcionais e em tempo real, a última fase blindou a rede contra escutas clandestinas (*Sniffing*) e abusos de acesso, transformando o C-Cord num sistema *Zero-Trust*.

### O que foi implementado:
- **Hashing de Palavras-Passe**: Substituição de passwords em texto limpo pelo algoritmo **DJB2**. As passwords não trafegam mais na rede.
- **Toy RSA**: O Hash gerado é cifrado com a chave pública do servidor (`RSA_E`) antes de ser enviado. O servidor valida a autenticação puramente por matemática cruzada (`check_auth_hash`).
- **Diffie-Hellman (DH)**: Implementação de um Key-Exchange seguro. Cliente e Servidor geram chaves privadas aleatórias e negociam uma **Chave de Sessão** simétrica através do canal inseguro, sem nunca transmitir as chaves privadas.
- **Cifra de César Dinâmica (E2EE)**: As mensagens enviadas para os canais de chat são localmente cifradas no lado do cliente utilizando a chave de sessão DH acordada. O servidor recebe o texto ilegível, não o consegue decifrar, e reencaminha-o (Broadcast) de forma cega. O cliente recetor efetua a decifragem.
- **Segurança contra Overflows**: Criação do motor matemático `exponenciacao_modular` com suporte interno a ponteiros lógicos de 128-bits (`__int128`), garantindo que a exponenciação e módulo dos algoritmos de criptografia não quebram em C.

### Código Relevante:
- **Motor Criptográfico**: Ambos os ficheiros possuem um bloco superior de funções chamado "FUNÇÕES CRIPTOGRÁFICAS — ETAPA 4" contendo as funções matemáticas de cifragem, *hash* e exp. modular.
- `client_linux.c`: `iniciar_diffie_hellman()` e o processo interno de cifrar a mensagem com `cifrar_cesar(input, chave_sessao)` antes do `send()`.
- `server_linux.c`: O handler para o comando `DH_EXCHANGE` (onde o servidor gera a sua metade pública B e devolve) e o comando de administração `VIEW_CRYPTO`.

---
*Ficheiro de Documentação atualizado para v4.0*
