#include "dsm.h"
#include <signal.h>

// Flag para controle de execução
static volatile int continuar_executando = 1;
static int modo_automatico = 0;
static volatile int signal_recebido = 0; // Flag para evitar logs duplicados

void signal_handler(int sig) {
    (void)sig; // Suprimir warning de parâmetro não utilizado
    
    // Evitar execução múltipla do handler
    if (signal_recebido) {
        return;
    }
    signal_recebido = 1;
    
    continuar_executando = 0;
    int id = (dsm_global != NULL) ? dsm_global->meu_id : -1;
    log_padronizado(COLOR_DEFAULT, "\n    • ", "[P%d] Recebido sinal de interrupção. Finalizando...", id);
}

void teste_basico() {
    int id = dsm_global->meu_id;
    log_padronizado(COLOR_STEP, "\n█ ","[P%d] TESTE BÁSICO DO SISTEMA DSM", id);
    
    // Teste de escrita e leitura local
    byte dados_escrita[] = "Hello DSM World!";
    byte buffer_leitura[100];
    
    log_padronizado(COLOR_STEP, "\n  ▶ ","[P%d] 1. Testando escrita em bloco local", id);
    
    // Encontrar um bloco que pertence a este processo
    int bloco_teste = dsm_global->meu_id;  // Primeiro bloco que me pertence
    int posicao = bloco_teste * T_TAMANHO_BLOCO;
    
    if (escreve(posicao, dados_escrita, strlen((char*)dados_escrita) + 1) == 0) {
        log_padronizado(COLOR_SUCCESS, "\n  ▶ ","[P%d] 1.1 Escrita local bem-sucedida", id);
        
        // Testar leitura do mesmo bloco
        log_padronizado(COLOR_STEP, "\n  ▶ ","[P%d] 2. Testando leitura do bloco local", id);
        if (le(posicao, buffer_leitura, strlen((char*)dados_escrita) + 1) == 0) {
            log_padronizado(COLOR_SUCCESS, "\n  ▶ ","[P%d] 2.1 Leitura local bem-sucedida: '%s'", id, buffer_leitura);
        } else {
            log_padronizado(COLOR_ERROR, "\n  ▶ ","[P%d] 2.2 Falha na leitura local", id);
        }
    } else {
        log_padronizado(COLOR_ERROR, "\n  ▶ ","[P%d] 2.3 Falha na escrita local", id);
    }
    
    // Teste de acesso a bloco remoto
    log_padronizado(COLOR_STEP, "\n  ▶ ","[P%d] 3. Testando acesso a bloco remoto", id);
    int bloco_remoto = (dsm_global->meu_id + 1) % dsm_global->num_processos;
    int posicao_remota = bloco_remoto * T_TAMANHO_BLOCO;
    
    if (le(posicao_remota, buffer_leitura, 16) == 0) {
        log_padronizado(COLOR_SUCCESS, "\n  ▶ ","[P%d] 3.1 Leitura remota bem-sucedida (cache miss)", id);
        
        // Segunda leitura deve ser cache hit
        if (le(posicao_remota, buffer_leitura, 16) == 0) { 
            log_padronizado(COLOR_SUCCESS, "\n  ▶ ","[P%d] 3.2 Segunda leitura remota bem-sucedida (cache hit)", id);
        }
    } else {
        log_padronizado(COLOR_ERROR, "\n  ▶ ","[P%d] 3.3 Falha na leitura remota", id);
    }
    
    // Teste de escrita em bloco remoto (deve falhar)
    log_padronizado(COLOR_STEP, "\n  ▶ ","[P%d] 4. Testando escrita em bloco remoto (deve falhar)", id);
    if (escreve(posicao_remota, dados_escrita, strlen((char*)dados_escrita) + 1) != 0) {
        log_padronizado(COLOR_SUCCESS, "\n  ▶ ","[P%d] 4.1 Escrita remota rejeitada corretamente", id);
    } else {
        log_padronizado(COLOR_ERROR, "\n  ▶ ","[P%d] 4.2 Escrita remota aceita incorretamente", id);
    }
}

void teste_interativo() {
    int id = dsm_global->meu_id;
    log_padronizado(COLOR_STEP, "\n█ ","[P%d] MODO INTERATIVO", id);
    log_padronizado(COLOR_DEFAULT, "    • ","[P%d] Comandos disponíveis:", id);
    log_padronizado(COLOR_DEFAULT, "    • ","[P%d] r <posicao> <tamanho> - Ler dados", id);
    log_padronizado(COLOR_DEFAULT, "    • ","[P%d] w <posicao> <dados>   - Escrever dados", id);
    log_padronizado(COLOR_DEFAULT, "    • ","[P%d] s                     - Mostrar estatísticas", id);
    log_padronizado(COLOR_DEFAULT, "    • ","[P%d] c                     - Mostrar estado do cache", id);
    log_padronizado(COLOR_DEFAULT, "    • ","[P%d] q                     - Sair", id);
    log_padronizado(COLOR_DEFAULT, "    • ","[P%d] Digite 'help' para ver os comandos novamente\n", id);
    
    char linha[256];
    char comando[16];
    int posicao, tamanho;
    char dados[256];
    byte buffer[1024];
    
    while (continuar_executando) {
        printf("DSM[%d]> ", dsm_global->meu_id);
        fflush(stdout);
        
        if (!fgets(linha, sizeof(linha), stdin)) {
            break;
        }
        
        // Remover quebra de linha
        linha[strcspn(linha, "\n")] = '\0';
        
        if (strlen(linha) == 0) continue;
        
        sscanf(linha, "%s", comando);
        
        if (strcmp(comando, "q") == 0 || strcmp(comando, "quit") == 0) {
            break;
        } else if (strcmp(comando, "help") == 0) {
            log_padronizado(COLOR_DEFAULT, "    • ", "[P%d] Comandos:", id);
            log_padronizado(COLOR_DEFAULT, "    • ", "[P%d]   r <pos> <tam> - Ler", id);
            log_padronizado(COLOR_DEFAULT, "    • ", "[P%d]   w <pos> <dados> - Escrever", id);
            log_padronizado(COLOR_DEFAULT, "    • ", "[P%d]   s - Estatísticas", id);
            log_padronizado(COLOR_DEFAULT, "    • ", "[P%d]   c - Cache", id);
            log_padronizado(COLOR_DEFAULT, "    • ", "[P%d]   q - Sair", id);
        } else if (strcmp(comando, "r") == 0) {
            if (sscanf(linha, "r %d %d", &posicao, &tamanho) == 2) {
                if (tamanho > 0 && (size_t)tamanho <= sizeof(buffer)) {
                    if (le(posicao, buffer, tamanho) == 0) {
                        printf("Leitura bem-sucedida: ");
                        for (int i = 0; i < tamanho; i++) {
                            if (buffer[i] >= 32 && buffer[i] <= 126) {
                                printf("%c", buffer[i]);
                            } else {
                                printf("[%02x]", buffer[i]);
                            }
                        }
                        printf("\n");
                    } else {
                        log_padronizado(COLOR_ERROR, "    • ", "[P%d] Falha na leitura", id);
                    }
                } else {
                    log_padronizado(COLOR_ERROR, "    • ", "[P%d] Tamanho inválido", id);
                }
            } else {
                log_padronizado(COLOR_DEFAULT, "    • ", "[P%d] Uso: r <posicao> <tamanho>", id);
            }
        } else if (strcmp(comando, "w") == 0) {
            if (sscanf(linha, "w %d %255s", &posicao, dados) == 2) {
                if (escreve(posicao, (byte*)dados, strlen(dados) + 1) == 0) {
                    log_padronizado(COLOR_SUCCESS, "    • ", "[P%d] Escrita bem-sucedida", id);
                } else {
                    log_padronizado(COLOR_ERROR, "    • ", "[P%d] Falha na escrita", id);
                }
            } else {
                log_padronizado(COLOR_DEFAULT, "    • ", "[P%d] Uso: w <posicao> <dados>", id);
            }
        } else if (strcmp(comando, "s") == 0) {
            imprimir_estatisticas();
        } else if (strcmp(comando, "c") == 0) {
            imprimir_estado_cache();
        } else {
            log_padronizado(COLOR_ERROR, "    • ", "[P%d] Comando desconhecido: %s", id, comando);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        log_padronizado(COLOR_ERROR, "    • ", "[P?] Uso: %s <id_processo> [auto]", argv[0]);
        log_padronizado(COLOR_DEFAULT, "    • ", "[P?] Onde id_processo é um número de 0 a %d", N_NUM_PROCESSOS - 1);
        log_padronizado(COLOR_DEFAULT, "    • ", "[P?] Use 'auto' como segundo parâmetro para modo automático (sem interativo)");
        return 1;
    }
    
    int meu_id = atoi(argv[1]);
    if (meu_id < 0 || meu_id >= N_NUM_PROCESSOS) {
        log_padronizado(COLOR_ERROR, "    • ", "[P%d] ID de processo deve estar entre 0 e %d", meu_id, N_NUM_PROCESSOS - 1);
        return 1;
    }
    
    // Verificar modo automático
    if (argc == 3 && strcmp(argv[2], "auto") == 0) {
        modo_automatico = 1;
    }
    
    // Configurar handler para sinais
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Configurar informações dos processos
    // Para este teste, todos os processos rodam na mesma máquina
    InfoProcesso processos[N_NUM_PROCESSOS];
    for (int i = 0; i < N_NUM_PROCESSOS; i++) {
        processos[i].id = i;
        strcpy(processos[i].ip, "127.0.0.1");
        processos[i].porta = 8080 + i;
    }
    
    log_padronizado(COLOR_STEP, "\n█ ","[P%d] INICIALIZANDO PROCESSO DSM", meu_id);
    log_padronizado(COLOR_DEFAULT, "    • ","[P%d] Porta: %d", meu_id, processos[meu_id].porta);
    
    // Inicializar sistema DSM
    if (dsm_init(meu_id, processos, N_NUM_PROCESSOS) != 0) {
        log_padronizado(COLOR_DEFAULT, "    • ","[P%d] Falha ao inicializar sistema DSM", meu_id);
        return 1;
    }
    
    log_padronizado(COLOR_DEFAULT, "    • ","[P%d] Sistema DSM inicializado com sucesso!", meu_id);
    log_padronizado(COLOR_DEFAULT, "    • ", "[P%d] Este processo possui os blocos: %d %d %d %d %d %d %d %d %d %d ...", 
        meu_id, dsm_global->meus_blocos[0], dsm_global->meus_blocos[1], dsm_global->meus_blocos[2],
        dsm_global->meus_blocos[3], dsm_global->meus_blocos[4], dsm_global->meus_blocos[5],
        dsm_global->meus_blocos[6], dsm_global->meus_blocos[7], dsm_global->meus_blocos[8],
        dsm_global->meus_blocos[9]);
    
    // Aguardar um pouco para outros processos iniciarem
    log_padronizado(COLOR_DEFAULT, "    • ","[P%d] Aguardando outros processos (10 segundos)...", meu_id);
    sleep(10);
    
    // Executar testes
    teste_basico();
    
    // Modo interativo (apenas se não for automático)
    if (!modo_automatico) {
        log_padronizado(COLOR_DEFAULT, "    • ","[P%d] \nPressione Enter para entrar no modo interativo ou Ctrl+C para sair...", meu_id);
        getchar();
        
        if (continuar_executando) {
            teste_interativo();
        }
    } else {
        // Aguardar mais tempo no modo automático para outros processos completarem
        // suas operações de comunicação antes de terminar
        sleep(15);
    }
    
    // Mostrar estatísticas finais
    log_padronizado(COLOR_STEP, "\n█ ","[P%d] ESTATÍSTICAS FINAIS", meu_id);
    imprimir_estatisticas();
    
    // Limpar sistema
    dsm_cleanup();
    
    log_padronizado(COLOR_DEFAULT, "    • ","[P%d] Processo finalizado.", meu_id);
    return 0;
} 