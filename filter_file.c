#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE_LENGTH 256

// Function to check if a line contains a branch command
int isBranchCommand(const char* line) {
    return (strstr(line, "beq") != NULL || strstr(line, "beqz") != NULL || strstr(line, "bne") != NULL 
            || strstr(line, "blt") != NULL || strstr(line, "bge") != NULL || strstr(line, "bgtz") != NULL 
            || strstr(line, "blez") != NULL || strstr(line, "bltz") != NULL || strstr(line, "bgez") != NULL 
            || strstr(line, "bltu") != NULL || strstr(line, "bgeu") != NULL);
}

void filterBranchCommands(const char* inputFileName, const char* outputFileName) {
    FILE* inputFile = fopen(inputFileName, "r");
    FILE* outputFile = fopen(outputFileName, "w");
    char line[MAX_LINE_LENGTH];
    char nextLine[MAX_LINE_LENGTH];
    int writeNextLine = 0;

    if (inputFile == NULL) {
        perror("Error opening input file");
        return;
    }

    if (outputFile == NULL) {
        perror("Error opening output file");
        fclose(inputFile);
        return;
    }

    while (fgets(line, sizeof(line), inputFile)) {
        if (writeNextLine) {
            fputs(line, outputFile);
            writeNextLine = 0;
        }
        if (isBranchCommand(line)) {
            fputs(line, outputFile);
            writeNextLine = 1;
        }
    }

    fclose(inputFile);
    fclose(outputFile);
}

int FilterFile(const char* inputFile, const char* outputFile) {
   
    filterBranchCommands(inputFile, outputFile);

    printf("Filtered branch commands have been written to %s\n", outputFile);

    return 0;
}

