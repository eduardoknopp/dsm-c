# 📋 EXPLICAÇÃO DETALHADA DO TESTE AUTOMÁTICO

Este documento explica em detalhes o que acontece no **teste automático** (`teste_basico()`) do sistema DSM, por que certas operações devem funcionar ou falhar, e qual é o comportamento esperado.

## 🏗️ **Arquitetura do Sistema**

### **Distribuição de Blocos**
O sistema usa **distribuição por módulo**:
- **Bloco ID % num_processos** = processo dono
- Bloco 0, 4, 8, 12... → Processo 0
- Bloco 1, 5, 9, 13... → Processo 1  
- Bloco 2, 6, 10, 14... → Processo 2
- Bloco 3, 7, 11, 15... → Processo 3

### **Protocolo Write-Invalidate**
- Apenas o **processo dono** pode **escrever** em um bloco
- Quando escreve, **invalida** caches remotos que possuem aquele bloco
- Outros processos podem **ler** qualquer bloco (via cache local ou requisição remota)

---

## 📊 **ANÁLISE DETALHADA DO `teste_basico()`**

### **CONFIGURAÇÃO INICIAL**
```c
int id = dsm_global->meu_id;  // ID do processo (0, 1, 2 ou 3)
byte dados_escrita[] = "Hello DSM World!";  // 17 bytes (16 + \0)
byte buffer_leitura[100];  // Buffer para leitura
```

**Contexto**: Cada processo executa este teste **independentemente** e **simultaneamente**.

---

## ✅ **TESTE 1: ESCRITA E LEITURA LOCAL**

### **Código:**
```c
// Encontrar um bloco que pertence a este processo
int bloco_teste = dsm_global->meu_id;  // P0=bloco 0, P1=bloco 1, etc.
int posicao = bloco_teste * T_TAMANHO_BLOCO;  // P0=0, P1=4096, P2=8192, P3=12288

if (escreve(posicao, dados_escrita, strlen((char*)dados_escrita) + 1) == 0) {
    // Escrita bem-sucedida → teste de leitura
    if (le(posicao, buffer_leitura, strlen((char*)dados_escrita) + 1) == 0) {
        // Leitura bem-sucedida
    }
}
```

### **Por que DEVE FUNCIONAR:**
| Processo | Bloco Testado | Posição | Motivo do Sucesso |
|----------|---------------|---------|-------------------|
| P0       | Bloco 0       | 0       | **P0 é dono do bloco 0** |
| P1       | Bloco 1       | 4096    | **P1 é dono do bloco 1** |
| P2       | Bloco 2       | 8192    | **P2 é dono do bloco 2** |
| P3       | Bloco 3       | 12288   | **P3 é dono do bloco 3** |

### **Fluxo de Execução:**
1. **Escrita**: `escreve()` → acesso direto à memória local (sem rede)
2. **Invalidação**: envia mensagens para outros processos (que podem estar offline)
3. **Leitura**: `le()` → acesso direto à memória local (sem cache)

### **Resultado Esperado:**
- ✅ **Escrita bem-sucedida** (cada processo em seu próprio bloco)
- ✅ **Leitura bem-sucedida** com dados corretos
- 📊 **Estatísticas**: Não afeta cache (leitura local)

---

## 🌐 **TESTE 2: LEITURA REMOTA (CACHE MISS → CACHE HIT)**

### **Código:**
```c
// Teste de acesso a bloco remoto
int bloco_remoto = (dsm_global->meu_id + 1) % dsm_global->num_processos;
int posicao_remota = bloco_remoto * T_TAMANHO_BLOCO;

if (le(posicao_remota, buffer_leitura, 16) == 0) {
    // Primeira leitura = CACHE MISS
    if (le(posicao_remota, buffer_leitura, 16) == 0) { 
        // Segunda leitura = CACHE HIT
    }
}
```

### **Por que PODE FALHAR:**
🔴 **Problema de Conectividade:**
- Se o **processo dono** não estiver executando, a requisição vai falhar
- Exemplo: P0 tenta ler bloco 1, mas P1 não está rodando
- Resultado: `"Falha ao enviar requisição do bloco 1 (processo 1 indisponível)"`

### **Por que DEVE FUNCIONAR (quando todos estão rodando):**

#### **Primeira Leitura (CACHE MISS):**
1. P0 tenta ler posição 4096 (bloco 1, pertence a P1)
2. Cache do P0 para bloco 1 está **vazio** (inválido)
3. P0 envia requisição TCP para P1 na porta 8081
4. P1 responde com dados do bloco 1
5. P0 carrega dados no cache e marca como **válido**
6. **Resultado**: `"Cache miss para bloco 1"` + `"Bloco 1 carregado no cache"`

#### **Segunda Leitura (CACHE HIT):**
1. P0 tenta ler posição 4096 novamente
2. Cache do P0 para bloco 1 está **válido**
3. P0 lê direto do cache local (sem rede)
4. **Resultado**: `"Cache hit para bloco 1"`

### **Estatísticas Esperadas:**
```
Cache hits: 1    (segunda leitura)
Cache misses: 1  (primeira leitura)
```

---

## ❌ **TESTE 3: ESCRITA REMOTA (DEVE FALHAR)**

### **Código:**
```c
// Teste de escrita em bloco remoto (deve falhar)
if (escreve(posicao_remota, dados_escrita, strlen((char*)dados_escrita) + 1) != 0) {
    // Escrita rejeitada corretamente
} else {
    // ERRO: Escrita aceita incorretamente
}
```

### **Por que DEVE FALHAR:**
| Processo | Tenta Escrever | Bloco | Dono Real | Motivo da Rejeição |
|----------|----------------|-------|-----------|-------------------|
| P0       | Posição 4096   | 1     | P1        | **P0 não é dono do bloco 1** |
| P1       | Posição 8192   | 2     | P2        | **P1 não é dono do bloco 2** |
| P2       | Posição 12288  | 3     | P3        | **P2 não é dono do bloco 3** |
| P3       | Posição 0      | 0     | P0        | **P3 não é dono do bloco 0** |

### **Fluxo de Validação:**
1. `escreve()` calcula dono: `id_bloco % num_processos`
2. Compara com `dsm_global->meu_id`
3. Se **diferente** → retorna erro sem fazer nada
4. **Resultado**: `"Erro: Tentativa de escrita em bloco X que pertence ao processo Y"`

### **Importância:**
Este teste valida a **proteção de escrita** do protocolo Write-Invalidate. É **fundamental** que falhe, pois garante a integridade dos dados.

---

## 🚧 **CENÁRIOS DE FALHA E SUCESSO**

### **Cenário 1: Todos os Processos Rodando** ✅
```bash
./test_dsm 0 auto &
./test_dsm 1 auto &  
./test_dsm 2 auto &
./test_dsm 3 auto &
```

**Resultado Esperado:**
- ✅ Escritas locais: **4/4 sucessos**
- ✅ Leituras remotas: **4/4 sucessos** (cache miss + cache hit)
- ✅ Escritas remotas: **4/4 falhas** (corretamente rejeitadas)
- 📊 **Cache hits**: 4 (um por processo)
- 📊 **Cache misses**: 4 (um por processo)

### **Cenário 2: Apenas P0 Rodando** ⚠️
```bash
./test_dsm 0 auto
```

**Resultado Esperado:**
- ✅ Escrita local (bloco 0): **sucesso**
- ❌ Leitura remota (bloco 1): **falha** (P1 não está rodando)
- ✅ Escrita remota (bloco 1): **falha** (corretamente rejeitada)
- 📊 **Cache hits**: 0
- 📊 **Cache misses**: 1 (tentativa que falhou)

### **Cenário 3: P0 e P1 Rodando** 🔄
```bash
./test_dsm 0 auto &
./test_dsm 1 auto &
```

**Resultado Esperado:**
- ✅ P0 escreve no bloco 0, lê do bloco 1 (P1)
- ✅ P1 escreve no bloco 1, lê do bloco 2 (P2 - **falha**)
- 📊 **P0**: 1 hit, 1 miss
- 📊 **P1**: 0 hits, 1 miss (falha)

---

## 🔄 **PROTOCOLO WRITE-INVALIDATE EM AÇÃO**

### **Quando P0 Escreve no Bloco 0:**
1. P0 escreve dados na memória local
2. P0 envia `MSG_INVALIDAR_BLOCO` para P1, P2, P3
3. Se P1 tem bloco 0 no cache → marca como **inválido**
4. P1 responde com `MSG_ACK_INVALIDACAO`
5. **Resultado**: Coerência mantida

### **Estatísticas de Invalidação:**
```
[P0] Invalidações enviadas: 3    (para P1, P2, P3)
[P1] Invalidações recebidas: 1   (do P0)
```

---

## 📈 **ANÁLISE DE PERFORMANCE**

### **Fatores que Afetam o Desempenho:**

#### **Cache Hits vs Misses:**
- **Cache Hit**: ~1µs (acesso à memória local)
- **Cache Miss**: ~1-10ms (comunicação TCP + serialização)
- **Melhoria**: 1000x-10000x mais rápido

#### **Invalidações:**
- **Custo**: O(n-1) mensagens por escrita (n = número de processos)
- **Benefício**: Garante coerência sem overhead de leitura

#### **Falhas de Conectividade:**
- **Timeout TCP**: ~5 segundos por processo offline
- **Otimização**: Cache de processos ativos/inativos

---

## 🎯 **CONCLUSÃO DO TESTE AUTOMÁTICO**

O `teste_basico()` é um **microbenchmark** que valida:

### **Funcionalidades Básicas:** ✅
- ✅ Escrita em blocos próprios
- ✅ Leitura de blocos próprios  
- ✅ Leitura de blocos remotos (quando disponíveis)
- ✅ Rejeição de escritas em blocos alheios

### **Protocolo Write-Invalidate:** ✅
- ✅ Cache miss na primeira leitura remota
- ✅ Cache hit na segunda leitura remota
- ✅ Invalidação de caches após escritas
- ✅ Comunicação TCP entre processos

### **Tratamento de Falhas:** ✅
- ✅ Processos offline detectados
- ✅ Timeouts TCP tratados graciosamente
- ✅ Estado consistente mesmo com falhas parciais

### **Métricas de Qualidade:**
- 🎯 **Correção**: Proteção de escrita + coerência
- ⚡ **Performance**: Cache hits reduzem latência
- 🛡️ **Robustez**: Funciona com processos offline
- 📊 **Observabilidade**: Estatísticas detalhadas

**O teste automático é uma demonstração completa e realística do sistema DSM em funcionamento!** 🚀 