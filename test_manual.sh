#!/bin/bash

# =============================================================================
# SCRIPT DE TESTES MANUAL DO SISTEMA DSM
# Demonstra o protocolo Write-Invalidate e coerência de cache
# =============================================================================

echo "====================================================================="
echo "               TESTES MANUAIS DO SISTEMA DSM                        "
echo "====================================================================="
echo

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

print_step() {
    echo -e "${BLUE}📋 PASSO $1:${NC} $2"
    echo
}

print_info() {
    echo -e "${CYAN}ℹ️  INFO:${NC} $1"
}

print_success() {
    echo -e "${GREEN}✅ SUCESSO:${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠️  ATENÇÃO:${NC} $1"
}

print_error() {
    echo -e "${RED}❌ FALHA:${NC} $1"
}

wait_for_enter() {
    echo -e "${PURPLE}👆 Pressione ENTER para continuar...${NC}"
    read
    echo
}

# Compilar se necessário
if [ ! -f "test_dsm" ]; then
    echo "🔨 Compilando o projeto..."
    gcc -Wall -Wextra -std=c99 -pthread -g -o test_dsm dsm.c test_dsm.c
    if [ $? -ne 0 ]; then
        print_error "Erro na compilação!"
        exit 1
    fi
    print_success "Compilação bem-sucedida!"
    echo
fi

echo "====================================================================="
echo "                    CENÁRIOS DE TESTE                               "
echo "====================================================================="
echo

print_info "Este script irá guiá-lo através de vários cenários de teste"
print_info "Você precisará abrir 2 terminais e seguir as instruções"
echo

print_step "1" "PREPARAÇÃO DO AMBIENTE"
print_info "Mapeamento de blocos no sistema:"
echo "  • Bloco 0 (posições 0-4095)     → Processo 0"
echo "  • Bloco 1 (posições 4096-8191)  → Processo 1" 
echo "  • Bloco 2 (posições 8192-12287) → Processo 2"
echo "  • Bloco 3 (posições 12288-16383)→ Processo 3"
echo "  • Bloco 4 (posições 16384-20479)→ Processo 0"
echo "  • ..."
echo

print_info "Abra 2 terminais e execute:"
echo "  📟 Terminal 1: ./test_dsm 0"
echo "  📟 Terminal 2: ./test_dsm 1"
echo

wait_for_enter

echo "====================================================================="
echo "                    TESTE 1: CACHE MISS → CACHE HIT                 "
echo "====================================================================="
echo

print_step "1.1" "DEMONSTRAR CACHE MISS E CACHE HIT"

print_info "OBJETIVO: Mostrar como funciona o cache de blocos remotos"
echo

print_info "No Terminal 1 (Processo 0), execute:"
echo "  DSM[0]> r 4096 10"
print_warning "EXPLICAÇÃO: P0 vai ler bloco 1 (pertence ao P1) = CACHE MISS"
print_info "Deve aparecer: 'Cache miss para bloco 1'"
echo

wait_for_enter

print_info "Agora no mesmo Terminal 1, execute novamente:"
echo "  DSM[0]> r 4096 10"
print_warning "EXPLICAÇÃO: P0 vai ler o mesmo bloco = CACHE HIT"
print_info "Deve aparecer: 'Cache hit para bloco 1'"
echo

print_info "Verifique as estatísticas:"
echo "  DSM[0]> s"
print_success "Deve mostrar: Cache hits: 1, Cache misses: 1"
echo

wait_for_enter

echo "====================================================================="
echo "                TESTE 2: PROTOCOLO WRITE-INVALIDATE                 "
echo "====================================================================="
echo

print_step "2.1" "INVALIDAÇÃO DE CACHE"

print_info "OBJETIVO: Demonstrar como escritas invalidam caches remotos"
echo

print_info "No Terminal 2 (Processo 1), escreva no bloco 1:"
echo "  DSM[1]> w 4096 dados"
print_warning "EXPLICAÇÃO: P1 escreve no seu bloco e invalida cache do P0"
print_info "Deve aparecer: 'Invalidações enviadas para X processos'"
echo

wait_for_enter

print_info "Agora no Terminal 1, tente ler novamente:"
echo "  DSM[0]> r 4096 10"
print_warning "EXPLICAÇÃO: Cache foi invalidado = novo CACHE MISS"
print_info "Deve aparecer: 'Cache miss para bloco 1' (não hit!)"
echo

print_info "Verifique as estatísticas no Terminal 1:"
echo "  DSM[0]> s"
print_success "Deve mostrar: Cache hits: 1, Cache misses: 2, Invalidações recebidas: 1"
echo

wait_for_enter

echo "====================================================================="
echo "              TESTE 3: TENTATIVAS DE ESCRITA INVÁLIDAS             "
echo "====================================================================="
echo

print_step "3.1" "ESCREVER EM BLOCO ALHEIO (DEVE FALHAR)"

print_info "OBJETIVO: Mostrar que processos só podem escrever em seus próprios blocos"
echo

print_info "No Terminal 1 (Processo 0), tente escrever no bloco do P1:"
echo "  DSM[0]> w 4096 teste"
print_error "EXPLICAÇÃO: P0 NÃO PODE escrever no bloco 1 (pertence ao P1)"
print_info "Deve aparecer: 'Erro: Tentativa de escrita em bloco 1 que pertence ao processo 1'"
echo

wait_for_enter

print_step "3.2" "ESCREVER EM BLOCO PRÓPRIO (DEVE FUNCIONAR)"

print_info "No Terminal 1 (Processo 0), escreva no seu próprio bloco:"
echo "  DSM[0]> w 0 meusdados"
print_success "EXPLICAÇÃO: P0 PODE escrever no bloco 0 (é seu)"
print_info "Deve aparecer: 'Escrita bem-sucedida'"
echo

print_info "Confirme lendo os dados:"
echo "  DSM[0]> r 0 10"
print_success "Deve mostrar os dados escritos"
echo

wait_for_enter

echo "====================================================================="
echo "              TESTE 4: COERÊNCIA ENTRE MÚLTIPLOS PROCESSOS         "
echo "====================================================================="
echo

print_step "4.1" "TESTE DE COERÊNCIA AVANÇADO"

print_info "OBJETIVO: Demonstrar coerência completa do sistema"
echo

print_info "1. Terminal 2 (P1) escreve dados:"
echo "   DSM[1]> w 4096 versao1"
echo

print_info "2. Terminal 1 (P0) lê os dados (cache miss):"
echo "   DSM[0]> r 4096 8"
print_warning "Deve ler os dados do P1"
echo

print_info "3. Terminal 2 (P1) atualiza os dados:"
echo "   DSM[1]> w 4100 versao2"
print_warning "Isto vai invalidar o cache do P0"
echo

print_info "4. Terminal 1 (P0) lê novamente (cache miss por invalidação):"
echo "   DSM[0]> r 4096 8"
print_warning "Cache foi invalidado, deve buscar dados atualizados"
echo

wait_for_enter

echo "====================================================================="
echo "                     TESTE 5: ESTATÍSTICAS FINAIS                  "
echo "====================================================================="
echo

print_step "5.1" "VERIFICAR ESTATÍSTICAS COMPLETAS"

print_info "Em ambos terminais, execute:"
echo "  DSM[X]> s"
echo

print_success "RESULTADOS ESPERADOS:"
echo "  Terminal 1 (P0):"
echo "    • Cache hits: 1-2 (dependendo dos testes)"
echo "    • Cache misses: 3-4 (leituras de blocos remotos)"
echo "    • Invalidações enviadas: 1+ (quando escreveu no bloco 0)"
echo "    • Invalidações recebidas: 1+ (quando P1 escreveu no bloco 1)"
echo
echo "  Terminal 2 (P1):"  
echo "    • Cache hits: 0 (só leu blocos próprios)"
echo "    • Cache misses: 0-1 (dependendo se leu blocos remotos)"
echo "    • Invalidações enviadas: 1+ (quando escreveu no bloco 1)"
echo "    • Invalidações recebidas: 1+ (quando P0 escreveu no bloco 0)"
echo

wait_for_enter

echo "====================================================================="
echo "                        TESTE 6: FINALIZAÇÃO                       "
echo "====================================================================="
echo

print_step "6.1" "SAIR DOS PROCESSOS"

print_info "Em ambos terminais, digite:"
echo "  DSM[X]> q"
print_warning "Ou pressione Ctrl+C para testar o cleanup"
echo

print_success "RESULTADOS ESPERADOS:"
echo "  • Estatísticas finais mostradas"
echo "  • 'Sistema DSM finalizado'"
echo "  • 'Processo finalizado.'"
echo

echo "====================================================================="
echo "                           RESUMO DOS TESTES                        "
echo "====================================================================="
echo

print_success "CONCEITOS DEMONSTRADOS:"
echo "  ✅ Cache Miss: Primeira leitura de bloco remoto"
echo "  ✅ Cache Hit: Segunda leitura do mesmo bloco remoto"  
echo "  ✅ Write-Invalidate: Escritas invalidam caches remotos"
echo "  ✅ Proteção de escrita: Só pode escrever em blocos próprios"
echo "  ✅ Coerência: Dados sempre consistentes entre processos"
echo "  ✅ Estatísticas: Métricas de performance do cache"
echo

print_info "ARQUITETURA DEMONSTRADA:"
echo "  🏗️  Distribuição de blocos por módulo"
echo "  🔄 Protocolo de coerência Write-Invalidate"  
echo "  🗄️  Cache local para blocos remotos"
echo "  🌐 Comunicação TCP entre processos"
echo "  🔒 Controle de acesso baseado em ownership"
echo

echo "====================================================================="
print_success "TESTES MANUAIS CONCLUÍDOS!"
echo "=====================================================================" 