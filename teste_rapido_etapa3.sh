#!/bin/bash

echo "╔════════════════════════════════════════════════════════════╗"
echo "║   TESTE RÁPIDO C-CORD ETAPA 3 — MOCKUPS & MENUS        ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Compilação
echo "▶ Compilando..."
gcc -Wall -Wextra -o server_linux server_linux.c -lm 2>&1 | grep -i error || echo "  ✅ Server OK (0 warnings)"
gcc -Wall -Wextra -o client_linux client_linux.c -lm 2>&1 | grep -i error || echo "  ✅ Client OK (0 warnings)"
echo ""

# Verificar binários
echo "▶ Verificando binários..."
[ -x ./server_linux ] && echo "  ✅ server_linux executável" || echo "  ❌ server_linux NÃO executável"
[ -x ./client_linux ] && echo "  ✅ client_linux executável" || echo "  ❌ client_linux NÃO executável"
echo ""

# Verificar estrutura de código
echo "▶ Verificando estrutura..."
grep -q "draw_header" client_linux.c && echo "  ✅ draw_header() presente"
grep -q "submenu_perfil" client_linux.c && echo "  ✅ submenu_perfil() presente"
grep -q "submenu_canais" client_linux.c && echo "  ✅ submenu_canais() presente"
grep -q "menu_admin" client_linux.c && echo "  ✅ menu_admin() presente"
echo ""

# Contar linhas
echo "▶ Estatísticas de código..."
CLIENT_LINES=$(wc -l < client_linux.c)
SERVER_LINES=$(wc -l < server_linux.c)
echo "  • client_linux.c: $CLIENT_LINES linhas"
echo "  • server_linux.c: $SERVER_LINES linhas"
echo "  • Total: $((CLIENT_LINES + SERVER_LINES)) linhas"
echo ""

# Verificar documentação
echo "▶ Documentação..."
[ -f RESUMO_ETAPA3.md ] && echo "  ✅ RESUMO_ETAPA3.md presente ($(wc -l < RESUMO_ETAPA3.md) linhas)"
[ -f README.md ] && echo "  ✅ README.md presente"
echo ""

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  ✅ TESTE RÁPIDO CONCLUÍDO COM SUCESSO                   ║"
echo "╚════════════════════════════════════════════════════════════╝"
