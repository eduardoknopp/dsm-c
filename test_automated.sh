#!/bin/bash

# Script de teste automatizado para o sistema DSM
# Demonstra o protocolo Write-Invalidate em ação

echo "=== TESTE AUTOMATIZADO DO SISTEMA DSM ==="
echo

# Compilar se necessário
if [ ! -f "test_dsm" ]; then
    echo "Compilando o projeto..."
    gcc -Wall -Wextra -std=c99 -pthread -g -o test_dsm dsm.c test_dsm.c
    if [ $? -ne 0 ]; then
        echo "Erro na compilação!"
        exit 1
    fi
fi

# Função para limpar processos
cleanup() {
    echo "Limpando processos..."
    pkill -f test_dsm 2>/dev/null || true
    sleep 2
}

# Limpar processos anteriores
cleanup

echo
echo "1. Iniciando processos DSM..."

# Iniciar processos em background com modo automático
./test_dsm 0 auto &
PID0=$!
sleep 1

./test_dsm 1 auto &
PID1=$!
sleep 1

./test_dsm 2 auto &
PID2=$!
sleep 1

./test_dsm 3 auto &
PID3=$!
sleep 1

echo "2. Testando comunicação básica..."

# Verificar se os processos ainda estão rodando
for pid in $PID0 $PID1 $PID2 $PID3; do
    if ! kill -0 $pid 2>/dev/null; then
        echo "   Erro: Processo $pid não está rodando"
        cleanup
        exit 1
    fi
done

echo "   ✓ Todos os processos estão rodando"

echo "3. Demonstração do protocolo Write-Invalidate:"
echo "   - Os processos executarão testes automáticos"
echo "   - Observe as mensagens coloridas para ver:"
echo "     * Cache hits e misses (cada processo tem sua cor)"
echo "     * Invalidações enviadas e recebidas"
echo "     * Comunicação entre processos"
echo "     * Sucessos em verde, erros em vermelho"

echo
echo "4. Os processos estão rodando em modo automático."
echo "   Eles executarão os testes e terminarão automaticamente."
echo "   Para modo interativo, execute: './test_dsm <id>' (sem 'auto')"

echo
echo "5. Para parar todos os processos:"
echo "   - Pressione Ctrl+C aqui"
echo "   - Ou execute 'pkill -f test_dsm'"

echo
echo "=== PROCESSOS EM EXECUÇÃO ==="
echo "Os seguintes processos estão rodando:"
ps aux | grep test_dsm | grep -v grep

# Aguardar sinal de interrupção
trap 'cleanup; echo "Teste finalizado."; exit 0' INT TERM

echo
echo "Pressione Ctrl+C para parar todos os processos..."
wait

# Se chegou até aqui, os processos terminaram naturalmente
echo "Todos os processos terminaram naturalmente."
cleanup
echo "Teste finalizado." 