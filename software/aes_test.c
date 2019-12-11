/*
 * aes_test.c: Test AES_Hardware_Accelerator on the ZED board
 *
 *  AUTHOR: 	Matthew Davis (with help from Mark McDermott)
 *  CREATED: 	December 5th, 2019
 *
 *
 *  DESCRIPTION: This program tests AES ECB encryption for the hardware 
 *               accelerator. First it sets up the CDMA in the PL and performs 
 *               DMA transfers using data provided. There are 2
 *               modes to provide data, a string mode and a file mode denoted
 *               by -s and -f respectively. A key must also be provided by string/
 *               file. The program will then spit out data, or it can be sent
 *               out to a file specified by -o. Using -c compares with software
 *               using the openssl implementation.
 *
 *  DEPENDENCIES: Works on the Xilinx ZED Board only
 *
 *
 */
 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h> 
#include <sys/wait.h>
#include <assert.h>
#include <endian.h>
#include <openssl/aes.h>

// Local includes
#include "acc_helper.h"

// ***********************************************************************
// const for keys/value for testbench values 
static const unsigned char key[] = {
        0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
        0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
        0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
        0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
};

unsigned char text_openssl[] = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
};

//  Time stamp set in the last sigio_signal_handler() invocation:
struct timeval sigio_signal_timestamp;

//  Array of interrupt latency measurements
unsigned long intr_latency_measurements[3000];

// SIGHANDLER Var
volatile unsigned int  data_cnt; 


// ***********************************************************************
//                              MAIN 
// ***********************************************************************

int main(int argc, char * argv[])   {
    FILE* input_file;
    
    // Init state from program args
    pstate state;
    init_state(argc, argv, &state);

    // Open the device file
    struct sigaction action;    // Structure for signalling
    interrupt_setup(&action);   
    
    // Set up memory mapped io
    mm_setup(&state);
        
    // *************************************************************************
    // Set up memory to hold data
    state.chunks = 1;
    if (state.mode == STRING) { // assumes 1 chunk string input
        #ifdef DEBUG
            printf("String mode\n");
        #endif
        
    }
    /* } else if (state.mode == FILE_MODE) { */
        /* input_file = fopen(state.aes_string, "r"); */    
        
    /*     // check file */
    /*     if (input_file == 0) { */
    /*             printf("Unable to open\n"); */
    /*             exit(-1); */
    /*     } */

    /*     // do stuff here */
    /*     else { */
    /*             // read file */
    /*             /1* printf("Reading file\n"); *1/ */
    /*             int index = 0; */
    /*             int c; */
    /*             while ((c = fgetc(input_file)) != EOF) { */
    /*                     buffer[index] = c; */
    /*                     ++index; */
    /*             } */
    /*             fclose(input_file); */
    /*             buffer[index] = 0x80; */

    /*             // figure out size */
    /*             int size = index << 3; // in bits */
    /*             chunks = size/512; */
    /*             /1* if(size % 512 != 0) *1/ */
    /*                     chunks++; */
    /*             /1* printf("chunks: %d, size: %d\n", chunks, size); *1/ */
    /*             /1* buffer[(chunks - 1)*64 + 60] = size; *1/ */
    /*             // Write chunk to ocm for cdma */
    /*             for(int i=0; i<chunks*16; i++)  { */
    /*                     ocm[i] = htobe32(((uint32_t*)buffer)[i]); */
    /*             } */
    /*             /1* printf("finished writing\n"); *1/ */
    /*             ocm[(chunks - 1)*16 + 15] = size; */
    /*     } */

    /* } else if (state.mode == TESTBENCH) { */
    else if(state.mode == TESTBENCH) {
        #ifdef DEBUG
            printf("Testbench mode\n");
        #endif
        
        // Setup testbench data
        testbench_setup(state.ocm_addr,         // key addr
                        &(state.acc_addr[13]),  // in chunk addr
                        &(state.acc_addr[29]),  // addr for write bram addr
                        16);                    // bram addr

    } else {
            printf("Idk how you got here\n");
            return -1;
    }

    
    // RESET DMA
    address_set(state.cdma_addr, CDMACR, 0x0004);	    
    
// ****************************************************************************
// The main meat
//
    pid_t               childpid;
    int                 cnt;
    int                 status;
    int                 wpid;
    unsigned int        timer_value;

    smb(GPIO_LED, GPIO_LED_NUM, 0x0); 
    smb(GPIO_LED, 0x5, 0x0);            // Clear error indicator
    
    // ********************************************************************
    // Fork off a child process to start the DMA process
    // 
    #ifdef DEBUG 
        printf("Fork\n");
    #endif
    
    childpid = vfork();     // Need to use vfork to prevent race condition

    if (childpid >=0)       // Fork suceeded    
    {
        // ****************************************************************
        // This code runs in the child process as the childpid == 0
        // 
            
        if (childpid == 0)
        {
            // Transfer Data into BRAM form CDMA
            #ifdef DEBUG
                printf("Starting cdma transfer\n");
            #endif

            int transfer_size = 16; //1 chunk = 128 bits = 16 bytes
            cdma_transfer(&state, BRAM1, OCM, 16);
               
            #ifdef DEBUG
                printf("CDMA done, starting accelerator now\n");
            #endif

            // setup sha, one for now
            if(state.chunks > 1)
                state.chunks++;
            address_set(state.acc_addr, NUM_CHUNKS, state.chunks); // 1 chunk
            address_set(state.acc_addr, START_ADDR, 0x0); // start addr for bram
            address_set(state.acc_addr, FIRST_REG, 0x0);  // Reset sha unit

            // start timer 
            smb(GPIO_TIMER_CR, GPIO_TIMER_EN_NUM, 0x1);     // Start timer
            smb(GPIO_LED, GPIO_LED_NUM, 0x1);               // Turn on the LED

            // start AES
            address_set(state.acc_addr, FIRST_REG, 0x3);  // Enable aes conversion
            address_set(state.acc_addr, FIRST_REG, 0x2);  // Turn off start
                
            #ifdef DEBUG
                printf("Exiting child process\n");
            #endif

            exit(0);  // Exit the child process
        }
            
        // ***********************************************************
        // This code runs in the parent process as the childpid != 0
        // 
        else 
        {
            // ******************************************************
            // Wait for child process to terminate before checking 
            // for interrupt 
                
            while (!get_det_int());
        
            address_set(state.acc_addr, FIRST_REG, 0x2);  // Disable sha conversion

            // Timer Stop plus store value
            timer_value = rm(GPIO_TIMER_VALUE);             // Read the timer value
            smb(GPIO_TIMER_CR, GPIO_TIMER_EN_NUM, 0x0);     // Disable timer
            smb(GPIO_LED, GPIO_LED_NUM, 0x0);               // Turn off the LED

            // do cmda read back
            int transfer_size = 16; //1 chunk = 128 bits = 16 bytes
            cdma_transfer(&state,
                          (OCM + transfer_size),        // Dest is ocm
                          (BRAM1 + transfer_size),      // Source is BRAM
                          transfer_size);
               
            /* #ifdef DEBUG */
                printf("CDMA write back done, printing\n");
            /* #endif */
                
            // Print value of ecb mode aes from hw
            char aes[80] = {0};
            print_aes(aes, &(state.ocm_addr[4]), 0);
            printf("HW AES is: %s\n", aes);
                
            
            // Calculate in sw and see time
            int diff = 1; // in us
            if(state.mode == TESTBENCH) {
                char sw_aes[80] = {0};
                diff = software_time(sw_aes, key, text_openssl);
                compare_aes_values(aes, sw_aes);
            }

            /* int hw_nsecs = timer_value*13; // 75Mhz = 13,333 us per */ 
            /* int sw_nsecs = diff; */
            /* printf("Time HW =  %d ns\n", timer_value*13); */
            /* printf("Speedup: %d\n", sw_nsecs/hw_nsecs); */
            #ifdef DEBUG
                printf("Done with calculations\n");
            #endif
                
            /* det_int = 0;                    // Clear interrupt detected flag */
            /* address_set(state.cdma_addr, CDMACR, 0x0000);  // Disable interrupts */
                
            /* if(timer_value < 10) smb(GPIO_LED, 0x3, 0x1); */
            /* intr_latency_measurements[cnt] = timer_value; */

        } // if chilpid ==0
    } // if childpid >=0

    else  // fork failed
    {
           perror("Fork failed");
           exit(0);
    } // if childpid >=0
       
/*
        
    // **************** Compute interrupt latency stats *******************
    //
    unsigned long    min_latency; 
    unsigned long    max_latency; 
    unsigned long    average_latency; 
    unsigned long    std_deviation; 
     
    compute_interrupt_latency_stats(
                    &min_latency, 
                    &max_latency,
                    &average_latency, 
                    &std_deviation);

    // **************** Print interrupt latency stats *******************
    //
    printf("-----------------------------------------------\n");
    printf("\nTest passed ----- %d loops and %d words \n", lp_cnt, (data_cnt+1));
    printf("Minimum Latency:    %lu\n" 
           "Maximum Latency:    %lu\n" 
           "Average Latency:    %lu\n" 
           "Standard Deviation: %lu\n",
            min_latency, 
            max_latency, 
            average_latency, 
            std_deviation);
     // Get interrupt number usage from /proc/interrupts
	char * buf = NULL;
	FILE *fp = fopen("/proc/interrupts", "r");
	int len;
	while(getline(&buf, &len, fp) != -1) {
		if (buf[1] == '4' && buf[2] == '6') {
			printf("%s", buf);
			break;
		}
	}
	fclose(fp);
*/
	
                       
    // **************** UNMAP all open Memory Blocks *******************
    //
EXIT:
    munmap(state.ocm_addr,65536);
    munmap(state.cdma_addr,4096);
    munmap(state.bram_addr,4096);
    munmap(state.acc_addr, 4096);
    return 0; 
    
}   // END OF main


