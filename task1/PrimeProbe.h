#include <stdint.h>

#define NUM_OF_SETS 64
#define VICTIM_LINES 12

struct Memory {
    void* evsets1[NUM_OF_SETS];
    void* evsets2[NUM_OF_SETS];
};

// Function to plot the heatmap using gnuplot
int plot_heatMap(uint64_t probeTimes[VICTIM_LINES][NUM_OF_SETS]);


// Function to create a linked list from an array of pointers
void* make_linkedlist(void** p_array, int size);

// Function to initialize memory and return a Memory struct
struct Memory ppinit();

// Function to perform memory access
void memoryaccess(void* address, int NumOfLines, int direction);

// Function for the prime step of Prime+Probe
void prime(void **evsets, int NumOfLines,int direction);

// Function for the probe step of Prime+Probe
void probe(void **evsets, uint64_t result[NUM_OF_SETS], int NumOfLines, int direction);

// Function to flush cache lines
void flush(void* head);

// Function to find associativity using Prime+Probe
uint64_t find_associativity_helper(void** evsets, int set_under_test, int NumOfLines);

// Function to find associativity using Prime+Probe and plot the heatmap
void find_associativity(struct Memory mem);

// Function to check if outliers are found in the probe results
int found_outliers(uint64_t tmp[NUM_OF_SETS]);

// Function to perform the Prime+Probe test and plot the heatmap
void Test_PP_HeatMap(struct Memory mem);

