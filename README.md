This project implements and evaluates four branch prediction strategies used in modern CPU designs: Local Private FSM (Finite State Machine), Local Shared FSM, Global Predictor, and Tournament Predictor. The primary goal of branch prediction is to anticipate the outcome of conditional branches in a program, helping to increase the efficiency of instruction execution in pipelined processors by speculatively executing future instructions. This project simulates these branch prediction techniques and compares their accuracy by analyzing the branch misprediction rates across several benchmark assembly programs, such as coremark, dhrystone, fibonacci, and linpack.

Project Overview:
The simulation works by first filtering out branch instructions and the next instruction from assembly trace files, which are then passed to the chosen predictor for analysis. 
Each of the four predictors operates differently:
Local Private FSM: Assigns each branch its own local history and a set of    2-bit prediction counters. This method relies entirely on localized information to predict whether the branch will be taken.
Local Shared FSM: Shares a common set of 2-bit counters among branches while still maintaining a separate local history for each branch, thereby reducing memory usage while maintaining some branch-specific tracking.

Global Predictor: Uses a single global history register (GHR) that stores the outcomes of recent branches. This global history is used to index into a shared table of 2-bit counters, making predictions based on the overall behavior of all branches.
Tournament Predictor: Combines both local and global prediction techniques, using a selector mechanism to choose the best predictor for each branch. A 2-bit chooser counter tracks which of the two predictors (local or global) has been more accurate for a given branch, adjusting dynamically based on performance.

How the Project Works:
Branch Filtering: The first step in the simulation is filtering the assembly trace files to extract only branch instructions. These instructions (such as beq, bne, and blt) are critical for predicting the control flow of the program. A filtering utility is implemented to scan the trace files and create filtered versions that contain only the relevant branch information.
Configuration: The behavior of the simulation is controlled by a configuration file, BTBConfiguration.txt. In this file, various parameters for the Branch Target Buffer (BTB) and predictors are defined, including:
ghr_bits: The number of bits used for the Global History Register in the Global and Tournament Predictors.
bhr_bits: The number of bits used for the Branch History Register in the Local Private and Local Shared FSM predictors.
entries: The number of entries in the BTB, which determines how many branches can be tracked by the predictor.
which_predictor: A setting to specify which branch predictor will be used during the simulation. Options include 0 (Local Private FSM), 1 (Local Shared FSM), 2 (Global Predictor), and 3 (Tournament Predictor).
Prediction Mechanism: Once the branch instructions are filtered, the selected predictor is applied to the trace data. Each predictor operates by first attempting to predict the outcome of each branch (whether it will be taken or not) based on historical data. After making the prediction, the actual outcome of the branch is revealed, and the predictor updates its internal data structures (counters and history registers) to improve the accuracy of future predictions.

How to Use:
To use the project, start by compiling the codebase. The simulation is structured around several C files, each corresponding to a different predictor (e.g., global.c for the Global Predictor, local_private_FSM.c for the Local Private FSM, etc.), along with utility files for filtering branch instructions (filter_file.c) and managing the configuration (main.c).
After compilation, the user can modify the BTBConfiguration.txt file to set the desired branch predictor and BTB settings, such as the number of entries and history register sizes. The simulation is then run on a set of trace files that contain branch instructions from different assembly programs.
Example Configuration:
A typical BTBConfiguration.txt might include the following settings:
makefile
Copy code
ghr_bits = 6
bhr_bits = 3
entries = 2048
which_predictor = 3   \\Tournament Predictor
This configuration specifies that the Tournament Predictor should be used, with a 6-bit Global History Register, a 3-bit Branch History Register, and 2048 entries in the Branch Target Buffer. This setup would test the hybrid approach, combining local and global prediction strategies.

Expected Output:
At the end of the simulation, the project reports the total number of branches processed, the number of mispredictions, and the misprediction rate for each predictor. These results offer valuable insights into the efficiency and accuracy of each branch prediction method, allowing users to compare the performance of different strategies in various programs.
