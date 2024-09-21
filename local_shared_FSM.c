#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    uint64_t tag;           // Tag (assuming 64-bit address and variable index bits)
    uint8_t bhr;            // Branch History Register (BHR)
    bool valid;             // Valid bit
} BTBEntry;

typedef struct {
    BTBEntry entries[2];    // 2-way set associative (2 entries per set)
    bool lru_bit;           // LRU bit to track the least recently used entry
} BTBSet;

static uint8_t* shared_counters = NULL; // Dynamic array of 2-bit counters

static void initialize_btb(BTBSet btb[], int btb_sets, int counter_size) {
    for (int i = 0; i < btb_sets; i++) {
        btb[i].entries[0].valid = false;
        btb[i].entries[1].valid = false;
        btb[i].lru_bit = false; // Start with the first entry as LRU
    }

    // Allocate and initialize the shared counters to 'weakly not taken' (01)
    shared_counters = (uint8_t*)malloc(counter_size * sizeof(uint8_t));
    if (!shared_counters) {
        perror("Failed to allocate memory for shared counters");
        exit(EXIT_FAILURE);
    }
    memset(shared_counters, 1, counter_size * sizeof(uint8_t)); // Initialize counters
}

static uint16_t get_index(uint64_t address, int index_bits) {
    return (address & ((1ULL << index_bits) - 1));
}

static uint64_t get_tag(uint64_t address, int index_bits) {
    return (address >> index_bits);
}

static bool predict_branch(BTBEntry* entry) {
    uint8_t bhr_value = entry->bhr;
    uint8_t counter = shared_counters[bhr_value];
    return (counter >> 1) & 0x1; // MSB of the 2-bit counter
}

static void update_btb(BTBSet btb[], uint64_t address, bool taken, int index_bits, int btb_sets, int bhr_mask) {
    uint16_t index = get_index(address, index_bits);
    uint64_t tag = get_tag(address, index_bits);

    BTBSet* set = &btb[index % btb_sets];
    BTBEntry* entry = NULL;

    // Search for the entry by comparing tags of both entries in the set
    if (set->entries[0].valid && set->entries[0].tag == tag) {
        entry = &set->entries[0]; // Match found in way 0
    }
    else if (set->entries[1].valid && set->entries[1].tag == tag) {
        entry = &set->entries[1]; // Match found in way 1
    }

    if (entry) {
        // Update the counter based on the actual branch outcome
        uint8_t bhr_value = entry->bhr;
        if (taken) {
            if (shared_counters[bhr_value] < 3) shared_counters[bhr_value]++;
        }
        else {
            if (shared_counters[bhr_value] > 0) shared_counters[bhr_value]--;
        }
        // Update BHR (shift left, add new outcome)
        entry->bhr = ((entry->bhr << 1) | (taken ? 1 : 0)) & bhr_mask; // Keep it BHR_BITS size
    }
    else {
        // No matching entry found, use the LRU bit to determine which entry to replace
        int entry_index = set->lru_bit ? 1 : 0; // Select the LRU entry for replacement
        entry = &set->entries[entry_index];

        // Initialize the new entry with the branch data
        entry->tag = tag;
        entry->valid = true;
        entry->bhr = 0; // Start with no history
    }

    // Update the LRU bit to reflect the most recently used entry
    set->lru_bit = (entry == &set->entries[0]) ? 1 : 0;
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

int Local_shared_FSM(const char* inputFile) {
    
    int bhr_bits = 3;
    int btb_entries = 2048;

    int index_bits = (int)(log2(btb_entries / 2));
    int tag_bits = 64 - index_bits; // Assuming 64-bit addresses
    int btb_sets = btb_entries / 2;
    int counter_size = 1 << bhr_bits;
    int bhr_mask = (1 << bhr_bits) - 1;

    BTBSet* btb = (BTBSet*)malloc(btb_sets * sizeof(BTBSet));
    if (!btb) {
        perror("Failed to allocate memory for BTB sets");
        return 1;
    }
    initialize_btb(btb, btb_sets, counter_size);

    FILE* file = fopen(inputFile, "r");
    if (!file) {
        perror("Failed to open file");
        free(btb);
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

            uint16_t index = get_index(branch_address, index_bits);
            uint64_t tag = get_tag(branch_address, index_bits);

            BTBSet* set = &btb[index % btb_sets];
            BTBEntry* entry = NULL;

            // Check both entries in the set
            if (set->entries[0].valid && set->entries[0].tag == tag) {
                entry = &set->entries[0];
            }
            else if (set->entries[1].valid && set->entries[1].tag == tag) {
                entry = &set->entries[1];
            }

            // Predict and update BTB
            if (entry) {
                bool prediction = predict_branch(entry);

                if (prediction != taken) {
                    mispredictions++; // Increment mispredictions if prediction was wrong
                }
                update_btb(btb, branch_address, taken, index_bits, btb_sets, bhr_mask);
            }
            else {
                // If miss, update the BTB with this new branch
                update_btb(btb, branch_address, taken, index_bits, btb_sets, bhr_mask);
                // The first-time prediction will be based on initialized counter values

                if (taken) {
                    mispredictions++; // Increment mispredictions if the initial prediction was wrong
                }
            }

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
    free(btb);
    free(shared_counters);
    return 0;
}
