#!/bin/bash

# TESTE RÁPIDO — Etapa 3 (C-Cord)
# Instrções interativas passo-a-passo

clear

cat << 'EOF'

╔══════════════════════════════════════════════════════════════════════════════╗
║                   🚀 TESTE RÁPIDO — ETAPA 3 (C-CORD)                       ║
╚══════════════════════════════════════════════════════════════════════════════╝

Este script vai guiar-te através dos testes críticos.

🎯 ANTES DE COMEÇAR:
   ✓ Tens 2-3 terminais abertos (ou tmux/screen)
   ✓ Estás em /home/kali/C-Cord/
   ✓ users.txt tem dados (joao:password123, maria:password456, etc)
   ✓ Compilação completa (client_linux e server_linux existem)

═══════════════════════════════════════════════════════════════════════════════

📋 TESTE 1: Verificação rápida

EOF

# Verificar ficheiros
echo ""
echo "✓ Compiláveis existem?"
ls -lh client_linux server_linux 2>/dev/null && echo "✅ SIM" || echo "❌ NÃO - Compila com: gcc -Wall -Wextra client_linux.c -o client_linux"
echo ""

echo "✓ Ficheiro users.txt existe?"
[ -f users.txt ] && echo "✅ SIM ($(wc -l < users.txt) utilizadores)" || echo "❌ NÃO"
echo ""

echo "✓ Ficheiro logs.txt existe?"
[ -f logs.txt ] && echo "✅ SIM ($(wc -l < logs.txt) linhas)" || echo "❌ NÃO (será criado na primeira execução)"
echo ""

cat << 'EOF'

═══════════════════════════════════════════════════════════════════════════════

📋 TESTE 2: SERVIDOR

INSTRUÇÕES:
  1. Abre TERMINAL 1
  2. Executa:
  
     $ ./server_linux
     
  3. Deves ver:
  
     [SERVIDOR] Iniciado na porta 10000
     [SERVIDOR] À escuta de conexões...
     
  4. Se vires isto, o servidor está a funcionar! ✅
  
  DEPOIS: Pressiona ENTER aqui para continuar

EOF

read -p "Pressiona ENTER após o servidor estar a correr..."

cat << 'EOF'

═══════════════════════════════════════════════════════════════════════════════

📋 TESTE 3: CLIENTE CONECTA

INSTRUÇÕES:
  1. Abre TERMINAL 2
  2. Executa:
  
     $ ./client_linux 127.0.0.1 10000
     
  3. Deves ver o menu:
  
     ╔════════════════════════════════════════╗
     ║      █████ C-CORD CHAT █████           ║
     ║                                        ║
     ║     STATUS: GUEST (branco)            ║
     ║                                        ║
     ╚════════════════════════════════════════╝
     
     F3 - Login
     F4 - Registo
     F0 - Exit
     
     Input: 
     
  4. Se vires isto, o cliente conectou! ✅
  
  DEPOIS: Pressiona ENTER para continuar

EOF

read -p "Pressiona ENTER após o cliente aparecer..."

cat << 'EOF'

═══════════════════════════════════════════════════════════════════════════════

📋 TESTE 4: AUTENTICAÇÃO (VÁLIDA)

INSTRUÇÕES (TERMINAL 2):
  1. Input: F3 (ou 3)
  2. Username: joao
  3. Password: password123
  
  🎯 RESULTADO ESPERADO:
  ✅ [AUTH_SUCCESS] Autenticado como joao (USER)
  ✅ TUI muda de BRANCO para CYAN
  ✅ Novo menu aparece (F5, F6, F7, F8, F9, F10)
  
  🔍 VERIFICAR NO SERVIDOR (TERMINAL 1):
  [HH:MM:SS] [OK] AUTH_SUCCESS: joao (USER) - 127.0.0.1

  DEPOIS: Pressiona ENTER

EOF

read -p "Pressiona ENTER após autenticação bem-sucedida..."

cat << 'EOF'

═══════════════════════════════════════════════════════════════════════════════

📋 TESTE 5: JOIN #CANAL

INSTRUÇÕES (TERMINAL 2 - Cliente autenticado):
  1. Input: F9 (JOIN #canal)
  2. Qual canal? #geral
  
  🎯 RESULTADO ESPERADO:
  ✅ [JOIN_OK] Entrou no canal #geral
  
  🔍 VERIFICAR NO SERVIDOR (TERMINAL 1):
  [HH:MM:SS] [INFO] joao JOINED #geral

  DEPOIS: Pressiona ENTER

EOF

read -p "Pressiona ENTER após JOIN bem-sucedido..."

cat << 'EOF'

═══════════════════════════════════════════════════════════════════════════════

📋 TESTE 6: BROADCAST (1 Cliente)

INSTRUÇÕES (TERMINAL 2 - Cliente no #geral):
  1. Input: F10 (BROADCAST)
  2. Mensagem: Olá a todos!
  
  🎯 RESULTADO ESPERADO:
  ✅ [BCAST_SENT] Mensagem enviada ao canal #geral
  
  🔍 VERIFICAR NO SERVIDOR (TERMINAL 1):
  [HH:MM:SS] [INFO] BROADCAST from joao: Olá a todos!

  DEPOIS: Pressiona ENTER

EOF

read -p "Pressiona ENTER após broadcast..."

cat << 'EOF'

═══════════════════════════════════════════════════════════════════════════════

📋 TESTE 7: BROADCAST (2 CLIENTES) — ⭐ CRITICAL TEST ⭐

Este é o teste MAIS IMPORTANTE!

INSTRUÇÕES:
  1. Abre TERMINAL 3
  2. Executa:
  
     $ ./client_linux 127.0.0.1 10000
     
  3. Autentica (F3):
     Username: joao
     Password: password123
     
  4. JOIN #geral (F9)
     Qual canal? #geral
     
  5. AGORA tens:
     Terminal 2 (Cliente A - joao em #geral)
     Terminal 3 (Cliente B - joao em #geral)
     
  6. Em Terminal 2, envia BROADCAST (F10):
     Mensagem: Isto é de A!
     
  7. 🎯 VERIFICAR TERMINAL 3:
     Deves ver aparecer (sem fazeres nenhum comando):
     
     [#geral] joao: Isto é de A!
     
     ✅ Se vires isto: SELECT MULTIPLEX FUNCIONA! 🎊
     ❌ Se NÃO vires: Problema no recv() em tempo real

  DEPOIS: Pressiona ENTER

EOF

read -p "Pressiona ENTER após teste de broadcast múltiplo..."

cat << 'EOF'

═══════════════════════════════════════════════════════════════════════════════

📋 TESTE 8: VERIFICAR LOGS

INSTRUÇÕES (TERMINAL NOVO):
  1. Executa:
  
     $ cat logs.txt | tail -20
     
  2. Deves ver:
     [HH:MM:SS] [OK] AUTH_SUCCESS: joao (USER)
     [HH:MM:SS] [INFO] joao JOINED #geral
     [HH:MM:SS] [INFO] BROADCAST from joao
     
  ✅ Se ves isto: Auditoria funciona!

DEPOIS: Pressiona ENTER

EOF

read -p "Pressiona ENTER..."

cat << 'EOF'

═══════════════════════════════════════════════════════════════════════════════

📊 RESUMO DOS TESTES

┌─────────────────────────────────────────────────────────────────────────────┐
│ Teste                                          │ Status  │ Notas            │
├─────────────────────────────────────────────────────────────────────────────┤
│ 1. Compilação                                  │ ✅      │ 0 warnings       │
│ 2. Servidor liga (porta 10000)                 │ ✅      │                  │
│ 3. Cliente conecta ao servidor                 │ ✅      │ TCP OK           │
│ 4. Autenticação VÁLIDA                         │ ✅      │ AUTH_SUCCESS     │
│ 5. JOIN #canal                                 │ ✅      │ Estado persiste  │
│ 6. BROADCAST (1 cliente)                       │ ✅      │ send() funciona  │
│ 7. BROADCAST (2+ clientes) - CRITICAL         │ ✅      │ select() funciona│
│ 8. Logs (auditoria)                            │ ✅      │ Ficheiro OK      │
└─────────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════

🎊 SE TODOS OS TESTES PASSARAM:

  ✅ ETAPA 3 ESTÁ 100% FUNCIONAL!
  
  Próximas etapas:
  1. Documentação: Pronto ✅
  2. Git: Commits feitos ✅
  3. Defesa: Código está preparado para apresentação ✅

═══════════════════════════════════════════════════════════════════════════════

⚠️  TROUBLESHOOTING:

Se algo não funcionar:

1. SERVIDOR NÃO LIGA
   → Porta 10000 já está em uso?
   → Tenta: sudo lsof -i :10000
   → Se sim: kill <PID> ou muda porta em server_linux.c (linha 40)

2. CLIENTE NÃO CONECTA
   → IP errado?
   → Tenta: ./client_linux localhost 10000
   → Firewall bloqueando?

3. AUTENTICAÇÃO FALHA
   → users.txt corrompido?
   → Verifica: cat users.txt
   → Formato: ID:username:password:role:status

4. BROADCAST NÃO RECEBE (TESTE 7 falha)
   → select() pode não estar a funcionar
   → Verifica logs.txt para erros
   → Tenta: strace -e recv,send ./client_linux

═══════════════════════════════════════════════════════════════════════════════

📋 TESTE 9: LIST_CHANNELS (NOVO - Etapa 3.0)

INSTRUÇÕES:
  1. Abre TERMINAL 1 → ./server_linux
  2. Abre TERMINAL 2 → ./client_linux 127.0.0.1 10000
     Input: 1 (Login)
     Username: joao
     Password: password123
     Input: JOIN #geral
     Input: LIST_CHANNELS
  
ESPERADO:
  CHANNELS: #geral (1)

NOTA:
  - Se tiveres 2+ clientes em #geral, o número sobe
  - Teste de validação: servidor está a contar utilizadores por canal

═══════════════════════════════════════════════════════════════════════════════

Muito bem! Testes concluídos.

Tens perguntas? Vê:
  1. README.md (documentação geral)
  2. DOCUMENTACAO.md (protocolo detalhado)
  3. logs.txt (auditoria servidor)

EOF
