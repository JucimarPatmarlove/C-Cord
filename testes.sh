#!/bin/bash
# ============================================================================
# C-CORD — Script de Testes Automático
# Responsável: Diego França (Risk & Testing Manager)
# Etapas: 1 (F3-F4) e 2 (F5-F8)
#
# Utilização:
#   chmod +x testes.sh
#   ./testes.sh
#
# Requisitos:
#   - Servidor a correr: ./server_linux (Terminal separado)
#   - netcat instalado: apt install netcat-openbsd
# ============================================================================

HOST="127.0.0.1"
PORT="10000"
PASSOU=0
FALHOU=0
TOTAL=0

# ── Cores ──
VERDE="\033[1;32m"
VERMELHO="\033[1;31m"
CIANO="\033[1;36m"
AMARELO="\033[1;33m"
RESET="\033[0m"

# ── Cabeçalho ──
clear
echo -e "${CIANO}"
echo "  _____        _____              _ "
echo " / ____|      / ____|            | |"
echo "| |     _____| |     ___  _ __ __| |"
echo "| |    |_____| |    / _ \| '__/ _\` |"
echo "| |____      | |___| (_) | | | (_| |"
echo " \_____|      \_____\___/|_|  \__,_|"
echo -e "${RESET}"
echo "======================================================"
echo "   C-CORD — SUITE DE TESTES AUTOMÁTICOS v1.0"
echo "   Risk & Testing Manager: Diego França"
echo "======================================================"
echo " Servidor: ${HOST}:${PORT}"
echo " Data    : $(date '+%Y-%m-%d %H:%M:%S')"
echo "------------------------------------------------------"
echo ""

# ── Verificar se servidor está online ──
echo -e " ${CIANO}[INFO]${RESET} A verificar servidor..."
if ! echo -n "GET_INFO" | nc -q1 $HOST $PORT > /dev/null 2>&1; then
    echo -e " ${VERMELHO}[ERRO CRÍTICO]${RESET} Servidor não encontrado em ${HOST}:${PORT}"
    echo " Inicia o servidor primeiro: ./server_linux"
    exit 1
fi
echo -e " ${VERDE}[OK]${RESET} Servidor online. A iniciar testes...\n"
sleep 1

# ── Função de teste ──
# Uso: testar "ID" "Descrição" "Comando" "Resposta esperada (parcial)"
testar() {
    local id="$1"
    local desc="$2"
    local cmd="$3"
    local esperado="$4"
    TOTAL=$((TOTAL + 1))

    # Enviar comando e receber resposta
    local resposta
    resposta=$(echo -n "$cmd" | nc -q1 $HOST $PORT 2>/dev/null)

    # Verificar se resposta contém o esperado
    if echo "$resposta" | grep -q "$esperado"; then
        echo -e " ${VERDE}[✅ OK]${RESET}  ${AMARELO}${id}${RESET} — ${desc}"
        echo -e "         Enviado : ${cmd}"
        echo -e "         Recebido: ${resposta:0:80}"
        PASSOU=$((PASSOU + 1))
    else
        echo -e " ${VERMELHO}[❌ FAIL]${RESET} ${AMARELO}${id}${RESET} — ${desc}"
        echo -e "         Enviado  : ${cmd}"
        echo -e "         Esperado : ${esperado}"
        echo -e "         Recebido : ${resposta:0:80}"
        FALHOU=$((FALHOU + 1))
    fi
    echo ""
}

# ============================================================
# ETAPA 1 — F3 e F4
# ============================================================
echo "======================================================"
echo " ETAPA 1 — F3 (Autenticação) e F4 (Echo/Info)"
echo "======================================================"
echo ""

testar "T1" "Login válido — utilizador USER" \
    "AUTH user1 pass123" "AUTH_SUCCESS:USER"

testar "T2" "Login válido — utilizador ADMIN" \
    "AUTH admin admin123" "AUTH_SUCCESS:ADMIN"

testar "T3" "Login inválido — password errada" \
    "AUTH admin wrongpass" "AUTH_FAIL"

testar "T4" "Login inválido — utilizador inexistente" \
    "AUTH ninguem pass123" "AUTH_FAIL"

testar "T5" "GET_INFO — versão e uptime" \
    "GET_INFO" "C-Cord Server"

testar "T6" "ECHO — mensagem simples" \
    "ECHO Ola Mundo" "Servidor Ecoa: Ola Mundo"

testar "T7" "ECHO — mensagem com espaços" \
    "ECHO Teste com espacos" "Servidor Ecoa: Teste com espacos"

testar "T8" "Comando inválido" \
    "COMANDO_INVALIDO" "CMD_INVALID"

# ============================================================
# ETAPA 2 — F5, F6, F7, F8
# ============================================================
echo "======================================================"
echo " ETAPA 2 — F5 (Mensagens) F6 (Registo) F7-F8 (Admin)"
echo "======================================================"
echo ""

# Limpar utilizador de teste se existir (para garantir estado limpo)
echo -n "DELETE_USER admin teste_diego" | nc -q1 $HOST $PORT > /dev/null 2>&1
sleep 0.2

testar "T9" "Registar novo utilizador" \
    "REGISTER teste_diego pass999" "REGISTER_OK"

testar "T10" "Registar utilizador duplicado" \
    "REGISTER teste_diego pass999" "REGISTER_FAIL"

testar "T11" "Login com conta PENDING (deve bloquear)" \
    "AUTH teste_diego pass999" "AUTH_PENDING"

testar "T12" "LIST_ALL — listar todos os utilizadores" \
    "LIST_ALL" "UTILIZADORES REGISTADOS"

testar "T13" "LIST_PENDING — listar pendentes" \
    "LIST_PENDING" "teste_diego"

testar "T14" "SEND_MSG — enviar mensagem a utilizador existente" \
    "SEND_MSG jucimar admin Ola Jucimar!" "MSG_SENT"

testar "T15" "SEND_MSG — destinatário inexistente" \
    "SEND_MSG utilizador_fantasma admin Ola" "MSG_FAIL"

testar "T16" "CHECK_INBOX — utilizador com mensagens" \
    "CHECK_INBOX jucimar" "De:"

testar "T17" "CHECK_INBOX — utilizador sem mensagens" \
    "CHECK_INBOX admin" "caixa"

testar "T18" "APPROVE_USER — admin aprova conta PENDING" \
    "APPROVE_USER admin teste_diego" "APPROVE_OK"

testar "T19" "Login após aprovação (deve funcionar)" \
    "AUTH teste_diego pass999" "AUTH_SUCCESS:USER"

testar "T20" "APPROVE_USER — sem permissão de admin" \
    "APPROVE_USER user1 teste_diego" "APPROVE_FAIL"

testar "T21" "SUSPEND_USER — suspender conta ACTIVE" \
    "SUSPEND_USER admin teste_diego" "SUSPEND_OK"

testar "T22" "Login com conta INACTIVE (deve bloquear)" \
    "AUTH teste_diego pass999" "AUTH_INACTIVE"

testar "T23" "SUSPEND_USER — reativar conta INACTIVE" \
    "SUSPEND_USER admin teste_diego" "SUSPEND_OK"

testar "T24" "Login após reativação (deve funcionar)" \
    "AUTH teste_diego pass999" "AUTH_SUCCESS"

testar "T25" "DELETE_USER — remover utilizador" \
    "DELETE_USER admin teste_diego" "DELETE_OK"

testar "T26" "DELETE_USER — admin tenta apagar-se" \
    "DELETE_USER admin admin" "DELETE_FAIL"

testar "T27" "DELETE_USER — sem permissão de admin" \
    "DELETE_USER user1 jucimar" "DELETE_FAIL"

testar "T28" "VIEW_LOGS — admin consulta logs" \
    "VIEW_LOGS admin" "REGISTO"

# ============================================================
# SUMÁRIO FINAL
# ============================================================
echo "======================================================"
echo " SUMÁRIO DOS TESTES"
echo "======================================================"
echo ""
echo -e " Total de testes : ${TOTAL}"
echo -e " ${VERDE}Passaram${RESET}         : ${PASSOU}"

if [ $FALHOU -gt 0 ]; then
    echo -e " ${VERMELHO}Falharam${RESET}         : ${FALHOU}"
else
    echo -e " Falharam        : ${FALHOU}"
fi

echo ""

if [ $FALHOU -eq 0 ]; then
    echo -e " ${VERDE}✅ TODOS OS TESTES PASSARAM — Sistema OK para entrega!${RESET}"
else
    echo -e " ${VERMELHO}❌ ${FALHOU} TESTE(S) FALHARAM — Verificar antes da entrega.${RESET}"
fi

echo ""
echo " Relatório guardado em: resultados_testes_$(date '+%Y%m%d_%H%M%S').txt"
echo "======================================================"

# Guardar resultado em ficheiro
{
    echo "C-CORD — Resultados dos Testes"
    echo "Data: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "Total: ${TOTAL} | Passaram: ${PASSOU} | Falharam: ${FALHOU}"
} > "resultados_testes_$(date '+%Y%m%d_%H%M%S').txt"
