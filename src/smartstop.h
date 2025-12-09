#ifndef SMARTSTOP_H
#define SMARTSTOP_H

#include <stdbool.h>

#define MAX_FLOORS   10
#define ELEVATOR_CAP 8

typedef struct {
    bool active;
    int floor;
    int est_passengers;   // passageiros estimados esperando
    int wait_time;        // "tempo de espera" simulado em ciclos
} HallCall;

typedef enum {
    TRAFFIC_LOW = 0,
    TRAFFIC_MEDIUM,
    TRAFFIC_HIGH
} TrafficMode;

typedef struct {
    int current_floor;
    int direction;      // +1 subindo, -1 descendo
    int occupancy;      // quantos passageiros dentro
} ElevatorState;

typedef struct {
    int total_stops;
    int skipped_stops;
    int total_cycles;
    int total_boarded;
} Stats;

// Inicialização
void smartstop_init(HallCall calls[], ElevatorState *e, Stats *s);

// Geração de tráfego (cria chamadas externas aleatórias)
void generate_random_hall_calls(HallCall calls[],
                                ElevatorState *e,
                                TrafficMode mode);

// Função que estima passageiros em cada chamada (0..N)
int estimate_passengers(TrafficMode mode);

// Decide a próxima parada / ou se segue sem parar
// Retorna -1 se não houver parada a fazer neste ciclo
int smartstop_decide_next_floor(HallCall calls[],
                                ElevatorState *e,
                                Stats *s,
                                float efficiency_threshold);

// Atualiza ocupação e limpa chamada do andar atendido
void smartstop_handle_stop(HallCall calls[],
                           ElevatorState *e,
                           Stats *s,
                           int floor);

// Funções de log para o Monitor Serial
void print_simulation_header(const ElevatorState *e);
void print_calls_info(const HallCall calls[]);
void print_stats(const Stats *s);

#endif

