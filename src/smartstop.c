#include "smartstop.h"
#include <stdio.h>
#include <stdlib.h>
#include "pico/time.h"

void smartstop_init(HallCall calls[], ElevatorState *e, Stats *s) {
    for (int i = 0; i < MAX_FLOORS; i++) {
        calls[i].active = false;
        calls[i].floor = i;
        calls[i].est_passengers = 0;
        calls[i].wait_time = 0;
    }

    e->current_floor = MAX_FLOORS - 1; // começa no último andar
    e->direction = -1;                 // descendo
    e->occupancy = 0;                  // vazio

    s->total_stops = 0;
    s->skipped_stops = 0;
    s->total_cycles = 0;
    s->total_boarded = 0;

    // Semente para números aleatórios
    uint64_t t = time_us_64();
    srand((unsigned int)(t ^ (t >> 32)));
}

int estimate_passengers(TrafficMode mode) {
    switch (mode) {
        case TRAFFIC_LOW:
            return rand() % 2;   // 0 a 1
        case TRAFFIC_MEDIUM:
            return rand() % 4;   // 0 a 3
        case TRAFFIC_HIGH:
            return rand() % 6;   // 0 a 5
        default:
            return rand() % 3;
    }
}

void generate_random_hall_calls(HallCall calls[],
                                ElevatorState *e,
                                TrafficMode mode) {
    // Probabilidade simples de surgir nova chamada por andar
    for (int i = 0; i < MAX_FLOORS; i++) {
        // Não gera chamada no andar atual (já está ali)
        if (i == e->current_floor) {
            continue;
        }

        if (!calls[i].active) {
            int r = rand() % 100;
            // 10% de chance de surgir nova chamada (ajuste se quiser)
            if (r < 10) {
                calls[i].active = true;
                calls[i].floor = i;
                calls[i].est_passengers = estimate_passengers(mode);
                calls[i].wait_time = 0;
            }
        } else {
            // aumenta tempo de espera simulado
            calls[i].wait_time++;
        }
    }
}

int smartstop_decide_next_floor(HallCall calls[],
                                ElevatorState *e,
                                Stats *s,
                                float efficiency_threshold) {
    s->total_cycles++;

    // Procura chamadas ativas na direção do movimento
    float best_efficiency = -1.0f;
    int best_floor = -1;

    for (int i = 0; i < MAX_FLOORS; i++) {
        if (!calls[i].active) continue;

        int delta = i - e->current_floor;

        // só considera andares "à frente" na direção atual
        if (e->direction == 1 && delta <= 0) continue;  // subindo
        if (e->direction == -1 && delta >= 0) continue; // descendo

        int est = calls[i].est_passengers;

        if (est <= 0) continue; // sem ganho não faz sentido

        // custo simples: diferença de andares + custo fixo de parada
        float cost = (float)(delta >= 0 ? delta : -delta) + 2.0f;
        float eff = (float)est / cost;

        // bonificação se a chamada está esperando há muito tempo
        if (calls[i].wait_time > 5) {
            eff *= 1.2f;
        }

        if (eff > best_efficiency) {
            best_efficiency = eff;
            best_floor = i;
        }
    }

    if (best_floor == -1) {
        // nenhuma chamada na direção atual
        return -1;
    }

    // Se eficiência for baixa, o algoritmo prefere "passar direto"
    if (best_efficiency < efficiency_threshold) {
        // Contabiliza que existe chamado, mas foi ignorado neste ciclo
        s->skipped_stops++;
        return -1;
    }

    return best_floor;
}

void smartstop_handle_stop(HallCall calls[],
                           ElevatorState *e,
                           Stats *s,
                           int floor) {
    if (!calls[floor].active) {
        // nada a fazer
        return;
    }

    int est = calls[floor].est_passengers;
    if (est < 0) est = 0;

    int available_capacity = ELEVATOR_CAP - e->occupancy;
    if (available_capacity < 0) available_capacity = 0;

    int boarded = est;
    if (boarded > available_capacity) {
        boarded = available_capacity;
    }

    e->occupancy += boarded;
    if (e->occupancy > ELEVATOR_CAP) {
        e->occupancy = ELEVATOR_CAP;
    }

    s->total_boarded += boarded;
    s->total_stops++;

    // Limpa chamada do andar
    calls[floor].active = false;
    calls[floor].est_passengers = 0;
    calls[floor].wait_time = 0;
}

void print_simulation_header(const ElevatorState *e) {
    printf("=== Simulacao SmartStop (BitDogLab) ===\n");
    printf("Andar atual: %d | Direcao: %s | Ocupacao: %d/%d\n",
           e->current_floor,
           (e->direction == 1 ? "Subindo" : "Descendo"),
           e->occupancy,
           ELEVATOR_CAP);
}

void print_calls_info(const HallCall calls[]) {
    printf("Chamadas externas ativas:\n");

    bool any = false;
    for (int i = 0; i < MAX_FLOORS; i++) {
        if (calls[i].active) {
            printf(" - Andar %2d | estimados: %d | espera: %d ciclos\n",
                   i,
                   calls[i].est_passengers,
                   calls[i].wait_time);
            any = true;
        }
    }

    if (!any) {
        printf(" (nenhuma chamada ativa)\n");
    }
}

void print_stats(const Stats *s) {
    printf("\n--- Estatisticas aproximadas ---\n");
    printf("Ciclos simulados:  %d\n", s->total_cycles);
    printf("Paradas realizadas:%d\n", s->total_stops);
    printf("Paradas ignoradas: %d\n", s->skipped_stops);
    printf("Passageiros embarcados (simulados): %d\n", s->total_boarded);

    if (s->total_stops + s->skipped_stops > 0) {
        float skip_rate = (float)s->skipped_stops /
                          (float)(s->total_stops + s->skipped_stops) * 100.0f;
        printf("Taxa de paradas evitadas: %.1f %%\n", skip_rate);
    }
    printf("--------------------------------\n\n");
}
