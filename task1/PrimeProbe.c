#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>
#include <sys/mman.h>
#include "PrimeProbe.h"

#define PAGE_SIZE 4096
#define BLOCK_SIZE 64
#define NUM_OF_SETS 64
#define NUM_OF_WAYS 8
#define CACHE_SIZE (BLOCK_SIZE * NUM_OF_WAYS * NUM_OF_SETS)
#define OUTLIER_THRESHOLD 500
#define VICTIM_LINES 12
#define FW 1
//===================================================
#define LNEXT(t)( * (void ** )(t))
#define OFFSET(p, o)((void * )((uintptr_t)(p) + (o)))
#define NEXTPTR(p)(OFFSET((p), sizeof(void * )))
//===================================================

static inline unsigned long long my_rand (unsigned long long limit)
{
	return ((unsigned long long)(((unsigned long long)rand()<<48)^((unsigned long long)rand()<<32)^((unsigned long long)rand()<<16)^(unsigned long long)rand())) % limit;
}


int plot_heatMap(uint64_t probeTimes[VICTIM_LINES][NUM_OF_SETS]){
	FILE *gnuplotPipe = popen("gnuplot -persist", "w");
    if (gnuplotPipe == NULL) {
        printf("Error opening pipe to gnuplot\n");
        return -1;
    }

    // Write the data to a temporary file
    FILE *dataFile = fopen("heatmap_data.txt", "w");
    for (int i = 0; i < VICTIM_LINES; i++) {
        for (int j = 0; j < NUM_OF_SETS; j++) {
            fprintf(dataFile, "%ld ",probeTimes[i][j]);
        }
		fprintf(dataFile, "\n");
    }
    fclose(dataFile);

    // Plot the heatmap using gnuplot
    fprintf(gnuplotPipe, "set pm3d map\n");
    fprintf(gnuplotPipe, "splot 'heatmap_data.txt' matrix with image\n");
    fflush(gnuplotPipe);

    // Wait for user input before closing the gnuplot window
    printf("Press enter to exit...\n");
    getchar();

    // Close the gnuplot pipe
    pclose(gnuplotPipe);

    // Delete the temporary data file
    remove("heatmap_data.txt");
}


void* make_linkedlist(void** p_array, int size){
	for(int i=0;i<size;i++){
		LNEXT(p_array[i]) = p_array[(i+1)%size];
		LNEXT(OFFSET(p_array[i],sizeof(void*))) = p_array[(i+size-1)%size];
	}
	return p_array[0];
}


struct Memory ppinit(){
	struct Memory mem;
	unsigned int dummy;
	void* p1 = mmap(NULL, NUM_OF_WAYS*PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	void* p2 = mmap(NULL, VICTIM_LINES*PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	void** p1_array = (void**)malloc(NUM_OF_WAYS*sizeof(void*));
	void** p2_array = (void**)malloc(VICTIM_LINES*sizeof(void*));

	if (p1 == MAP_FAILED || p1 == MAP_FAILED) {
        perror("mmap");
        exit(0);
    }

	for(int cache_set=0; cache_set < NUM_OF_SETS; cache_set++){
		for (int i = 0; i < NUM_OF_WAYS; i++) p1_array[i] = p1 + i * PAGE_SIZE + cache_set*BLOCK_SIZE;
		for (int i = 0; i < VICTIM_LINES; i++) p2_array[i] = p2 + i * PAGE_SIZE + cache_set*BLOCK_SIZE;
		
		mem.evsets1[cache_set] = make_linkedlist(p1_array, NUM_OF_WAYS);
		mem.evsets2[cache_set] = make_linkedlist(p2_array, VICTIM_LINES);
	}
	
	free(p1_array);
	free(p2_array);
	

	uint64_t start = __rdtscp(&dummy);
	while(__rdtscp(&dummy) - start < 1000000000);

	return mem;
}


void memoryaccess(void* address,int NumOfLines, int direction){
	void *p,*pp;
	int lines = 0;
	p = address;
	pp = p;
	do {
		if(lines >= NumOfLines) break;
		if(direction) p = LNEXT(p);
		else p = LNEXT(NEXTPTR(p));
		lines++;
	} while (p != (void *) pp);
	__asm__ __volatile__("lfence" : : : "memory");
}


void prime(void **evsets, int NumOfLines,int direction) {
	// iterates over the whole cache
	for (int cacheSetIdx = 0; cacheSetIdx < NUM_OF_SETS; cacheSetIdx++){
		memoryaccess(evsets[cacheSetIdx],NumOfLines,direction);
		memoryaccess(evsets[cacheSetIdx],NumOfLines,direction);
		memoryaccess(evsets[cacheSetIdx],NumOfLines,direction);
		memoryaccess(evsets[cacheSetIdx],NumOfLines,direction);
		memoryaccess(evsets[cacheSetIdx],NumOfLines,direction);
		
	}
}


void probe(void **evsets, uint64_t result[NUM_OF_SETS], int NumOfLines, int direction) {
	unsigned int dummy;
	char sets_tested[NUM_OF_SETS] = {0};
	char set_under_test;

	for (int setIdx = 0; setIdx < NUM_OF_SETS; setIdx++) {
		//Randomize the set traversing order.
		do{
			set_under_test = my_rand(NUM_OF_SETS);
		} while(sets_tested[set_under_test]);
		sets_tested[set_under_test] = 1;

		uint64_t start = __rdtscp(&dummy);
		memoryaccess(evsets[set_under_test],NumOfLines, direction);
		result[set_under_test] = __rdtscp(&dummy) - start;
	}
}


void flush(void *head){
	void *p, *pp;
	p = head;
	do {
		pp = p;
		p = LNEXT(p);
		_mm_clflush(pp);
	} while (p != (void *) head);
	_mm_clflush(p);
}


uint64_t find_associativity_helper(void** evsets, int set_under_test, int NumOfLines) {
	int numberOfIterations = 100;
	unsigned int tmp, acc = 0;
	unsigned int dummy;
	uint64_t start;
	void* p = evsets[set_under_test];
	
	//Inset the lines to the cache.
	memoryaccess(p,NumOfLines,FW);
	memoryaccess(p,NumOfLines,FW);
	
	for (int i = 0; i < numberOfIterations; i++) {
		start = __rdtscp(&dummy);
		memoryaccess(p,NumOfLines,FW);
		tmp = __rdtscp(&dummy) - start;
		if(tmp < OUTLIER_THRESHOLD) acc += tmp;
		else i--;
	}
	return (uint64_t)(acc/numberOfIterations);
}


void find_associativity(struct Memory mem){
	uint64_t probeTimes[VICTIM_LINES][NUM_OF_SETS];	
	int sets_tested[NUM_OF_SETS];
	int set_under_test;

	for (int NumOfLines = 0; NumOfLines < VICTIM_LINES; NumOfLines++) {
		
		for(int k = 0; k < NUM_OF_SETS; k++) sets_tested[k] = 0;
		
		for (int cache_set = 0; cache_set < NUM_OF_SETS; cache_set++){
			do{
				set_under_test = my_rand(NUM_OF_SETS);
			} while(sets_tested[set_under_test]);
			sets_tested[set_under_test] = 1;
			
			probeTimes[NumOfLines][set_under_test] = find_associativity_helper(mem.evsets2, set_under_test, NumOfLines);
		}
	}

	plot_heatMap(probeTimes);

}


int found_outliers(uint64_t tmp[NUM_OF_SETS]){
	int found = 0;
	for(int j = 0; j < NUM_OF_SETS; j++){
		if(tmp[j] > OUTLIER_THRESHOLD){
			found = 1;
			break;
		}
	}
	return found;
}


void Test_PP_HeatMap(struct Memory mem){
	uint64_t probeTimes[VICTIM_LINES][NUM_OF_SETS] = {0};	
	uint64_t tmp[NUM_OF_SETS], acc[NUM_OF_SETS];
	int numberOfIterations = 10000;
	
	for (int NumOfLines = 0; NumOfLines < VICTIM_LINES; NumOfLines++){
		for (int i = 0; i < NUM_OF_SETS; i++) acc[i] = 0;
		
		for (int i = 0; i < numberOfIterations; i++){
			prime(mem.evsets1, NUM_OF_WAYS,FW);
			prime(mem.evsets2, NumOfLines,FW);	//victim!!!
			probe(mem.evsets1, tmp, NUM_OF_WAYS,!FW);
			
			if(!found_outliers(tmp)){
				for(int j = 0; j < NUM_OF_SETS; j++) acc[j] += tmp[j];
			}
			else i--;
		}
		for(int j = 0; j < NUM_OF_SETS; j++) probeTimes[NumOfLines][j] = (uint64_t)(acc[j] / numberOfIterations);
	}

	plot_heatMap(probeTimes);

}


int main(){
	struct Memory mem = ppinit();
	Test_PP_HeatMap(mem);
	find_associativity(mem);
	return 0;
}
