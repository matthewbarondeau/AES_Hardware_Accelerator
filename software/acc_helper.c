// acc_helper.c
// Matthew Davis

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <math.h> 
#include <limits.h>
#include <sys/mman.h> 
#include <sys/types.h>                
#include <sys/stat.h>  
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <assert.h>
#include <endian.h>

// Includes
#include "acc_helper.h"

// ******************************************************************** 
// Defines
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

// ******************************************************************** 
// One-bit masks for bits 0-31
#define ONE_BIT_MASK(_bit)    (0x00000001 << (_bit))



// ***********************  SET MEMORY BIT (SMB) ROUTINE  *************
// 

int smb(unsigned int target_addr, unsigned int pin_number,
        unsigned int bit_val)
{
    unsigned int reg_data;

    int fd = open("/dev/mem", O_RDWR|O_SYNC);
    
    if(fd == -1)
    {
        printf("Unable to open /dev/mem. Ensure it exists \
                (major=1, minor=1)\n");
        return -1;
    }    

    volatile unsigned int *regs, *address ;
    
    regs = (unsigned int *)mmap(NULL, 
                                MAP_SIZE, 
                                PROT_READ|PROT_WRITE, 
                                MAP_SHARED, 
                                fd, 
                                target_addr & ~MAP_MASK);
    
    address = regs + (((target_addr) & MAP_MASK)>>2);

#ifdef DEBUG1
    printf("REGS           = 0x%.8x\n", regs);    
    printf("Target Address = 0x%.8x\n", target_addr);
    printf("Address        = 0x%.8x\n", address); // display address value
#endif 
   
    /* Read register value to modify */
    
    reg_data = *address;
    
    if (bit_val == 0) {
        
        // Deassert output pin in the target port's DR register
        
        reg_data &= ~ONE_BIT_MASK(pin_number);
        *address = reg_data;
    } else {
        
        // Assert output pin in the target port's DR register
                
        reg_data |= ONE_BIT_MASK(pin_number);
        *address = reg_data;
    }
    
    int temp = close(fd);
    if(temp == -1)
    {
        printf("Unable to close /dev/mem.  Ensure it exists (major=1, minor=1)\n");
        return -1;
    }    

    munmap(NULL, MAP_SIZE);        
    return 0;
   
}    // End of smb routine


// ************************ READ MEMORY (RM) ROUTINE **************************
//
unsigned int rm( unsigned int target_addr) 
{
	int fd = open("/dev/mem", O_RDWR|O_SYNC);
	volatile unsigned int *regs, *address ;
	
	if(fd == -1)
	{
		perror("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");
		return -1;
	}	
		
	regs = (unsigned int *)mmap(NULL, 
	                            MAP_SIZE, 
	                            PROT_READ|PROT_WRITE, 
	                            MAP_SHARED, 
	                            fd, 
	                            target_addr & ~MAP_MASK);		

    address = regs + (((target_addr) & MAP_MASK)>>2);    	
    //printf("Timer register = 0x%.8x\n", *address);
    
	unsigned int rxdata = *address;         // Perform read of SPI 
            	
	int temp = close(fd);                   // Close memory
	if(temp == -1)
	{
		perror("Unable to close /dev/mem.  Ensure it exists (major=1, minor=1)\n");
		return -1;
	}	

	munmap(NULL, MAP_SIZE);                 // Unmap memory
	return rxdata;                          // Return data from read

}   // End of em routine


// ************************ PUT MEMORY (PM) ROUTINE **************************
//

int pm( unsigned int target_addr, unsigned int value ) 
{
	int fd = open("/dev/mem", O_RDWR|O_SYNC);
	volatile unsigned int *regs, *address ;
	
	if(fd == -1)
	{
		perror("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");		
		return -1;
	}	
		
	regs = (unsigned int *)mmap(NULL, 
	                            MAP_SIZE, 
	                            PROT_READ|PROT_WRITE, 
	                            MAP_SHARED, 
	                            fd, 
	                            target_addr & ~MAP_MASK);		

    address = regs + (((target_addr) & MAP_MASK)>>2);    	

	*address = value; 	                    // Perform write command
        	
	int temp = close(fd);                   // Close memory
	if(temp == -1)
	{
		perror("Unable to close /dev/mem.  Ensure it exists (major=1, minor=1)\n");
		return -1;
	}	

	munmap(NULL, MAP_SIZE);                 // Unmap memory
	return 0;                               // Return status

}   // End of pm routine

// *************************  ADDRESS_SET ************************************   
//

unsigned int address_set(unsigned int* virtual_address, int offset, unsigned int value) {
    virtual_address[offset>>2] = value;
}

// ***************************  DMA_GET ************************************ 
//

unsigned int dma_get(unsigned int* dma_virtual_address, int offset) {
    return dma_virtual_address[offset>>2];
}

int cdma_sync(unsigned int* dma_virtual_address) {
    unsigned int status = dma_get(dma_virtual_address, CDMASR);
    if( (status&0x40) != 0)
    {
        unsigned int desc = dma_get(dma_virtual_address, CURDESC_PNTR);
        printf("error address : %X\n", desc);
    }
    while(!(status & 1<<1)){
        status = dma_get(dma_virtual_address, CDMASR);
    }
}

// *************************  SIGHANDLER  ********************************
//  This routine does the interrupt handling for the main loop.
//

void sighandler(int signo)
{
    if (signo==SIGIO)
        det_int++;      // Set flag
    #ifdef DEBUG1        
         printf("Interrupt captured by SIGIO\n");  // DEBUG
    #endif
   
    return;  /* Return to main loop */

}

// *************************  print_aes ******************************
// Puts state of program into pstate based on cmd args

void init_state(int argc, char* argv[], pstate* state)
{
    if (argc < 4 && (strcmp(argv[1], "-tb") != 0)) {
		printf("Wrong number of args for program \n");
		exit(-1);
	}
	
    if (strcmp(argv[1], "-s") == 0) {
	state->mode = STRING;
        state->aes_string = argv[2];
        state->key_string = argv[3];
	printf("String mode, using string \"%s\" \n", state->aes_string);
    }
    else if (strcmp(argv[1], "-f") == 0) {
	/* printf("File mode \n"); */
	state->mode = FILE_MODE;
        state->aes_string = argv[2];
    } else if (strcmp(argv[1], "-tb") == 0) {
        // testbench mode, ie inputs from secwork testbench
        state->mode = TESTBENCH;
    }
    else {
	printf("No mode specifier\n");
	exit(-1);
    }
	
}

// *************************  print_aes ******************************
// Puts hex from encrypt array into aes as a string for printing
// Assumes that aes is 33 bytes (32 bytes for each hex + null)
//
void print_aes(char *aes, uint32_t *encrypt, int endian_switch) {
        char tstr[20];
        for(int index = 0; index < 4; index++) {
                if(endian_switch)
                        sprintf(tstr, "%.8x", be32toh(encrypt[index]));
                else
                        sprintf(tstr, "%.8x", encrypt[index]);
                strcat(aes, tstr);
        }
        /* aes[32] = '\0'; */
}


// *************************  COMPUTE INT LATENCY ***************************
//  This routine does the interrupt handling for the main loop.
//
unsigned long int_sqrt(unsigned long n)
{
	for(unsigned int i = 1; i < n; i++)
	{
		if(i*i > n)
			return i - 1;
	}
}

/* void compute_interrupt_latency_stats( unsigned long   *min_latency_p, */ 
/*                                       unsigned long   *max_latency_p, */ 
/*                                       unsigned long   *average_latency_p, */ 
/*                                       unsigned long   *std_deviation_p, */
/*                                       unsigned long   *intr_latency_measurements; */
/*                                       unsigned int     lp_cnt) */
/* { */
/* 	int number_of_ones = 0; */
/* 	*min_latency_p = INT_MAX; */
/* 	*max_latency_p = 0; */
/* 	unsigned long sum = 0; */
/* 	for(unsigned int index = 0;index < lp_cnt;index++) */
/* 	{ */
/* 		if(intr_latency_measurements[index] < *min_latency_p) */
/* 			*min_latency_p = intr_latency_measurements[index]; */
/* 		if(intr_latency_measurements[index] > *max_latency_p) */
/* 			*max_latency_p = intr_latency_measurements[index]; */
/* 		if(intr_latency_measurements[index] == 1) */
/* 			number_of_ones++; */
/* 		sum += intr_latency_measurements[index]; */
                
/*                 #ifdef DEBUG */
/*                         printf("DEBUG: Number of ones: %ld\n", */ 
/*                                intr_latency_measurements[index]); */
/*                 #endif */
/* 	} */
/* 	*average_latency_p = sum / lp_cnt; */

/* 	//Standard Deviation */
/* 	sum = 0; */
/* 	for(int index = 0;index < lp_cnt;index++) */
/* 	{ */
/* 		int temp = intr_latency_measurements[index] - */ 
/*                            *average_latency_p; */
/* 		sum += temp*temp; */
/* 	} */
/* 	*std_deviation_p = int_sqrt(sum/lp_cnt); */
/* } */

