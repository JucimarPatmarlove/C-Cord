# C-CORD – Plano de Testes Final (Etapas 1 a 4)

**Versão:** 4.0  
**Data:** 2026-06-22  

---

## 1. Resumo

Este documento consolida os testes das Etapas 1, 2, 3 e **4 (nova)**.

| Categoria | Total | Sucesso |
|-----------|-------|---------|
| Etapa 1 (F3-F4) | 8 | ✅ 8/8 |
| Etapa 2 (F5-F8) | 20 | ✅ 20/20 |
| Etapa 3 (F9-F10) | 18 | ✅ 18/18 |
| **Etapa 4 (F11-F14)** | **6** | **✅ 6/6** |
| **Total** | **52** | **✅ 52/52** |

---

## 2. Testes da Etapa 4 (F11–F14)

| ID  | Func. | Descrição                                    | Input                                      | Resultado Esperado                                                         | Resultado Obtido |
|-----|-------|----------------------------------------------|--------------------------------------------|----------------------------------------------------------------------------|------------------|
| T47 | F11   | Login com Hash + RSA (E2EE)                 | `admin` / `admin123`                       | Servidor aceita; cliente mostra `[CRYPTO]`                                | ✅ OK            |
| T48 | F12   | Troca de chaves Diffie-Hellman              | (automático após login)                    | Cliente e servidor calculam `K` igual; mensagem `DH_EXCHANGE` nos logs    | ✅ OK            |
| T49 | F11   | Broadcast cifrado (César)                   | `BROADCAST Olá`                            | Mensagem cifrada na rede; decifrada no cliente recetor                    | ✅ OK            |
| T50 | F11   | ECHO com cifra                              | `ECHO Teste`                               | Servidor ecoa mensagem cifrada; cliente decifra e mostra                  | ✅ OK            |
| T51 | F14   | VIEW_CRYPTO (Admin)                         | (Admin) `VIEW_CRYPTO`                      | Devolve parâmetros DH, RSA, chaves ativas                                 | ✅ OK            |
| T52 | F14   | VIEW_CRYPTO (User)                          | (User) `VIEW_CRYPTO`                       | `CRYPTO_FAIL: Sem permissões`                                             | ✅ OK            |

**Nota:** Todos os testes foram executados em Kali Linux com GCC, porta 10000.
