
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h> 

uint8_t global_bhr = 0; // Global Branch History Register (BHR)
static uint8_t* shared_counters = NULL; // Dynamic array of 2-bit counters

static void initialize_predictor(int counter_size) {
    // Allocate and initialize the shared counters to 'weakly not taken' (01)
    shared_counters = (uint8_t*)malloc(counter_size * sizeof(uint8_t));
    if (!shared_counters) {
        perror("Failed to allocate memory for shared counters");
        exit(EXIT_FAILURE);
    }
    memset(shared_counters, 1, counter_size * sizeof(uint8_t));
}

static bool predict_branch() {
    uint8_t counter = shared_counters[global_bhr];
    return (counter >> 1) & 0x1; // MSB of the 2-bit counter
}

static void update_predictor(bool taken, int bhr_mask) {
    // Update the counter based on the actual branch outcome
    if (taken) {
        if (shared_counters[global_bhr] < 3) shared_counters[global_bhr]++;
    }
    else {
        if (shared_counters[global_bhr] > 0) shared_counters[global_bhr]--;
    }
    // Update the global BHR (shift left, add new outcome)
    global_bhr = ((global_bhr << 1) | (taken ? 1 : 0)) & bhr_mask; // Keep it ghr_bits size
}

static uint64_t parse_address(const char* line) {
    uint64_t address;
    sscanf(line, "Info 'riscvOVPsim/cpu', 0x%lx", &address);
    return address;
}

static bool determine_taken(uint64_t branch_address, uint64_t next_address) {
    // If the next address is the branch address + 4, branch is not taken
    return !(next_address == branch_address + 4);
}

int Global(const char* inputFile, int ghr_bits) {
    
    int counter_size = 1 << ghr_bits;
    int bhr_mask = (1 << ghr_bits) - 1;

    initialize_predictor(counter_size);

    FILE* file = fopen(inputFile, "r");
    if (!file) {
        perror("Failed to open file");
        free(shared_counters);
        return 1;
    }

    int total_branches = 0;
    int mispredictions = 0;

    char line[256];
    uint64_t branch_address, next_address;
    bool is_branch = true;

    while (fgets(line, sizeof(line), file)) {
        if (is_branch) {
            // This line is the branch instruction
            branch_address = parse_address(line);
        }
        else {
            // This line is the instruction right after the branch
            next_address = parse_address(line);

            bool taken = determine_taken(branch_address, next_address);
            bool prediction = predict_branch();

            if (prediction != taken) {
                mispredictions++; // Increment mispredictions if prediction was wrong
            }
            update_predictor(taken, bhr_mask);

            total_branches++; // Increment total branches
        }

        // Toggle between branch and the instruction after
        is_branch = !is_branch;
    }

    double misprediction_rate = (double)mispredictions / total_branches;

    printf("\n%s for %s:\n", __func__, inputFile);
    printf("Total Branches: %d\n", total_branches);
    printf("Mispredictions: %d\n", mispredictions);
    printf("Misprediction Rate: %.4f\n", misprediction_rate*100);

    fclose(file);
    free(shared_counters);
    return 0;
}

