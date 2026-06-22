# C-CORD – Plano de Riscos e Testes (Etapas 1, 2 e 3)

**Versão:** 3.0 (Final – alinhada com o código de Junho de 2026)  
**Data:** 2026-06-11  
**Responsável:** Diego França (Risk & Testing Manager)  
**Unidade Curricular:** Administração e Segurança de Sistemas – ESTG/IPG

---

## 1. Introdução

Este documento integra o **Plano de Riscos** e o **Plano de Testes** para as Etapas 1, 2 e 3 do projecto C-Cord.  
A Etapa 3 introduziu:

- Servidor multiplexado com `select()` (ligações persistentes)
- Canais e broadcast apenas para membros do mesmo canal
- Cliente com `select()` para escuta dupla (stdin + socket) – chat em tempo real
- Melhorias: timestamps nas mensagens, comando `DELETE_USER_ADMIN`, validações extra

Todos os testes foram executados no ambiente de desenvolvimento (Kali Linux, GCC, porta 10000) e os resultados são **100% positivos**.

---

## 2. Identificação da Equipa

| Nome           | Papel              | Responsabilidades na Etapa 3                     |
| -------------- | ------------------ | ------------------------------------------------ |
| Carlos Martins | Team Manager       | Coordenação, planeamento Gantt, apresentações    |
| Diego França   | Risk & Testing Mgr | Este documento – riscos, testes, validação       |
| David Bunga    | Dev Team           | Desenvolvimento geral (F1–F15)                   |
| Jucimar Cabral | Dev Team           | Implementação do servidor/cliente Linux (F3–F10) |

---

## 3. Plano de Riscos

### 3.1 Metodologia

Riscos avaliados por **Probabilidade** (Baixa/Média/Alta) e **Impacto** (Baixo/Médio/Alto).  
Estado: `Resolvido` / `Monitorizado` / `Activo` / `Planeado`.

### 3.2 Matriz de Riscos (actualizada para o código final)

| ID  | Descrição                                 | Prob. | Impacto | Nível | Mitigação                                     | Estado       |
| --- | ----------------------------------------- | ----- | ------- | ----- | --------------------------------------------- | ------------ |
| R1  | Erros na criação de sockets (bind/listen) | Baixa | Alto    | Médio | Testes isolados; SO_REUSEADDR                 | Monitorizado |
| R2  | Protocolo mal definido                    | Baixa | Médio   | Baixo | Documentação; testes unitários                | Resolvido    |
| R3  | Corrupção do ficheiro users.txt           | Média | Alto    | Alto  | Validação com fscanf(); backup manual         | Activo       |
| R4  | Falhas no AUTH                            | Média | Médio   | Médio | Testes unitários a check_auth()               | Resolvido    |
| R5  | Falta de tratamento de erros (crash)      | Média | Alto    | Alto  | Verificação de todos os retornos de função    | Activo       |
| R6  | Input inválido do cliente                 | Alta  | Alto    | Alto  | Uso de sscanf() com limites; BUF_SIZE=4096    | Activo       |
| R7  | Incumprimento de tarefas                  | Baixa | Alto    | Médio | Supervisão semanal; GitHub commits            | Monitorizado |
| R8  | Falta de comunicação                      | Baixa | Alto    | Médio | Canal Discord; reuniões                       | Monitorizado |
| R9  | Atrasos nas entregas                      | Média | Médio   | Médio | Gantt; divisão clara de tarefas               | Monitorizado |
| R10 | Conflitos de merge                        | Média | Médio   | Médio | Branches separadas; merge pelo Team Manager   | Activo       |
| R11 | Formato inconsistente do users.txt        | Média | Alto    | Alto  | Formato fixo: ID:user:pass:ROLE:STATUS        | Resolvido    |
| R12 | Servidor não suporta múltiplos clientes   | Alta  | Alto    | Alto  | **Resolvido com select() na Etapa 3**         | Resolvido    |
| R13 | Race condition no broadcast               | Média | Alto    | Alto  | select() single-thread elimina races          | Resolvido    |
| R14 | Cliente bloqueia ao receber broadcast     | Alta  | Alto    | Alto  | **select() duplo (stdin+socket) no cliente**  | Resolvido    |
| R15 | Passwords em texto simples                | Alta  | Alto    | Alto  | **Planeado para Etapa 4 (hashing/cifra)**     | Planeado     |
| R16 | Perda de ligação persistente              | Baixa | Alto    | Médio | Timeout configurável; reconexão manual        | Planeado     |
| R17 | Esgotamento de descritores (MAX_CLIENTES) | Baixa | Médio   | Baixo | Limite de 50 clientes; servidor rejeita acima | Resolvido    |

---

## 4. Plano de Testes

### 4.1 Ambiente de Teste

- **Sistema:** Kali Linux Rolling (kernel 6.18+)
- **Compilador:** GCC 11.4 com flags `-Wall -Wextra`
- **Rede:** 127.0.0.1:10000 (loopback)
- **Ficheiros:** `users.txt`, `inbox.txt`, `logs.txt` no directório do servidor
- **Ferramentas auxiliares:** `netcat-openbsd`, múltiplos terminais

### 4.2 Resumo dos Resultados

| Categoria               | Total | Sucesso |
| ----------------------- | ----- | ------- |
| Testes Etapa 1 (F3-F4)  | 8     | 8 ✅    |
| Testes Etapa 2 (F5-F8)  | 20    | 20 ✅   |
| Testes Etapa 3 (F9-F10) | 18\*  | 18 ✅   |
| Testes de Integração    | 5     | 5 ✅    |

> _Na Etapa 3 foram adicionados testes específicos para `DELETE_USER_ADMIN`, timestamps e persistência de canal._

### 4.3 Testes da Etapa 3 (F9–F10) – Versão final

| ID  | Func. | Descrição                                | Input                              | Resultado Esperado                                                    | Resultado Obtido  | Estado |
| --- | ----- | ---------------------------------------- | ---------------------------------- | --------------------------------------------------------------------- | ----------------- | ------ |
| T29 | F10   | JOIN canal pré-definido                  | `JOIN #geral`                      | `JOIN_OK: Entrou no canal #geral`                                     | Idem              | ✅ OK  |
| T30 | F10   | JOIN canal personalizado                 | `JOIN #dev`                        | `JOIN_OK: Entrou no canal #dev`                                       | Idem              | ✅ OK  |
| T31 | F10   | JOIN sem autenticação                    | `JOIN #geral` (após logout)        | `ERRO: Deve estar autenticado`                                        | Idem              | ✅ OK  |
| T32 | F9    | BROADCAST no canal                       | `BROADCAST Olá`                    | `BCAST_SENT`; outros no canal recebem `[HH:MM:SS] [#canal] user: Olá` | Conforme esperado | ✅ OK  |
| T33 | F9    | BROADCAST sem canal activo               | `BROADCAST msg` (sem JOIN)         | `BCAST_FAIL: sem canal`                                               | Idem              | ✅ OK  |
| T34 | F9    | Remetente não recebe o próprio broadcast | (Cliente envia BROADCAST)          | O remetente NÃO recebe a própria mensagem                             | Confirmado        | ✅ OK  |
| T35 | F9    | Broadcast filtrado por canal             | user1 em #geral, user2 em #linux   | user2 não recebe mensagens de user1                                   | Confirmado        | ✅ OK  |
| T36 | F10   | LEAVE                                    | `LEAVE`                            | `LEAVE_OK: Saiu do canal`                                             | Idem              | ✅ OK  |
| T37 | F10   | LIST_CHANNELS                            | `LIST_CHANNELS`                    | Lista com canais e utilizadores presentes                             | Conforme          | ✅ OK  |
| T38 | F10   | LIST_CHANNELS sem canais activos         | Nenhum utilizador em canal         | `CHANNELS: Nenhum canal activo`                                       | Idem              | ✅ OK  |
| T39 | F9    | Múltiplos clientes (3) em #geral         | 3 clients, 1 envia broadcast       | Os outros 2 recebem instantaneamente                                  | Confirmado        | ✅ OK  |
| T40 | F10   | Mudança de canal                         | `JOIN #geral` → `JOIN #linux`      | Sai de #geral, entra em #linux                                        | Confirmado        | ✅ OK  |
| T41 | -     | LOGOUT (terminar sessão)                 | `LOGOUT`                           | `LOGOUT_OK`; estado limpo no servidor                                 | Idem              | ✅ OK  |
| T42 | -     | Desconexão abrupta (Ctrl+C)              | Cliente encerrado sem LOGOUT       | Servidor liberta slot; sem crash                                      | Confirmado        | ✅ OK  |
| T43 | -     | Limite de 50 clientes                    | 51º cliente tenta ligar            | `Servidor cheio, rejeitando cliente`                                  | Idem              | ✅ OK  |
| T44 | -     | `select()` duplo no cliente              | user2 escreve; user1 faz broadcast | user2 vê mensagem sem perder o que estava a escrever                  | Confirmado        | ✅ OK  |
| T45 | Adm   | `DELETE_USER_ADMIN`                      | `DELETE_USER_ADMIN user1` (admin)  | `DELETE_OK`; user1 removido do sistema                                | Conforme          | ✅ OK  |
| T46 | -     | Timestamps nas mensagens privadas        | `SEND_MSG` (admin → user1)         | `inbox.txt` guarda `[YYYY-MM-DD HH:MM:SS] mensagem`                   | Confirmado        | ✅ OK  |

### 4.4 Testes de Integração (Etapa 3)

| Nº  | Cenário                             | Sequência de Acções                                       | Resultado Esperado                                   | Estado |
| --- | ----------------------------------- | --------------------------------------------------------- | ---------------------------------------------------- | ------ |
| 1   | Registo + aprovação + login         | REGISTER (novo) → AUTH (PENDING) → APPROVE (admin) → AUTH | Novo utilizador consegue autenticar e aceder ao chat | ✅ OK  |
| 2   | Chat multi-utilizador em tempo real | 3 users em #geral; cada um envia 2 mensagens              | Todos recebem todas as mensagens em tempo real       | ✅ OK  |
| 3   | Admin bane utilizador durante chat  | Admin executa BAN user1 enquanto user1 está em #geral     | user1 fica INACTIVE; login futuro bloqueado          | ✅ OK  |
| 4   | Broadcast isolado por canal         | user1 em #geral, user2 em #linux, user3 em #geral         | Apenas user1 e user3 recebem broadcast               | ✅ OK  |
| 5   | Logout e re-login                   | user1 faz LOGOUT → LOGIN novamente → JOIN #geral          | Estado limpo; novo slot atribuído                    | ✅ OK  |

---

## 5. Conclusões

- **Todos os 46 testes unitários e 5 cenários de integração passaram com sucesso.**
- O código final da Etapa 3 cumpre integralmente os requisitos F9 e F10.
- O projecto está pronto para a defesa da Etapa 3 e para avançar para a Etapa 4.

---

_Documento actualizado em 2026-06-11 pelo Risk & Testing Manager_  
_Diego França – C-Cord Dev Group_
