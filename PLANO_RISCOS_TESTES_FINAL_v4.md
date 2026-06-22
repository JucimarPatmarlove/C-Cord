# C-CORD – Plano de Riscos e Testes Final (v4.0)

**Versão:** 4.0 (Final – alinhada com a entrega de Junho de 2026)  
**Data:** 2026-06-22  
**Responsável:** Diego França (Risk & Testing Manager)  
**Unidade Curricular:** Administração e Segurança de Sistemas – ESTG/IPG

---

## 1. Introdução

Este documento integra o **Plano de Riscos** e o **Plano de Testes** consolidados do projecto C-Cord.  
A versão **v4.0** introduziu:
- Camada criptográfica E2EE (Encriptação Ponta-a-Ponta) com Cifra de César simétrica dinâmica.
- Autenticação segura com Hash DJB2 cifrado via Toy RSA de 1024-bits (simulado).
- Acordo de chaves efêmeras Diffie-Hellman em tempo real entre o cliente e o servidor.
- Comandos administrativos de segurança (`VIEW_CRYPTO`).
- Auditoria SecOps interna e mitigação definitiva de overflows com lógica de 128-bits (`__int128`).

---

## 2. Identificação da Equipa

| Nome           | Papel              | Responsabilidades no Projeto                     |
| -------------- | ------------------ | ------------------------------------------------ |
| Carlos Martins | Team Manager       | Coordenação, planeamento Gantt, apresentações    |
| Diego França   | Risk & Testing Mgr | Este documento – riscos, testes, validação       |
| David Bunga    | Dev Team           | Desenvolvimento geral (F1–F15)                   |
| Jucimar Cabral | Dev Team           | Implementação do servidor/cliente Linux (F3–F14) |

---

## 3. Plano de Riscos

### 3.1 Metodologia

Riscos avaliados por **Probabilidade** (Baixa/Média/Alta) e **Impacto** (Baixo/Médio/Alto).  
Estado: `Resolvido` / `Monitorizado` / `Activo` / `Planeado`.

### 3.2 Matriz de Riscos

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
| R15 | Passwords em texto simples                | Alta  | Alto    | Alto  | **Resolvido com Hash DJB2 + RSA na Etapa 4**  | Resolvido    |
| R16 | Perda de ligação persistente              | Baixa | Alto    | Médio | Timeout configurável; reconexão manual        | Resolvido    |
| R17 | Esgotamento de descritores (MAX_CLIENTES) | Baixa | Médio   | Baixo | Limite de 50 clientes; servidor rejeita acima | Resolvido    |

---

## 4. Plano de Testes

### 4.1 Resumo dos Resultados

| Categoria | Total | Sucesso |
|-----------|-------|---------|
| Etapa 1 (F3-F4) | 8 | ✅ 8/8 |
| Etapa 2 (F5-F8) | 20 | ✅ 20/20 |
| Etapa 3 (F9-F10) | 18 | ✅ 18/18 |
| **Etapa 4 (F11-F14)** | **6** | **✅ 6/6** |
| **Total** | **52** | **✅ 52/52** |

---

### 4.2 Testes da Etapa 4 (F11–F14)

| ID  | Func. | Descrição                                    | Input                                      | Resultado Esperado                                                         | Resultado Obtido |
|-----|-------|----------------------------------------------|--------------------------------------------|----------------------------------------------------------------------------|------------------|
| T47 | F11   | Login com Hash + RSA (E2EE)                 | `admin` / `admin123`                       | Servidor aceita; cliente mostra `[CRYPTO]`                                | ✅ OK            |
| T48 | F12   | Troca de chaves Diffie-Hellman              | (automático após login)                    | Cliente e servidor calculam `K` igual; mensagem `DH_EXCHANGE` nos logs    | ✅ OK            |
| T49 | F11   | Broadcast cifrado (César)                   | `BROADCAST Olá`                            | Mensagem cifrada na rede; decifrada no cliente recetor                    | ✅ OK            |
| T50 | F11   | ECHO com cifra                              | `ECHO Teste`                               | Servidor ecoa mensagem cifrada; cliente decifra e mostra                  | ✅ OK            |
| T51 | F14   | VIEW_CRYPTO (Admin)                         | (Admin) `VIEW_CRYPTO`                      | Devolve parâmetros DH, RSA, chaves ativas                                 | ✅ OK            |
| T52 | F14   | VIEW_CRYPTO (User)                          | (User) `VIEW_CRYPTO`                       | `CRYPTO_FAIL: Sem permissões`                                             | ✅ OK            |

---

### 4.3 Testes da Etapa 3 (F9–F10)

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

---

## 5. Conclusões

- **Todos os 52 testes unitários e os cenários de integração foram validados com sucesso no Kali Linux.**
- O C-Cord v4.0 é resiliente a regressões e está em total conformidade com as regras criptográficas especificadas nas Fases de Desenvolvimento.

---
_Documento consolidado em 2026-06-22 por Diego França – Risk & Testing Manager_
