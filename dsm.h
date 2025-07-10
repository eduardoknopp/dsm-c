#ifndef DSM_H
#define DSM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

// Configurações do sistema DSM
#define K_NUM_BLOCOS 1024
#define T_TAMANHO_BLOCO 4096  // 4KB por bloco
#define N_NUM_PROCESSOS 4
#define TAMANHO_MEMORIA_TOTAL (K_NUM_BLOCOS * T_TAMANHO_BLOCO)

// Tipos de mensagem para comunicação
typedef enum {
    MSG_REQUISICAO_BLOCO = 1,
    MSG_RESPOSTA_BLOCO = 2,
    MSG_INVALIDAR_BLOCO = 3,
    MSG_ACK_INVALIDACAO = 4
} TipoMensagem;

// Tipo para representar um byte
typedef uint8_t byte;

// Códigos de cor ANSI
#define COLOR_RESET   "\033[0m"

// Cores para tipos de log
#define COLOR_SUCCESS "\033[1;32m"  // Verde bold
#define COLOR_ERROR   "\033[1;31m"  // Vermelho bold
#define COLOR_DEFAULT "\033[0;37m"  // Branco normal
#define COLOR_STEP    "\033[1;37m"  // Branco bold

// Tipos de log
typedef enum {
    LOG_DEFAULT = 0,
    LOG_STEP = 1,
    LOG_SUCCESS = 2,
    LOG_ERROR = 3
} TipoLog;

// Estrutura para um bloco no cache
typedef struct {
    int id_bloco;
    int valido;  // 1 se válido, 0 se inválido
    byte dados[T_TAMANHO_BLOCO];
    pthread_mutex_t mutex;  // Para sincronização
} BlocoCache;

// Estrutura para mensagens de rede
typedef struct {
    TipoMensagem tipo;
    int id_bloco;
    int tamanho_dados;
    byte dados[T_TAMANHO_BLOCO];
} Mensagem;

// Estrutura para informações de processo
typedef struct {
    int id;
    char ip[16];
    int porta;
} InfoProcesso;

// Estrutura principal do sistema DSM
typedef struct {
    int meu_id;
    int num_processos;
    
    // Informações dos processos
    InfoProcesso processos[N_NUM_PROCESSOS];
    
    // Mapeamento de donos dos blocos
    int dono_do_bloco[K_NUM_BLOCOS];
    
    // Memória local (blocos que este processo possui)
    byte **minha_memoria_local;
    int num_blocos_locais;
    int *meus_blocos;  // Array com IDs dos blocos que possuo
    
    // Cache local
    BlocoCache meu_cache[K_NUM_BLOCOS];
    
    // Thread de escuta
    pthread_t thread_servidor;
    int socket_servidor;
    int servidor_rodando;
    
    // Mutex para sincronização geral
    pthread_mutex_t mutex_global;
    
} SistemaDSM;

// Variável global do sistema
extern SistemaDSM *dsm_global;

// Protótipos das funções principais
int dsm_init(int meu_id, InfoProcesso processos[], int num_processos);
int dsm_cleanup(void);
void* thread_servidora(void* arg);

// API pública
int le(int posicao, byte *buffer, int tamanho);
int escreve(int posicao, byte *buffer, int tamanho);

// Funções auxiliares
int calcular_dono_bloco(int id_bloco);
int e_meu_bloco(int id_bloco);
int enviar_mensagem(int id_processo_destino, Mensagem *msg);
int receber_mensagem(int socket_cliente, Mensagem *msg);
BlocoCache* obter_bloco_cache(int id_bloco);
int requisitar_bloco_remoto(int id_bloco);
int invalidar_caches_remotos(int id_bloco);
void imprimir_estatisticas(void);

// Função de log padronizada
void log_padronizado(const char *cor, const char *prefixo, const char *formato, ...);
void imprimir_estado_cache(void);

#endif // DSM_H 