# Sistema de Memória Compartilhada Distribuída (DSM)

Este projeto implementa um protótipo de sistema de memória compartilhada distribuída com protocolo de coerência de cache Write-Invalidate, desenvolvido como trabalho da disciplina de Programação Paralela e Distribuída.

## 📋 Resumo dos Requisitos

### Objetivo
Desenvolver uma aplicação distribuída que simule um espaço de endereçamento compartilhado entre múltiplos processos, com resolução de conflitos e garantia de consistência de dados.

### Terminologia
- **n**: número de processos (4)
- **k**: número de blocos (1024)  
- **t**: tamanho dos blocos em bytes (4096 = 4KB)
- **pi**: o i-ésimo processo
- **bj**: o j-ésimo bloco do espaço de endereçamento

### Configuração do Sistema
- **Espaço total**: 1024 blocos × 4KB = 4GB
- **Processos**: 4 processos (IDs 0-3)
- **Distribuição**: Cada processo gerencia ~1GB (256 blocos)
- **Comunicação**: TCP sockets (portas 8080-8083)

## 🏗️ Arquitetura da Implementação

### Estruturas Principais

#### 1. Sistema DSM (`SistemaDSM` em `dsm.h:78-108`)
```c
typedef struct {
    int meu_id;                           // ID do processo (0-3)
    int num_processos;                    // Número total de processos
    InfoProcesso processos[N_NUM_PROCESSOS]; // Info de todos os processos
    int dono_do_bloco[K_NUM_BLOCOS];     // Mapeamento bloco→dono
    byte **minha_memoria_local;           // Blocos locais deste processo
    BlocoCache meu_cache[K_NUM_BLOCOS];  // Cache para blocos remotos
    pthread_t thread_servidor;           // Thread para comunicação
    // ... outros campos
} SistemaDSM;
```

#### 2. Cache de Blocos (`BlocoCache` em `dsm.h:57-62`)
```c
typedef struct {
    int id_bloco;                        // ID do bloco
    int valido;                          // Flag de validade (0/1)
    byte dados[T_TAMANHO_BLOCO];        // Dados do bloco (4KB)
    pthread_mutex_t mutex;               // Sincronização
} BlocoCache;
```

### Distribuição de Blocos
**Implementação**: Função `calcular_dono_bloco()` em `dsm.c:135`
```c
int calcular_dono_bloco(int id_bloco) {
    return id_bloco % dsm_global->num_processos;  // Distribuição por módulo
}
```

**Resultado**:
- Processo 0: blocos 0, 4, 8, 12, ... (256 blocos)
- Processo 1: blocos 1, 5, 9, 13, ... (256 blocos)  
- Processo 2: blocos 2, 6, 10, 14, ... (256 blocos)
- Processo 3: blocos 3, 7, 11, 15, ... (256 blocos)

## 🔧 API Implementada

### Função de Leitura
**Especificação**: `int le(int posicao, byte buffer, int tamanho)`
**Implementação**: `dsm.c:545-631`

```c
int le(int posicao, byte *buffer, int tamanho)
```

**Funcionalidades**:
- ✅ Validação de parâmetros e limites
- ✅ Cálculo de bloco e offset: `id_bloco = posicao / T_TAMANHO_BLOCO`
- ✅ Acesso local direto para blocos próprios
- ✅ Cache hit/miss para blocos remotos
- ✅ Requisição automática de blocos remotos

### Função de Escrita
**Especificação**: `int escreve(int posicao, byte buffer, int tamanho)`
**Implementação**: `dsm.c:635-700`

```c
int escreve(int posicao, byte *buffer, int tamanho)
```

**Funcionalidades**:
- ✅ Validação de parâmetros e limites
- ✅ Escrita apenas em blocos locais (ownership)
- ✅ Protocolo Write-Invalidate automático
- ✅ Invalidação de caches remotos

## 🔄 Protocolo de Coerência de Cache

### Write-Invalidate Protocol
**Implementação**: `dsm.c:250-270`

#### Cenário de Escrita:
1. **Validação**: Processo só pode escrever em blocos próprios
2. **Escrita Local**: Atualiza memória local
3. **Invalidação**: Envia mensagem `MSG_INVALIDAR_BLOCO` para todos os outros processos
4. **Confirmação**: Recebe ACKs das invalidações

#### Cenário de Leitura:
1. **Bloco Local**: Acesso direto à memória local
2. **Cache Hit**: Retorna dados do cache local
3. **Cache Miss**: Requisita bloco do dono via `MSG_REQUISICAO_BLOCO`

### Tipos de Mensagem (`dsm.h:22-27`)
```c
typedef enum {
    MSG_REQUISICAO_BLOCO = 1,    // Solicitar bloco remoto
    MSG_RESPOSTA_BLOCO = 2,      // Enviar dados do bloco
    MSG_INVALIDAR_BLOCO = 3,     // Invalidar cache
    MSG_ACK_INVALIDACAO = 4      // Confirmar invalidação
} TipoMensagem;
```

### Thread Servidora
**Implementação**: `dsm.c:276-375`

Processa mensagens de rede:
- **Requisições de blocos**: Envia dados para outros processos
- **Invalidações**: Marca blocos como inválidos no cache local
- **Comunicação TCP**: Usa sockets para comunicação inter-processos

## 📁 Arquivos do Projeto

### Código Principal
- **`dsm.h`** - Definições, estruturas e protótipos da API
- **`dsm.c`** - Implementação completa do sistema DSM
- **`test_dsm.c`** - Programa de teste e interface interativa

### Scripts de Teste
- **`test_automated.sh`** - Script para teste automatizado com 4 processos

## 🚀 Compilação e Execução

### Compilação
```bash
gcc -Wall -Wextra -std=c99 -pthread -g -o test_dsm dsm.c test_dsm.c
```

### Execução Básica
```bash
# Processo único (teste limitado)
./test_dsm 0

# Modo automático (sem interface interativa)
./test_dsm 0 auto
```

### 🎨 Sistema de Logs Hierárquico e Colorido
- **Estrutura visual com indentação**:
  - `█ SEÇÕES PRINCIPAIS` - Cabeçalhos de grandes blocos
  - `  ▶ Subseções` - Passos específicos dos testes  
  - `    • Detalhes` - Informações complementares
  - `      [P0] Comunicação` - Mensagens entre processos

- **Cada processo tem sua cor específica**:
  - Processo 0: 🔵 Azul
  - Processo 1: 🟣 Magenta  
  - Processo 2: 🔷 Ciano
  - Processo 3: 🟡 Amarelo

- **Tipos de mensagem por cor**:
  - ✅ **Verde**: Sucessos
  - ❌ **Vermelho**: Erros
  - 📋 **Branco bold**: Seções e passos
  - 📝 **Branco**: Detalhes e informações
  - 🔍 **Cinza**: Debug e comunicação

### Execução Completa (4 Processos)

**Opção 1: Terminais separados (recomendado)**
```bash
# Terminal 1
./test_dsm 0

# Terminal 2
./test_dsm 1

# Terminal 3
./test_dsm 2

# Terminal 4
./test_dsm 3
```

**Opção 2: Background**
```bash
./test_dsm 0 &
./test_dsm 1 &
./test_dsm 2 &
./test_dsm 3 &
wait
```

**Opção 3: Script automatizado (modo automático com cores)**
```bash
./test_automated.sh
```
> Este script executa todos os processos em modo automático com logs coloridos, sem interface interativa.

## 🧪 Casos de Teste

### 1. Teste Básico Automático
**Arquivo**: `test_dsm.c:12-62`

#### Testes Executados:
- ✅ **Escrita Local**: Escreve "Hello DSM World!" em bloco próprio
- ✅ **Leitura Local**: Lê dados do mesmo bloco local
- ✅ **Leitura Remota (Cache Miss)**: Primeira leitura de bloco remoto
- ✅ **Leitura Remota (Cache Hit)**: Segunda leitura do mesmo bloco
- ✅ **Escrita Remota (Rejected)**: Tentativa de escrita em bloco alheio

### 2. Modo Interativo
**Arquivo**: `test_dsm.c:64-146`

#### Comandos Disponíveis:
```
r <posicao> <tamanho>  - Ler dados
w <posicao> <dados>    - Escrever dados  
s                      - Estatísticas
c                      - Estado do cache
q                      - Sair
```

#### Exemplos de Uso:
```bash
DSM[0]> w 0 "Processo0"        # Escreve em bloco próprio
DSM[0]> r 0 9                  # Lê dados escritos
DSM[0]> r 4096 16              # Lê bloco remoto (processo 1)
DSM[0]> s                      # Mostra estatísticas
DSM[0]> c                      # Estado do cache
```

### 3. Verificação de Coerência

#### Cenário de Teste Manual:
1. **Processo 0**: `w 0 "dados_v1"` (escreve em bloco 0)
2. **Processo 1**: `r 0 8` (lê bloco 0 - cache miss)
3. **Processo 0**: `w 0 "dados_v2"` (atualiza bloco 0 - invalida cache do Processo 1)
4. **Processo 1**: `r 0 8` (lê novamente - deve ser cache miss devido à invalidação)

#### Monitoramento:
- **Estatísticas**: Comando `s` mostra hits/misses/invalidações
- **Debug**: Mensagens mostram comunicação entre processos
- **Cache**: Comando `c` mostra estado atual do cache

## 📊 Monitoramento e Debug

### Estatísticas por Processo
**Implementação**: `dsm.c:106-117`
- Cache hits e misses
- Invalidações enviadas e recebidas
- Taxa de acerto do cache

### Sistema de Logs Hierárquico
**Implementação**: `dsm.c:19-145`
- **Seções** (`log_section`): Cabeçalhos principais
- **Subseções** (`log_subsection`): Passos dos testes
- **Detalhes** (`log_detail`): Informações complementares
- **Comunicação** (`log_communication`): Mensagens entre processos
- **Sucessos/Erros** (`log_success`/`log_error`): Resultados dos testes

## 🛠️ Funcionalidades Implementadas

### ✅ Requisitos Atendidos
- [x] **API conforme especificação** (`le()` e `escreve()`)
- [x] **Distribuição de blocos** (distribuição por módulo)
- [x] **Cache local** com protocolo Write-Invalidate
- [x] **Comunicação TCP** entre processos
- [x] **Sincronização** com pthreads e mutexes
- [x] **Testes de coerência** automáticos e interativos
- [x] **Estatísticas** de performance
- [x] **Interface interativa** para testes manuais
- [x] **Sistema de logs coloridos** por processo e tipo
- [x] **Modo automático** para execução sem interação

### 🔧 Limitações Conhecidas
- Comunicação simplificada (não totalmente assíncrona)
- Suporte apenas para acessos dentro de um bloco
- Rede local apenas (localhost)

## 🚨 Comandos de Limpeza

```bash
# Parar todos os processos
pkill -f test_dsm

# Remover executável
rm -f test_dsm
```

---
**Desenvolvido para**: Programação Paralela e Distribuída - UFSC  
**Paradigma**: Memória Compartilhada Distribuída com Write-Invalidate Protocol 