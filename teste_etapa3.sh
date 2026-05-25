#!/bin/bash

# Limpar logs/state
rm -f users.txt inbox.txt logs.txt

# 1. Registar utilizadores
echo "=== Teste 1: Registar utilizadores ==="
echo "REGISTER admin password123" | nc -q 1 127.0.0.1 10000 2>/dev/null

sleep 0.5

echo "REGISTER user1 pass1" | nc -q 1 127.0.0.1 10000 2>/dev/null

sleep 0.5

echo "REGISTER user2 pass2" | nc -q 1 127.0.0.1 10000 2>/dev/null

sleep 0.5

# 2. Teste LIST_ALL (deve mostrar todos os utilizadores registados)
echo ""
echo "=== Teste 2: LIST_ALL ==="
echo "LIST_ALL" | nc -q 1 127.0.0.1 10000 2>/dev/null

sleep 0.5

# 3. Teste LIST_PENDING (deve estar vazio, pois não registámos pending)
echo ""
echo "=== Teste 3: LIST_PENDING ==="
echo "LIST_PENDING" | nc -q 1 127.0.0.1 10000 2>/dev/null

sleep 0.5

# 4. Teste LIST_CHANNELS (deve mostrar canais com utilizadores)
echo ""
echo "=== Teste 4: LIST_CHANNELS (antes de JOIN) ==="
echo "LIST_CHANNELS" | nc -q 1 127.0.0.1 10000 2>/dev/null

