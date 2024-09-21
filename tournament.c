#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Local Predictor Structures
typedef struct {

    uint64_t tag;
    uint8_t bhr;
    uint8_t* counters; // Dynamic 2-bit counters for local predictor
    bool valid;
} BTBEntry;

typedef struct {
    BTBEntry entries[2];
    bool lru_bit; // LRU bit to track the least recently used entry
} BTBSet;

uint8_t global_ghr = 0; // Global GHR shared among all branches
static uint8_t* shared_counters = NULL; // Dynamic array of 2-bit counters for global predictor
uint8_t chooser[1024]; // Array of 2-bit saturating counters

static void initialize_predictors(BTBSet btb[], int btb_sets, int global_counter_size, int chooser_size, int local_bhr_size) {
    for (int i = 0; i < btb_sets; i++) {
        btb[i].entries[0].valid = false;
        btb[i].entries[1].valid = false;
        btb[i].lru_bit = false; // Start with the first entry as LRU

        // Allocate counters dynamically for each entry
        btb[i].entries[0].counters = (uint8_t*)malloc(local_bhr_size * sizeof(uint8_t));
        btb[i].entries[1].counters = (uint8_t*)malloc(local_bhr_size * sizeof(uint8_t));
        if (!btb[i].entries[0].counters || !btb[i].entries[1].counters) {
            perror("Failed to allocate memory for local predictor counters");
            exit(EXIT_FAILURE);
        }

        // Initialize counters to 'weakly not taken' (01)
        memset(btb[i].entries[0].counters, 1, local_bhr_size * sizeof(uint8_t));
        memset(btb[i].entries[1].counters, 1, local_bhr_size * sizeof(uint8_t));
    }

    // Allocate and initialize the global counters to 'weakly not taken' (01)
    shared_counters = (uint8_t*)malloc(global_counter_size * sizeof(uint8_t));
    if (!shared_counters) {
        perror("Failed to allocate memory for global counters");
        exit(EXIT_FAILURE);
    }
    memset(shared_counters, 1, global_counter_size * sizeof(uint8_t));

    // Initialize the chooser array to 'weakly favor global' (01)
    for (int i = 0; i < chooser_size; i++) {
        chooser[i] = 1; 
    }
}

static uint16_t get_index(uint64_t address, int index_bits) {
    return (address & ((1ULL << index_bits) - 1));
}

static uint64_t get_tag(uint64_t address, int index_bits) {
    return (address >> index_bits);
}

static bool predict_local(BTBSet btb[], uint64_t address, int index_bits, int btb_sets) {
    uint16_t index = get_index(address, index_bits);
    uint64_t tag = get_tag(address, index_bits);

    BTBSet* set = &btb[index % btb_sets];
    BTBEntry* entry = NULL;

    // Search for the entry by comparing tags of both entries in the set
    if (set->entries[0].valid && set->entries[0].tag == tag) {
        entry = &set->entries[0];
    }
    else if (set->entries[1].valid && set->entries[1].tag == tag) {
        entry = &set->entries[1];
    }

    if (entry) {
        uint8_t bhr_value = entry->bhr;
        uint8_t counter = entry->counters[bhr_value];
        return (counter >> 1) & 0x1; // MSB of the 2-bit counter
    }

    return true; // Default prediction if not found
}

static bool predict_global() {
    uint8_t counter = shared_counters[global_ghr];
    return (counter >> 1) & 0x1; // MSB of the 2-bit counter
}

static void update_local(BTBSet btb[], uint64_t address, bool taken, int index_bits, int btb_sets, int local_bhr_mask) {
    uint16_t index = get_index(address, index_bits);
    uint64_t tag = get_tag(address, index_bits);

    BTBSet* set = &btb[index % btb_sets];
    BTBEntry* entry = NULL;

    // Search for the entry by comparing tags of both entries in the set
    if (set->entries[0].valid && set->entries[0].tag == tag) {
        entry = &set->entries[0];
    }
    else if (set->entries[1].valid && set->entries[1].tag == tag) {
        entry = &set->entries[1];
    }

    if (entry) {
        uint8_t bhr_value = entry->bhr;
        if (taken) {
            if (entry->counters[bhr_value] < 3) entry->counters[bhr_value]++;
        }
        else {
            if (entry->counters[bhr_value] > 0) entry->counters[bhr_value]--;
        }
        entry->bhr = ((entry->bhr << 1) | (taken ? 1 : 0)) & local_bhr_mask;
    }
    else {
        int entry_index = set->lru_bit ? 1 : 0; // Select the LRU entry for replacement
        entry = &set->entries[entry_index];

        entry->tag = tag;
        entry->valid = true;
        entry->bhr = 0; // Start with no history
        memset(entry->counters, 1, 8 * sizeof(uint8_t)); // Initialize counters to 'weakly not taken' (01)
    }

    set->lru_bit = (entry == &set->entries[0]) ? 1 : 0;
}

static void update_global(bool taken, int global_ghr_mask) {
    if (taken) {
        if (shared_counters[global_ghr] < 3) shared_counters[global_ghr]++;
    }
    else {
        if (shared_counters[global_ghr] > 0) shared_counters[global_ghr]--;
    }
    global_ghr = ((global_ghr << 1) | (taken ? 1 : 0)) & global_ghr_mask;
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

int Tournament(const char* inputFile) {
  
    int local_bhr_bits = 3;
    int global_ghr_bits = 6;
    int instruction_length = 64;
    int btb_entries = 2048;
    int chooser_size = 1024;

    int index_bits = (int)(log2(btb_entries / 2));
    int tag_bits = instruction_length - index_bits;
    int btb_sets = btb_entries / 2;
    int local_bhr_mask = (1 << local_bhr_bits) - 1;
    int local_bhr_size = (1 << local_bhr_bits); // 2^3 = 8 possible histories
    int global_counter_size = (1 << global_ghr_bits); // 2^6 = 64 possible histories
    int global_ghr_mask = (1 << global_ghr_bits) - 1;

    BTBSet* btb = (BTBSet*)malloc(btb_sets * sizeof(BTBSet));
    if (!btb) {
        perror("Failed to allocate memory for BTB sets");
        return 1;
    }
    initialize_predictors(btb, btb_sets, global_counter_size, chooser_size, local_bhr_size);

    FILE* file = fopen(inputFile, "r");
    if (!file) {
        perror("Failed to open file");
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
            uint16_t chooser_index = get_index(branch_address, index_bits) % chooser_size; // Map branch to chooser index

            bool local_prediction = predict_local(btb, branch_address, index_bits, btb_sets);
            bool global_prediction = predict_global();

            // Determine which predictor to use based on the chooser's MSB
            bool use_local = (chooser[chooser_index] >> 1) & 0x1; // MSB of chooser counter

            bool prediction = use_local ? local_prediction : global_prediction;

            // Update misprediction count
            if (prediction != taken) {
                mispredictions++;
            }

            update_local(btb, branch_address, taken, index_bits, btb_sets, local_bhr_mask);
            update_global(taken, global_ghr_mask);

            // Update chooser based on which predictor was correct
            if (local_prediction == taken && global_prediction != taken) {
                if (chooser[chooser_index] < 3) chooser[chooser_index]++;  // Move towards favoring local
            }
            else if (local_prediction != taken && global_prediction == taken) {
                if (chooser[chooser_index] > 0) chooser[chooser_index]--;  // Move towards favoring global
            }

            total_branches++;
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
    free(btb);
    free(shared_counters);
    return 0;
}
