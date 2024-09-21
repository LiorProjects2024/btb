#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to trim whitespace from the beginning and end of a string
char* trim_whitespace(char* str) {
   
    while (isspace((unsigned char)*str)) str++;
    char* end = str + strlen(str) - 1;
    
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';

    return str;
}

// Function to read configuration from a file and set variables
void read_config(int* ghr_bits, int* bhr_bits, int* entries, int* which_predictor) {
    FILE* file = fopen("BTBConfiguration.txt", "r");
    if (!file) {
        perror("Failed to open configuration file");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || strlen(trim_whitespace(line)) == 0) {
            continue;
        }

        char* key = strtok(line, "=");
        char* value = strtok(NULL, "=");

        if (key && value) {
            key = trim_whitespace(key);
            value = trim_whitespace(value);

            // Assign the appropriate variable based on the key
            if (strcmp(key, "ghr_bits") == 0) {
                *ghr_bits = atoi(value);
            }
            else if (strcmp(key, "bhr_bits") == 0) {
                *bhr_bits = atoi(value);
            }
            else if (strcmp(key, "entries") == 0) {
                *entries = atoi(value);
            }
            else if (strcmp(key, "which_predictor") == 0) {
                *which_predictor = atoi(value);
            }
            else {
                printf("Unknown configuration key: %s\n", key);
            }
        }
    }

    fclose(file);
}

int main()
{
    const char* files[4] = { "coremark_val.trc","dhrystone_val.trc","fibonacci_val.trc","linpack_val.trc" };
    const char *filesFilterd[4] = { "coremark_val_filtered.trc","dhrystone_val_filtered.trc","fibonacci_val_filtered.trc","linpack_val_filtered.trc" };

    int ghr_bits = 0;
	int bhr_bits = 0;
	int entries = 0;
	int which_predictor = 0;
    read_config(&ghr_bits, &bhr_bits, &entries, &which_predictor);

    for (int index = 0; index < 4; index++)
    {
        FilterFile(files[index], filesFilterd[index]);
    }
    
    switch (which_predictor)
    {
        case 0: //LOCAL_PRIVATE_FSM
            for (int index = 0; index < 4; index++)
            {
                Local_private_FSM(filesFilterd[index], bhr_bits, entries);
            }
            break;
        case 1: //LOCAL_SHARES_FSM
            for (int index = 0; index < 4; index++)
            {
                Local_shared_FSM(filesFilterd[index]);
            }
            break;
        case 2: // GLOBAL
            for (int index = 0; index < 4; index++)
            {
                Global(filesFilterd[index], ghr_bits);
            }
            break;
        case 3: //TOURNAMENT
            for (int index = 0; index < 4; index++)
            {
                Tournament(filesFilterd[index]);
            }
            break;
        default:
            break;
    }
	return 0;
}