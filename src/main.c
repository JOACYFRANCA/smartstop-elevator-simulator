/*
 * SmartStop Elevator Simulator
 * 
 * - Placa: Raspberry Pi Pico W (BitDogLab)
 * - Objetivo: simular lógica de despacho de elevador com prioridades realistas:
 *   - Emergência por tempo de espera
 *   - Chamadas internas (passageiros a bordo)
 *   - Chamadas manuais (botões físicos A/B)
 *   - Proximidade na direção atual
 *   - Estratégia SmartStop (paradas eficientes)
 *   - Fallback por baixa ocupação (<= 2 passageiros)
 *
 * Saída: logs no terminal (USB serial) mostrando decisões a cada ciclo.
 */

#include <stdio.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "smartstop.h"
#include <stdlib.h>

// LEDs RGB da BitDogLab
#define LED_R 13
#define LED_G 11
#define LED_B 12

// Botões físicos
#define BUTTON_A 5   // Botão A: chamada interna
#define BUTTON_B 6   // Botão B: chamada externa

// Constantes realistas
#define MAX_WAIT_TIME 25           // Tempo máximo de espera aceitável (ciclos)
#define EMERGENCY_WAIT_TIME 15     // Tempo para prioridade emergencial
#define CYCLES_FULL_MAX 8          // Ciclos máximos lotado sem desembarcar
#define MIN_DISEMBARK_PASSENGERS 1 // Mínimo que desembarca por parada
#define MAX_DISEMBARK_PASSENGERS 4 // Máximo que desembarca por parada
#define TRAVEL_TIME_MS 400         // Tempo realista entre andares (ms)
#define DOOR_TIME_MS 800           // Tempo de abertura/fechamento de portas (ms)

// Vetor de chamadas internas (destinos dos passageiros)
static bool internal_calls[MAX_FLOORS];

// flags para saber quais chamadas vieram dos botões
static bool internal_from_button[MAX_FLOORS];  // Andares solicitados pelo botão A
static bool external_from_button[MAX_FLOORS];  // Andares solicitados pelo botão B

static int cycles_at_full_capacity = 0;
static int total_cycles = 0;

static void leds_init(void) {
    gpio_init(LED_R);
    gpio_init(LED_G);
    gpio_init(LED_B);
    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_set_dir(LED_B, GPIO_OUT);
}

static void set_rgb(bool r, bool g, bool b) {
    gpio_put(LED_R, r ? 1 : 0);
    gpio_put(LED_G, g ? 1 : 0);
    gpio_put(LED_B, b ? 1 : 0);
}

static void set_yellow(void) {
    set_rgb(true, true, false);
}

static void set_cyan(void) {
    set_rgb(false, true, true);
}

static void buttons_init(void) {
    gpio_init(BUTTON_A);
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_pull_up(BUTTON_B);
}

static void internal_calls_init(void) {
    for (int i = 0; i < MAX_FLOORS; i++) {
        internal_calls[i] = false;
        internal_from_button[i] = false;  
        external_from_button[i] = false;  
    }
}

static bool any_internal_call(void) {
    for (int i = 0; i < MAX_FLOORS; i++) {
        if (internal_calls[i]) return true;
    }
    return false;
}

// Simula desembarque realista de passageiros
static void simulate_disembark(ElevatorState *elevator, int floor, bool has_call) {
    if (elevator->occupancy <= 2) return;
    
    // Chance de alguém descer neste andar
    bool someone_exits = false;
    
    // Térreo e último andar têm maior probabilidade de desembarque
    int exit_probability = 35; // 35% base
    if (floor == 0 || floor == (MAX_FLOORS - 1)) {
        exit_probability = 70; // 70% nos extremos
    }
    
    // Se há chamada externa no andar, aumenta probabilidade (pessoas chegando = pessoas saindo)
    if (has_call) {
        exit_probability += 25; // Aumenta 25% se há chamada no andar
    }
    
    // Se há chamada interna para este andar, garantido desembarque
    if (internal_calls[floor]) {
        someone_exits = true;
        internal_calls[floor] = false;
    } else {
        someone_exits = (rand() % 100) < exit_probability;
    }
    
    if (someone_exits) {
        int disembark_count = MIN_DISEMBARK_PASSENGERS + 
                             (rand() % (MAX_DISEMBARK_PASSENGERS - MIN_DISEMBARK_PASSENGERS + 1));
        
        // Não pode desembarcar mais que a ocupação atual
        if (disembark_count > elevator->occupancy) {
            disembark_count = elevator->occupancy;
        }
        
        elevator->occupancy -= disembark_count;
        
        printf("  >> DESEMBARQUE: %d passageiro(s) saiu/saíram no andar %d\n", 
               disembark_count, floor);
        
        // LED ciano para desembarque
        set_cyan();
        sleep_ms(300);
        set_rgb(false, false, false);
    }
}

// Encontra chamadas em emergência (esperando muito tempo)
// IGNORA chamadas sem passageiros (est_passengers == 0)
static int find_emergency_call(HallCall calls[]) {
    int worst_floor = -1;
    int worst_wait = 0;
    
    for (int i = 0; i < MAX_FLOORS; i++) {
        if (calls[i].active && calls[i].est_passengers > 0 && calls[i].wait_time > worst_wait) {
            worst_wait = calls[i].wait_time;
            worst_floor = i;
        }
    }
    
    if (worst_wait >= EMERGENCY_WAIT_TIME) {
        return worst_floor;
    }
    
    return -1;
}


// Escolhe o próximo andar com base em prioridades realistas
// Escolhe o próximo andar com base em prioridades realistas
static int choose_next_floor_realistic(HallCall calls[], 
                                       ElevatorState *elevator,
                                       Stats *stats) {

    // PRIORIDADE 0: Chamadas em emergência (esperando muito tempo)
    int emergency_floor = find_emergency_call(calls);
    if (emergency_floor != -1) {
        // Conta quantas chamadas ativas existem entre aqui e lá
        int calls_in_path = 0;
        int direction = (emergency_floor > elevator->current_floor) ? 1 : -1;
        
        for (int f = elevator->current_floor; f != emergency_floor; f += direction) {
            if (calls[f].active && calls[f].est_passengers > 0) {
                calls_in_path++;
            }
        }
        
        // Se tiver poucas chamadas no caminho OU o elevador estiver bem vazio,
        // vai direto para a emergência
        if (calls_in_path < 2 || elevator->occupancy < 2) {
            printf("  [EMERGÊNCIA] Andar %d esperando %d ciclos - atendimento prioritário!\n",
                   emergency_floor, calls[emergency_floor].wait_time);
            return emergency_floor;
        } else {
            printf("  [EMERGÊNCIA DETECTADA] Mas há %d chamadas no caminho - atendendo caminho primeiro\n",
                   calls_in_path);
            // não retorna aqui: deixa seguir para outras prioridades (botão, internas, etc.)
        }
    }

    // PRIORIDADE 1: Chamadas disparadas manualmente pelos botões A e B
    int best_btn_floor = -1;
    int best_btn_dist = 999;
    for (int i = 0; i < MAX_FLOORS; i++) {
        if (!(internal_from_button[i] || external_from_button[i])) continue;

        int delta = i - elevator->current_floor;
        int dist = (delta >= 0) ? delta : -delta;
        if (dist < best_btn_dist) {
            best_btn_dist = dist;
            best_btn_floor = i;
        }
    }
    if (best_btn_floor != -1) {
        printf("  [PRIORIDADE BOTÃO] Atendendo chamada manual no andar %d\n",
               best_btn_floor);
        return best_btn_floor;
    }
    
    // PRIORIDADE 2: Chamadas internas (passageiros já dentro)
    if (any_internal_call()) {
        int best_floor = -1;
        int best_dist = 999;
        
        for (int i = 0; i < MAX_FLOORS; i++) {
            if (!internal_calls[i]) continue;
            
            int delta = i - elevator->current_floor;
            
            // Prioriza mesma direção
            if ((elevator->direction == 1 && delta < 0) || 
                (elevator->direction == -1 && delta > 0)) {
                continue;
            }
            
            int dist = (delta >= 0) ? delta : -delta;
            if (dist < best_dist) {
                best_dist = dist;
                best_floor = i;
            }
        }
        
        // Se não achou na direção atual, pega o mais próximo
        if (best_floor == -1) {
            for (int i = 0; i < MAX_FLOORS; i++) {
                if (!internal_calls[i]) continue;
                int delta = i - elevator->current_floor;
                int dist = (delta >= 0) ? delta : -delta;
                if (dist < best_dist) {
                    best_dist = dist;
                    best_floor = i;
                }
            }
        }
        
        if (best_floor != -1) {
            printf("  [PRIORIDADE INTERNA] Atendendo destino interno: andar %d\n", 
                   best_floor);
            return best_floor;
        }
    }
    
    // PRIORIDADE 3: Se lotado há muito tempo, FORÇAR desembarque
    if (elevator->occupancy >= ELEVATOR_CAP && 
        cycles_at_full_capacity >= CYCLES_FULL_MAX) {
        
        int next = elevator->current_floor + elevator->direction;
        if (next >= 0 && next < MAX_FLOORS) {
            printf("  [DESEMBARQUE FORÇADO] Elevador lotado há %d ciclos - parando no andar %d\n",
                   cycles_at_full_capacity, next);
            cycles_at_full_capacity = 0;
            return next;
        }
    }
    
    // PRIORIDADE 4: Primeiro atende chamadas próximas na direção atual
    for (int offset = 0; offset <= 2; offset++) {  // até 2 andares de distância
        int check_floor = elevator->current_floor + (offset * elevator->direction);
        
        if (check_floor >= 0 && check_floor < MAX_FLOORS) {
            if (calls[check_floor].active && calls[check_floor].est_passengers > 0) {
                printf("  [PROXIMIDADE] Chamada próxima detectada no andar %d\n", check_floor);
                return check_floor;
            }
        }
    }
    
    // PRIORIDADE 5: Se não está muito lotado, usar SmartStop
    if (elevator->occupancy < ELEVATOR_CAP - 2) {
        const float efficiency_threshold = 0.65f;
        int smartstop_floor = smartstop_decide_next_floor(calls, elevator, stats, 
                                                          efficiency_threshold);
        if (smartstop_floor != -1) {
            printf("  [SmartStop] Parada eficiente calculada: andar %d\n", smartstop_floor);
            return smartstop_floor;
        }
    }
    
    // PRIORIDADE 6: Se lotado mas não emergencial, buscar chamadas na direção
    // só considera chamadas COM passageiros
    if (elevator->occupancy >= ELEVATOR_CAP - 1) {
        for (int offset = 1; offset < MAX_FLOORS; offset++) {
            int floor = elevator->current_floor + (offset * elevator->direction);
            if (floor < 0 || floor >= MAX_FLOORS) break;

            if (calls[floor].active && calls[floor].est_passengers > 0) {
                printf("  [LOTADO] Buscando desembarque - andar %d na direção\n", floor);
                return floor;
            }
        }
    }

    // — FALLBACK REALISTA
    // Se elevador estiver vazio e existir chamada externa,
    // vá atender a chamada mais próxima.
    if (elevator->occupancy == 0) {
        int best_floor = -1;
        int best_dist = 999;

        for (int i = 0; i < MAX_FLOORS; i++) {
            if (calls[i].active && calls[i].est_passengers > 0) {
                int delta = i - elevator->current_floor;
                int dist = (delta >= 0) ? delta : -delta;
                if (dist < best_dist) {
                    best_dist = dist;
                    best_floor = i;
                }
            }
        }

        if (best_floor != -1) {
            printf("  [FALLBACK VAZIO] Elevador sem passageiros - indo atender andar %d\n",
                   best_floor);
            return best_floor;
        }
    }

    return -1;

}

// Remove chamadas "vazias": ativas mas com 0 passageiros
static void cleanup_empty_calls(HallCall calls[]) {
    for (int i = 0; i < MAX_FLOORS; i++) {
        if (calls[i].active && calls[i].est_passengers <= 0) {
            calls[i].active = false;
            calls[i].est_passengers = 0;
        }
    }
}


int main() {
    stdio_init_all();
    leds_init();
    buttons_init();
    internal_calls_init();

    HallCall calls[MAX_FLOORS];
    ElevatorState elevator;
    Stats stats;

    smartstop_init(calls, &elevator, &stats);

    const TrafficMode mode = TRAFFIC_MEDIUM;

    sleep_ms(2000);
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  Sistema SmartStop Realista - Simulador de Elevador      ║\n");
    printf("║  Botão A: Chamada Interna | Botão B: Chamada Externa     ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    bool last_a = true;
    bool last_b = true;

    while (true) {
        total_cycles++;
        set_rgb(false, false, false);

        // Leitura dos botões
        bool now_a = gpio_get(BUTTON_A);
        bool now_b = gpio_get(BUTTON_B);

        // BOTÃO A: Chamada interna (destino aleatório diferente do andar atual)
        if (!now_a && last_a) {
            int dest = rand() % MAX_FLOORS;
            if (dest == elevator.current_floor) {
                dest = (dest + 1) % MAX_FLOORS;
            }
            internal_calls[dest] = true;
            internal_from_button[dest] = true;   // 🔴 marca como chamada vinda do botão A
            printf("\n🔵 [BOTÃO A] Passageiro solicitou andar %d (chamada interna)\n", dest);
        }

        // BOTÃO B: Chamada externa (hall call)
        if (!now_b && last_b) {
            int floor = rand() % MAX_FLOORS;
            if (!calls[floor].active) {
                calls[floor].active = true;
                calls[floor].floor = floor;
                calls[floor].est_passengers = estimate_passengers(mode);
                calls[floor].wait_time = 0;
                external_from_button[floor] = true;  // 🔴 marca como chamada vinda do botão B
                printf("\n🟢 [BOTÃO B] Chamada HALL no andar %d (%d pessoa(s) esperando)\n",
                       floor, calls[floor].est_passengers);
            }
        }

        last_a = now_a;
        last_b = now_b;

        // Gera tráfego aleatório
        generate_random_hall_calls(calls, &elevator, mode);
        // Limpa chamadas vazias antes de decidir o próximo andar
cleanup_empty_calls(calls);


        // Interface de status
        printf("\n┌─────────────────────────────────────────────────────────┐\n");
        printf("│ Ciclo: %3d | Andar: %2d | Dir: %-7s | Ocupação: %d/%d %s│\n",
               total_cycles,
               elevator.current_floor,
               elevator.direction == 1 ? "Subindo" : "Descendo",
               elevator.occupancy,
               ELEVATOR_CAP,
               elevator.occupancy >= ELEVATOR_CAP ? "🔴" : "  ");
        printf("└─────────────────────────────────────────────────────────┘\n");

        // Lista chamadas ativas
        bool has_calls = false;
        for (int i = 0; i < MAX_FLOORS; i++) {
            if (calls[i].active && calls[i].est_passengers > 0) {
                if (!has_calls) {
                    printf("Chamadas ativas:\n");
                    has_calls = true;
                }
                printf("  • Andar %2d: %d pessoa(s) | Espera: %2d ciclos %s\n",
                       i, calls[i].est_passengers, calls[i].wait_time,
                       calls[i].wait_time >= EMERGENCY_WAIT_TIME ? "⚠️" : "");
            }
        }
        
        if (!has_calls) {
            printf("(Nenhuma chamada externa ativa)\n");
        }

        if (elevator.occupancy >= ELEVATOR_CAP) {
            cycles_at_full_capacity++;
        } else {
            cycles_at_full_capacity = 0;
        }

        // Decide próxima parada
        int target_floor = choose_next_floor_realistic(calls, &elevator, &stats);

        if (target_floor == -1) {
            printf("\n→ Movimento contínuo (sem paradas eficientes detectadas)\n");
            
            // Simula desembarque probabilístico durante movimento
            if (elevator.occupancy > 0 && (rand() % 100) < 15) {
                simulate_disembark(&elevator, elevator.current_floor, false);
            }
            
            elevator.current_floor += elevator.direction;

            if (elevator.current_floor <= 0) {
                elevator.current_floor = 0;
                elevator.direction = 1;
                printf("  ↻ Invertendo direção no térreo\n");
            } else if (elevator.current_floor >= (MAX_FLOORS - 1)) {
                elevator.current_floor = MAX_FLOORS - 1;
                elevator.direction = -1;
                printf("  ↻ Invertendo direção no último andar\n");
            }

            set_yellow();
            sleep_ms(150);
            set_rgb(false, false, false);
            
        } else {
           printf("\n🎯 DECISÃO: Parar no andar %d\n", target_floor);

// Ajuste inteligente de direção baseado no destino
if (target_floor > elevator.current_floor) {
    elevator.direction = 1;   // Sobe diretamente ao destino
} 
else if (target_floor < elevator.current_floor) {
    elevator.direction = -1;  // Desce diretamente ao destino
}

// Movimento andar por andar
while (elevator.current_floor != target_floor) {
    int prev_floor = elevator.current_floor;
    elevator.current_floor += elevator.direction;

    printf("  ├─ Deslocando: andar %d → %d", prev_floor, elevator.current_floor);
                
    // Verifica passagem por chamadas ativas
    bool skipped = false;
    for (int f = 0; f < MAX_FLOORS; f++) {
        if (calls[f].active && calls[f].est_passengers > 0 && 
            f == elevator.current_floor && f != target_floor) {
            printf(" [ignorando chamada do andar %d]", f);
            stats.skipped_stops++;
            skipped = true;
        }
    }
    printf("\n");

    if (skipped) {
        set_yellow();
        sleep_ms(100);
        set_rgb(false, false, false);
    }

    // Inverte nos extremos
    if (elevator.current_floor <= 0) {
        elevator.current_floor = 0;
        elevator.direction = 1;
    } else if (elevator.current_floor >= (MAX_FLOORS - 1)) {
        elevator.current_floor = (MAX_FLOORS - 1);
        elevator.direction = -1;
    }

    sleep_ms(TRAVEL_TIME_MS);
}

            // CHEGOU NO ANDAR
            printf("  └─ 🚪 PARADA no andar %d\n", target_floor);
            
            // LED verde
            set_rgb(false, true, false);
            sleep_ms(DOOR_TIME_MS / 2);

            // 1º: SEMPRE tenta desembarcar (prioridade máxima!)
            if (elevator.occupancy > 0) {
                simulate_disembark(&elevator, target_floor, calls[target_floor].active);
            }

            // 2º: EMBARQUE (só se houver chamada externa e espaço)
            if (calls[target_floor].active && elevator.occupancy < ELEVATOR_CAP) {
                // Só embarca se realmente tem pessoas esperando
                if (calls[target_floor].est_passengers > 0) {
                    smartstop_handle_stop(calls, &elevator, &stats, target_floor);
                    printf("  >> EMBARQUE: Passageiros entraram no elevador\n");
                } else {
                    // Chamada vazia, apenas remove
                    calls[target_floor].active = false;
                    printf("  >> Chamada vazia removida (sem passageiros)\n");
                }
            } else if (calls[target_floor].active && elevator.occupancy >= ELEVATOR_CAP) {
                printf("  ⚠️  Elevador LOTADO - passageiros aguardam próximo elevador\n");
                // Chamada permanece ativa
            } else if (calls[target_floor].active && calls[target_floor].est_passengers == 0) {
                // Remove chamadas vazias mesmo sem embarque
                calls[target_floor].active = false;
                printf("  >> Chamada vazia removida (sem passageiros)\n");
            }

            // 🔴 LIMPA flags de botão para esse andar, pois já foi atendido
            internal_from_button[target_floor] = false;
            external_from_button[target_floor] = false;

            printf("  📊 Ocupação atual: %d/%d\n", elevator.occupancy, ELEVATOR_CAP);

            if (elevator.occupancy >= ELEVATOR_CAP) {
                set_rgb(true, false, false);
                sleep_ms(300);
            }

            sleep_ms(DOOR_TIME_MS / 2);
            set_rgb(false, false, false);
        }

        print_stats(&stats);
        printf("\n════════════════════════════════════════════════════════════\n\n");

        sleep_ms(800);
    }

    return 0;
}
