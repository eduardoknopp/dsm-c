#include "dsm.h"
#include <signal.h>

// Flag para controle de execução
static volatile int continuar_executando = 1;
static int modo_automatico = 0;

void signal_handler(int sig) {
    (void)sig; // Suprimir warning de parâmetro não utilizado
    continuar_executando = 0;
    log_padronizado(COLOR_DEFAULT, "\n    • ", "Recebido sinal de interrupção. Finalizando...");
}

void teste_basico() {
    log_padronizado(COLOR_STEP, "\n█ ","TESTE BÁSICO DO SISTEMA DSM");
    
    // Teste de escrita e leitura local
    byte dados_escrita[] = "Hello DSM World!";
    byte buffer_leitura[100];
    
    log_padronizado(COLOR_STEP, "  ▶ ","1. Testando escrita em bloco local");
    
    // Encontrar um bloco que pertence a este processo
    int bloco_teste = dsm_global->meu_id;  // Primeiro bloco que me pertence
    int posicao = bloco_teste * T_TAMANHO_BLOCO;
    
    if (escreve(posicao, dados_escrita, strlen((char*)dados_escrita) + 1) == 0) {
        log_padronizado(COLOR_SUCCESS, "","1.1 Escrita local bem-sucedida");
        
        // Testar leitura do mesmo bloco
        log_padronizado(COLOR_STEP, "  ▶ ","2. Testando leitura do bloco local");
        if (le(posicao, buffer_leitura, strlen((char*)dados_escrita) + 1) == 0) {
            log_padronizado(COLOR_SUCCESS, "","2.1 Leitura local bem-sucedida: '%s'", buffer_leitura);
        } else {
            log_padronizado(COLOR_ERROR, "","2.2 Falha na leitura local");
        }
    } else {
        log_padronizado(COLOR_ERROR, "","2.3 Falha na escrita local");
    }
    
    // Teste de acesso a bloco remoto
    log_padronizado(COLOR_STEP, "  ▶ ","3. Testando acesso a bloco remoto");
    int bloco_remoto = (dsm_global->meu_id + 1) % dsm_global->num_processos;
    int posicao_remota = bloco_remoto * T_TAMANHO_BLOCO;
    
    if (le(posicao_remota, buffer_leitura, 16) == 0) {
        log_padronizado(COLOR_SUCCESS, "","3.1 Leitura remota bem-sucedida (cache miss)");
        
        // Segunda leitura deve ser cache hit
        if (le(posicao_remota, buffer_leitura, 16) == 0) {
            log_padronizado(COLOR_SUCCESS, "","3.2 Segunda leitura remota bem-sucedida (cache hit)");
        }
    } else {
        log_padronizado(COLOR_ERROR, "","3.3 Falha na leitura remota");
    }
    
    // Teste de escrita em bloco remoto (deve falhar)
    log_padronizado(COLOR_STEP, "  ▶ ","4. Testando escrita em bloco remoto (deve falhar)");
    if (escreve(posicao_remota, dados_escrita, strlen((char*)dados_escrita) + 1) != 0) {
        log_padronizado(COLOR_SUCCESS, "","4.1 Escrita remota rejeitada corretamente");
    } else {
        log_padronizado(COLOR_ERROR, "","4.2 Escrita remota aceita incorretamente");
    }
}

void teste_interativo() {
    log_padronizado(COLOR_STEP, "\n█ ","MODO INTERATIVO");
    log_padronizado(COLOR_DEFAULT, "    • ","Comandos disponíveis:");
    log_padronizado(COLOR_DEFAULT, "    • ","r <posicao> <tamanho> - Ler dados");
    log_padronizado(COLOR_DEFAULT, "    • ","w <posicao> <dados>   - Escrever dados");
    log_padronizado(COLOR_DEFAULT, "    • ","s                     - Mostrar estatísticas");
    log_padronizado(COLOR_DEFAULT, "    • ","c                     - Mostrar estado do cache");
    log_padronizado(COLOR_DEFAULT, "    • ","q                     - Sair");
    log_padronizado(COLOR_DEFAULT, "    • ","Digite 'help' para ver os comandos novamente\n");
    
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
            log_padronizado(COLOR_DEFAULT, "    • ", "Comandos:");
            log_padronizado(COLOR_DEFAULT, "    • ", "  r <pos> <tam> - Ler");
            log_padronizado(COLOR_DEFAULT, "    • ", "  w <pos> <dados> - Escrever");
            log_padronizado(COLOR_DEFAULT, "    • ", "  s - Estatísticas");
            log_padronizado(COLOR_DEFAULT, "    • ", "  c - Cache");
            log_padronizado(COLOR_DEFAULT, "    • ", "  q - Sair");
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
                        log_padronizado(COLOR_ERROR, "    • ", "Falha na leitura");
                    }
                } else {
                    log_padronizado(COLOR_ERROR, "    • ", "Tamanho inválido");
                }
            } else {
                log_padronizado(COLOR_DEFAULT, "    • ", "Uso: r <posicao> <tamanho>");
            }
        } else if (strcmp(comando, "w") == 0) {
            if (sscanf(linha, "w %d %255s", &posicao, dados) == 2) {
                if (escreve(posicao, (byte*)dados, strlen(dados) + 1) == 0) {
                    log_padronizado(COLOR_SUCCESS, "    • ", "Escrita bem-sucedida");
                } else {
                    log_padronizado(COLOR_ERROR, "    • ", "Falha na escrita");
                }
            } else {
                log_padronizado(COLOR_DEFAULT, "    • ", "Uso: w <posicao> <dados>");
            }
        } else if (strcmp(comando, "s") == 0) {
            imprimir_estatisticas();
        } else if (strcmp(comando, "c") == 0) {
            imprimir_estado_cache();
        } else {
            log_padronizado(COLOR_ERROR, "    • ", "Comando desconhecido: %s", comando);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        log_padronizado(COLOR_ERROR, "    • ", "Uso: %s <id_processo> [auto]", argv[0]);
        log_padronizado(COLOR_DEFAULT, "    • ", "Onde id_processo é um número de 0 a %d", N_NUM_PROCESSOS - 1);
        log_padronizado(COLOR_DEFAULT, "    • ", "Use 'auto' como segundo parâmetro para modo automático (sem interativo)");
        return 1;
    }
    
    int meu_id = atoi(argv[1]);
    if (meu_id < 0 || meu_id >= N_NUM_PROCESSOS) {
        log_padronizado(COLOR_ERROR, "    • ", "ID de processo deve estar entre 0 e %d", N_NUM_PROCESSOS - 1);
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
    
    log_padronizado(COLOR_STEP, "\n█ ","INICIALIZANDO PROCESSO DSM %d", meu_id);
    log_padronizado(COLOR_DEFAULT, "    • ","Porta: %d", processos[meu_id].porta);
    
    // Inicializar sistema DSM
    if (dsm_init(meu_id, processos, N_NUM_PROCESSOS) != 0) {
        log_padronizado(COLOR_DEFAULT, "    • ","Falha ao inicializar sistema DSM");
        return 1;
    }
    
    log_padronizado(COLOR_DEFAULT, "    • ","Sistema DSM inicializado com sucesso!");
    log_padronizado(COLOR_DEFAULT, "    • ","%sEste processo possui os blocos: %s", COLOR_DEFAULT, COLOR_DEFAULT);
    for (int i = 0; i < dsm_global->num_blocos_locais; i++) {
        printf("%d ", dsm_global->meus_blocos[i]);
        if (i > 10) {
            printf("...");
            break;
        }
    }
    printf("%s\n", COLOR_RESET);
    
    // Aguardar um pouco para outros processos iniciarem
    log_padronizado(COLOR_DEFAULT, "    • ","Aguardando outros processos (10 segundos)...");
    sleep(10);
    
    // Executar testes
    teste_basico();
    
    // Modo interativo (apenas se não for automático)
    if (!modo_automatico) {
        log_padronizado(COLOR_DEFAULT, "    • ","\nPressione Enter para entrar no modo interativo ou Ctrl+C para sair...");
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
    log_padronizado(COLOR_STEP, "\n█ ","ESTATÍSTICAS FINAIS - PROCESSO %d", meu_id);
    imprimir_estatisticas();
    
    // Limpar sistema
    dsm_cleanup();
    
    log_padronizado(COLOR_DEFAULT, "    • ","Processo %d finalizado.", meu_id);
    return 0;
} 