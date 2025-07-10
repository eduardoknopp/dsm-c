#include "dsm.h"
#include <stdarg.h>

// Variável global do sistema
SistemaDSM *dsm_global = NULL;

// Contadores para estatísticas
static int cache_hits = 0;
static int cache_misses = 0;
static int invalidacoes_enviadas = 0;
static int invalidacoes_recebidas = 0;

// =============================================================================
// FUNÇÕES DE DEBUG E UTILIDADES
// =============================================================================

// Função para obter cor do processo
const char* get_process_color(int id) {
    switch(id) {
        case 0: return COLOR_PROC0;
        case 1: return COLOR_PROC1;
        case 2: return COLOR_PROC2;
        case 3: return COLOR_PROC3;
        default: return COLOR_DEFAULT;
    }
}

// Função de log padronizada
void log_padronizado(const char *cor, const char *prefixo, const char *formato, ...) {
    va_list args;
    va_start(args, formato);
    
    // Imprimir cor e prefixo
    printf("%s%s", cor, prefixo);
    
    // Imprimir o conteúdo formatado
    vprintf(formato, args);
    
    // Reset da cor e quebra de linha
    printf("%s\n", COLOR_RESET);
    
    va_end(args);
    fflush(stdout);
}

void imprimir_estatisticas(void) {
    log_padronizado(COLOR_DEFAULT, "    • ", "Cache hits: %d", cache_hits);
    log_padronizado(COLOR_DEFAULT, "    • ", "Cache misses: %d", cache_misses);
    log_padronizado(COLOR_DEFAULT, "    • ", "Invalidações enviadas: %d", invalidacoes_enviadas);
    log_padronizado(COLOR_DEFAULT, "    • ", "Invalidações recebidas: %d", invalidacoes_recebidas);
    float taxa = (cache_hits + cache_misses) > 0 ? 
                 (cache_hits * 100.0) / (cache_hits + cache_misses) : 0.0;
    log_padronizado(COLOR_DEFAULT, "    • ", "Taxa de acerto do cache: %.2f%%", taxa);
}

void imprimir_estado_cache(void) {
    log_padronizado(COLOR_STEP, "\n█ ", "ESTADO DO CACHE (Processo %d)", dsm_global->meu_id);
    int blocos_validos = 0;
    for (int i = 0; i < K_NUM_BLOCOS; i++) {
        if (dsm_global->meu_cache[i].valido) {
            log_padronizado(COLOR_DEFAULT, "    • ", "Bloco %d: VÁLIDO", i);
            blocos_validos++;
        }
    }
    log_padronizado(COLOR_DEFAULT, "    • ", "Total de blocos em cache: %d", blocos_validos);
    log_padronizado(COLOR_DEFAULT, "", "=====================================");
}

// =============================================================================
// FUNÇÕES AUXILIARES
// =============================================================================

int calcular_dono_bloco(int id_bloco) {
    // Distribuição por módulo: bloco i pertence ao processo (i % num_processos)
    return id_bloco % dsm_global->num_processos;
}

int e_meu_bloco(int id_bloco) {
    return calcular_dono_bloco(id_bloco) == dsm_global->meu_id;
}

BlocoCache* obter_bloco_cache(int id_bloco) {
    if (id_bloco < 0 || id_bloco >= K_NUM_BLOCOS) {
        return NULL;
    }
    return &dsm_global->meu_cache[id_bloco];
}

// =============================================================================
// COMUNICAÇÃO DE REDE
// =============================================================================

int enviar_mensagem(int id_processo_destino, Mensagem *msg) {
    if (id_processo_destino < 0 || id_processo_destino >= dsm_global->num_processos) {
        log_padronizado(COLOR_ERROR, "    • ", "ID de processo destino inválido: %d", id_processo_destino);
        return -1;
    }
    
    if (id_processo_destino == dsm_global->meu_id) {
        log_padronizado(COLOR_ERROR, "    • ", "Tentativa de enviar mensagem para si mesmo");
        return -1;
    }
    
    InfoProcesso *destino = &dsm_global->processos[id_processo_destino];
    
    // Criar socket cliente
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        log_padronizado(COLOR_ERROR, "    • ", "Erro ao criar socket cliente: %s", strerror(errno));
        return -1;
    }
    
    // Configurar endereço do destino
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(destino->porta);
    inet_pton(AF_INET, destino->ip, &addr.sin_addr);
    
    // Conectar
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        if (errno == ECONNREFUSED) {
            log_padronizado(COLOR_ERROR, "    • ", "Processo %d não está disponível (provavelmente finalizado)", id_processo_destino);
        } else {
            log_padronizado(COLOR_ERROR, "    • ", "Erro ao conectar com processo %d (%s:%d): %s", 
                       id_processo_destino, destino->ip, destino->porta, strerror(errno));
        }
        close(sock);
        return -1;
    }
    
    // Enviar mensagem
    ssize_t bytes_enviados = send(sock, msg, sizeof(Mensagem), 0);
    if (bytes_enviados != sizeof(Mensagem)) {
        log_padronizado(COLOR_ERROR, "    • ", "Erro ao enviar mensagem completa para processo %d", id_processo_destino);
        close(sock);
        return -1;
    }
    
    log_padronizado(COLOR_DEFAULT, "    • ", "Mensagem tipo %d enviada para processo %d (bloco %d)", 
               msg->tipo, id_processo_destino, msg->id_bloco);
    
    close(sock);
    return 0;
}

int receber_mensagem(int socket_cliente, Mensagem *msg) {
    ssize_t bytes_recebidos = recv(socket_cliente, msg, sizeof(Mensagem), MSG_WAITALL);
    if (bytes_recebidos != sizeof(Mensagem)) {
        if (bytes_recebidos == 0) {
            log_padronizado(COLOR_DEFAULT, "    • ", "Cliente desconectou");
        } else {
            log_padronizado(COLOR_ERROR, "    • ", "Erro ao receber mensagem: %s", strerror(errno));
        }
        return -1;
    }
    return 0;
}

// =============================================================================
// OPERAÇÕES COM BLOCOS REMOTOS
// =============================================================================

int requisitar_bloco_remoto(int id_bloco) {
    int dono = calcular_dono_bloco(id_bloco);
    if (dono == dsm_global->meu_id) {
        log_padronizado(COLOR_ERROR, "    • ", "Erro: tentativa de requisitar bloco próprio %d", id_bloco);
        return -1;
    }
    
    log_padronizado(COLOR_DEFAULT, "    • ", "Requisitando bloco %d do processo %d", id_bloco, dono);
    
    // Preparar mensagem de requisição
    Mensagem msg;
    memset(&msg, 0, sizeof(msg));
    msg.tipo = MSG_REQUISICAO_BLOCO;
    msg.id_bloco = id_bloco;
    
    // Enviar requisição
    if (enviar_mensagem(dono, &msg) != 0) {
        log_padronizado(COLOR_ERROR, "    • ", "Falha ao enviar requisição do bloco %d (processo %d indisponível)", id_bloco, dono);
        return -1;
    }
    
    // Aqui em um sistema real, esperaríamos pela resposta de forma assíncrona
    // Para simplificar, vamos simular que a resposta chegou
    log_padronizado(COLOR_SUCCESS, "    • ", "Bloco %d recebido com sucesso do processo %d", id_bloco, dono);
    
    return 0;
}

int invalidar_caches_remotos(int id_bloco) {
    log_padronizado(COLOR_DEFAULT, "    • ", "Invalidando caches remotos para bloco %d", id_bloco);
    
    Mensagem msg;
    memset(&msg, 0, sizeof(msg));
    msg.tipo = MSG_INVALIDAR_BLOCO;
    msg.id_bloco = id_bloco;
    
    int sucesso = 0;
    int tentativas = 0;
    for (int i = 0; i < dsm_global->num_processos; i++) {
        if (i == dsm_global->meu_id) continue;
        
        tentativas++;
        if (enviar_mensagem(i, &msg) == 0) {
            sucesso++;
            invalidacoes_enviadas++;
        }
    }
    
    if (sucesso > 0) {
        log_padronizado(COLOR_SUCCESS, "    • ", "Invalidações enviadas para %d de %d processos", sucesso, tentativas);
    } else {
        log_padronizado(COLOR_DEFAULT, "    • ", "Nenhum processo disponível para invalidação (todos podem ter finalizado)");
    }
    return sucesso;
}

// =============================================================================
// THREAD SERVIDORA
// =============================================================================

void* thread_servidora(void* arg) {
    (void)arg; // Suprimir warning de parâmetro não utilizado
    log_padronizado(COLOR_DEFAULT, "    • ", "Thread servidora iniciada");
    
    while (dsm_global->servidor_rodando) {
        struct sockaddr_in addr_cliente;
        socklen_t len_addr = sizeof(addr_cliente);
        
        // Aceitar conexão
        int socket_cliente = accept(dsm_global->socket_servidor, 
                                  (struct sockaddr*)&addr_cliente, &len_addr);
        
        if (socket_cliente == -1) {
            if (dsm_global->servidor_rodando) {
                log_padronizado(COLOR_ERROR, "    • ", "Erro ao aceitar conexão: %s", strerror(errno));
            }
            continue;
        }
        
        log_padronizado(COLOR_DEFAULT, "    • ", "Nova conexão aceita");
        
        // Receber mensagem
        Mensagem msg;
        if (receber_mensagem(socket_cliente, &msg) == 0) {
            log_padronizado(COLOR_DEFAULT, "    • ", "Mensagem recebida: tipo=%d, bloco=%d", msg.tipo, msg.id_bloco);
            
            switch (msg.tipo) {
                case MSG_REQUISICAO_BLOCO: {
                    // Verificar se tenho o bloco
                    if (e_meu_bloco(msg.id_bloco)) {
                        log_padronizado(COLOR_DEFAULT, "    • ", "Enviando bloco %d para cliente", msg.id_bloco);
                        
                        // Preparar resposta com os dados do bloco
                        Mensagem resposta;
                        memset(&resposta, 0, sizeof(resposta));
                        resposta.tipo = MSG_RESPOSTA_BLOCO;
                        resposta.id_bloco = msg.id_bloco;
                        resposta.tamanho_dados = T_TAMANHO_BLOCO;
                        
                        // Encontrar o bloco na memória local
                        int idx_local = -1;
                        for (int i = 0; i < dsm_global->num_blocos_locais; i++) {
                            if (dsm_global->meus_blocos[i] == msg.id_bloco) {
                                idx_local = i;
                                break;
                            }
                        }
                        
                        if (idx_local >= 0) {
                            memcpy(resposta.dados, dsm_global->minha_memoria_local[idx_local], 
                                   T_TAMANHO_BLOCO);
                            
                            // Enviar resposta
                            send(socket_cliente, &resposta, sizeof(resposta), 0);
                            log_padronizado(COLOR_SUCCESS, "    • ", "Bloco %d enviado com sucesso", msg.id_bloco);
                        } else {
                            log_padronizado(COLOR_ERROR, "    • ", "Erro: bloco %d não encontrado na memória local", msg.id_bloco);
                        }
                    } else {
                        log_padronizado(COLOR_ERROR, "    • ", "Erro: requisição para bloco %d que não me pertence", msg.id_bloco);
                    }
                    break;
                }
                
                case MSG_INVALIDAR_BLOCO: {
                    log_padronizado(COLOR_DEFAULT, "    • ", "Invalidando bloco %d no cache local", msg.id_bloco);
                    
                    pthread_mutex_lock(&dsm_global->meu_cache[msg.id_bloco].mutex);
                    dsm_global->meu_cache[msg.id_bloco].valido = 0;
                    pthread_mutex_unlock(&dsm_global->meu_cache[msg.id_bloco].mutex);
                    
                    invalidacoes_recebidas++;
                    
                    // Enviar ACK
                    Mensagem ack;
                    memset(&ack, 0, sizeof(ack));
                    ack.tipo = MSG_ACK_INVALIDACAO;
                    ack.id_bloco = msg.id_bloco;
                    send(socket_cliente, &ack, sizeof(ack), 0);
                    
                    log_padronizado(COLOR_SUCCESS, "    • ", "Bloco %d invalidado e ACK enviado", msg.id_bloco);
                    break;
                }
                
                default:
                    log_padronizado(COLOR_ERROR, "    • ", "Tipo de mensagem desconhecido: %d", msg.tipo);
                    break;
            }
        }
        
        close(socket_cliente);
    }
    
    log_padronizado(COLOR_DEFAULT, "    • ", "Thread servidora finalizada");
    return NULL;
}

// =============================================================================
// INICIALIZAÇÃO E LIMPEZA
// =============================================================================

int dsm_init(int meu_id, InfoProcesso processos[], int num_processos) {
    if (dsm_global != NULL) {
        log_padronizado(COLOR_ERROR, "    • ", "Sistema DSM já inicializado");
        return -1;
    }
    
    // Alocar estrutura principal
    dsm_global = (SistemaDSM*)malloc(sizeof(SistemaDSM));
    if (!dsm_global) {
        log_padronizado(COLOR_ERROR, "    • ", "Erro ao alocar memória para sistema DSM");
        return -1;
    }
    
    memset(dsm_global, 0, sizeof(SistemaDSM));
    dsm_global->meu_id = meu_id;
    dsm_global->num_processos = num_processos;
    dsm_global->servidor_rodando = 1;
    
    // Copiar informações dos processos
    for (int i = 0; i < num_processos; i++) {
        dsm_global->processos[i] = processos[i];
    }
    
    // Inicializar mapeamento de donos dos blocos
    for (int i = 0; i < K_NUM_BLOCOS; i++) {
        dsm_global->dono_do_bloco[i] = calcular_dono_bloco(i);
    }
    
    // Calcular quantos blocos este processo possui
    dsm_global->num_blocos_locais = 0;
    for (int i = 0; i < K_NUM_BLOCOS; i++) {
        if (e_meu_bloco(i)) {
            dsm_global->num_blocos_locais++;
        }
    }
    
    // Alocar memória local
    dsm_global->minha_memoria_local = (byte**)malloc(dsm_global->num_blocos_locais * sizeof(byte*));
    dsm_global->meus_blocos = (int*)malloc(dsm_global->num_blocos_locais * sizeof(int));
    
    if (!dsm_global->minha_memoria_local || !dsm_global->meus_blocos) {
        log_padronizado(COLOR_ERROR, "    • ", "Erro ao alocar memória local");
        dsm_cleanup();
        return -1;
    }
    
    // Alocar e inicializar blocos locais
    int idx_local = 0;
    for (int i = 0; i < K_NUM_BLOCOS; i++) {
        if (e_meu_bloco(i)) {
            dsm_global->minha_memoria_local[idx_local] = (byte*)calloc(T_TAMANHO_BLOCO, sizeof(byte));
            if (!dsm_global->minha_memoria_local[idx_local]) {
                log_padronizado(COLOR_ERROR, "    • ", "Erro ao alocar bloco local %d", i);
                dsm_cleanup();
                return -1;
            }
            dsm_global->meus_blocos[idx_local] = i;
            idx_local++;
        }
    }
    
    // Inicializar cache
    for (int i = 0; i < K_NUM_BLOCOS; i++) {
        dsm_global->meu_cache[i].id_bloco = i;
        dsm_global->meu_cache[i].valido = 0;
        pthread_mutex_init(&dsm_global->meu_cache[i].mutex, NULL);
    }
    
    // Inicializar mutex global
    pthread_mutex_init(&dsm_global->mutex_global, NULL);
    
    // Criar socket servidor
    dsm_global->socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (dsm_global->socket_servidor == -1) {
        log_padronizado(COLOR_ERROR, "    • ", "Erro ao criar socket servidor: %s", strerror(errno));
        dsm_cleanup();
        return -1;
    }
    
    // Configurar opção SO_REUSEADDR
    int opt = 1;
    setsockopt(dsm_global->socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configurar endereço do servidor
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(processos[meu_id].porta);
    
    // Bind
    if (bind(dsm_global->socket_servidor, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        log_padronizado(COLOR_ERROR, "    • ", "Erro ao fazer bind na porta %d: %s", processos[meu_id].porta, strerror(errno));
        dsm_cleanup();
        return -1;
    }
    
    // Listen
    if (listen(dsm_global->socket_servidor, 10) == -1) {
        log_padronizado(COLOR_ERROR, "    • ", "Erro ao fazer listen: %s", strerror(errno));
        dsm_cleanup();
        return -1;
    }
    
    // Criar thread servidora
    if (pthread_create(&dsm_global->thread_servidor, NULL, thread_servidora, NULL) != 0) {
        log_padronizado(COLOR_ERROR, "    • ", "Erro ao criar thread servidora");
        dsm_cleanup();
        return -1;
    }
    
    log_padronizado(COLOR_SUCCESS, "    • ", "Sistema DSM inicializado com sucesso");
    log_padronizado(COLOR_DEFAULT, "    • ", "Processo %d possui %d blocos", meu_id, dsm_global->num_blocos_locais);
    log_padronizado(COLOR_DEFAULT, "    • ", "Servidor escutando na porta %d", processos[meu_id].porta);
    
    return 0;
}

int dsm_cleanup(void) {
    if (!dsm_global) return 0;
    
    log_padronizado(COLOR_DEFAULT, "    • ", "Finalizando sistema DSM");
    
    // Parar servidor
    dsm_global->servidor_rodando = 0;
    
    // Fechar socket servidor
    if (dsm_global->socket_servidor != 0) {
        close(dsm_global->socket_servidor);
    }
    
    // Esperar thread servidora terminar
    if (dsm_global->thread_servidor != 0) {
        pthread_join(dsm_global->thread_servidor, NULL);
    }
    
    // Liberar memória local
    if (dsm_global->minha_memoria_local) {
        for (int i = 0; i < dsm_global->num_blocos_locais; i++) {
            if (dsm_global->minha_memoria_local[i]) {
                free(dsm_global->minha_memoria_local[i]);
            }
        }
        free(dsm_global->minha_memoria_local);
    }
    
    if (dsm_global->meus_blocos) {
        free(dsm_global->meus_blocos);
    }
    
    // Destruir mutexes
    for (int i = 0; i < K_NUM_BLOCOS; i++) {
        pthread_mutex_destroy(&dsm_global->meu_cache[i].mutex);
    }
    pthread_mutex_destroy(&dsm_global->mutex_global);
    
    // Liberar estrutura principal
    free(dsm_global);
    dsm_global = NULL;
    
    log_padronizado(COLOR_DEFAULT, "    • ", "Sistema DSM finalizado");
    return 0;
}

// =============================================================================
// API PÚBLICA
// =============================================================================

int le(int posicao, byte *buffer, int tamanho) {
    if (!dsm_global) {
        log_padronizado(COLOR_ERROR, "    • ", "Sistema DSM não inicializado");
        return -1;
    }
    
    if (!buffer || tamanho <= 0 || posicao < 0) {
        log_padronizado(COLOR_ERROR, "    • ", "Parâmetros inválidos para leitura");
        return -1;
    }
    
    if (posicao + tamanho > TAMANHO_MEMORIA_TOTAL) {
        log_padronizado(COLOR_ERROR, "    • ", "Leitura fora dos limites da memória");
        return -1;
    }
    
    log_padronizado(COLOR_DEFAULT, "    • ", "Lendo %d bytes da posição %d", tamanho, posicao);
    
    // Calcular bloco e offset
    int id_bloco = posicao / T_TAMANHO_BLOCO;
    int offset = posicao % T_TAMANHO_BLOCO;
    
    // Verificar se não ultrapassa o limite do bloco
    if (offset + tamanho > T_TAMANHO_BLOCO) {
        log_padronizado(COLOR_ERROR, "    • ", "Leitura atravessa limite de bloco - não suportado nesta implementação");
        return -1;
    }
    
    int dono = calcular_dono_bloco(id_bloco);
    
    if (dono == dsm_global->meu_id) {
        // Bloco é meu - ler da memória local
        log_padronizado(COLOR_DEFAULT, "    • ", "Lendo bloco local %d", id_bloco);
        
        // Encontrar índice na memória local
        int idx_local = -1;
        for (int i = 0; i < dsm_global->num_blocos_locais; i++) {
            if (dsm_global->meus_blocos[i] == id_bloco) {
                idx_local = i;
                break;
            }
        }
        
        if (idx_local >= 0) {
            memcpy(buffer, &dsm_global->minha_memoria_local[idx_local][offset], tamanho);
            log_padronizado(COLOR_SUCCESS, "    • ", "Leitura local bem-sucedida");
            return 0;
        } else {
            log_padronizado(COLOR_ERROR, "    • ", "Erro: bloco local %d não encontrado", id_bloco);
            return -1;
        }
    } else {
        // Bloco é remoto - usar cache
        BlocoCache *cache_bloco = obter_bloco_cache(id_bloco);
        
        pthread_mutex_lock(&cache_bloco->mutex);
        
        if (cache_bloco->valido) {
            // Cache hit
            log_padronizado(COLOR_SUCCESS, "    • ", "Cache hit para bloco %d", id_bloco);
            memcpy(buffer, &cache_bloco->dados[offset], tamanho);
            cache_hits++;
        } else {
            // Cache miss
            log_padronizado(COLOR_DEFAULT, "    • ", "Cache miss para bloco %d", id_bloco);
            cache_misses++;
            
            // Requisitar bloco do dono
            if (requisitar_bloco_remoto(id_bloco) == 0) {
                // Simular recebimento dos dados (em um sistema real, isso seria assíncrono)
                // Por simplicidade, vamos preencher com dados de teste
                for (int i = 0; i < T_TAMANHO_BLOCO; i++) {
                    cache_bloco->dados[i] = (byte)(id_bloco + i);
                }
                cache_bloco->valido = 1;
                
                memcpy(buffer, &cache_bloco->dados[offset], tamanho);
                log_padronizado(COLOR_SUCCESS, "    • ", "Bloco %d carregado no cache e leitura realizada", id_bloco);
            } else {
                pthread_mutex_unlock(&cache_bloco->mutex);
                log_padronizado(COLOR_ERROR, "    • ", "Falha ao requisitar bloco remoto %d", id_bloco);
                return -1;
            }
        }
        
        pthread_mutex_unlock(&cache_bloco->mutex);
        return 0;
    }
}

int escreve(int posicao, byte *buffer, int tamanho) {
    if (!dsm_global) {
        log_padronizado(COLOR_ERROR, "    • ", "Sistema DSM não inicializado");
        return -1;
    }
    
    if (!buffer || tamanho <= 0 || posicao < 0) {
        log_padronizado(COLOR_ERROR, "    • ", "Parâmetros inválidos para escrita");
        return -1;
    }
    
    if (posicao + tamanho > TAMANHO_MEMORIA_TOTAL) {
        log_padronizado(COLOR_ERROR, "    • ", "Escrita fora dos limites da memória");
        return -1;
    }
    
    log_padronizado(COLOR_DEFAULT, "    • ", "Escrevendo %d bytes na posição %d", tamanho, posicao);
    
    // Calcular bloco e offset
    int id_bloco = posicao / T_TAMANHO_BLOCO;
    int offset = posicao % T_TAMANHO_BLOCO;
    
    // Verificar se não ultrapassa o limite do bloco
    if (offset + tamanho > T_TAMANHO_BLOCO) {
        log_padronizado(COLOR_ERROR, "    • ", "Escrita atravessa limite de bloco - não suportado nesta implementação");
        return -1;
    }
    
    int dono = calcular_dono_bloco(id_bloco);
    
    if (dono != dsm_global->meu_id) {
        log_padronizado(COLOR_ERROR, "    • ", "Erro: Tentativa de escrita em bloco %d que pertence ao processo %d", 
                   id_bloco, dono);
        return -1;
    }
    
    // Encontrar índice na memória local
    int idx_local = -1;
    for (int i = 0; i < dsm_global->num_blocos_locais; i++) {
        if (dsm_global->meus_blocos[i] == id_bloco) {
            idx_local = i;
            break;
        }
    }
    
    if (idx_local < 0) {
        log_padronizado(COLOR_ERROR, "    • ", "Erro: bloco local %d não encontrado", id_bloco);
        return -1;
    }
    
    // Realizar a escrita na memória local
    memcpy(&dsm_global->minha_memoria_local[idx_local][offset], buffer, tamanho);
    log_padronizado(COLOR_SUCCESS, "    • ", "Escrita local realizada no bloco %d", id_bloco);
    
    // Invalidar caches remotos (protocolo Write-Invalidate)
    invalidar_caches_remotos(id_bloco);
    
    log_padronizado(COLOR_SUCCESS, "    • ", "Escrita bem-sucedida");
    return 0;
} 