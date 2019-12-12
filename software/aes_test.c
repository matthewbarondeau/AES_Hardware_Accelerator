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

// ***********************************************************************
//                              MAIN 
// ***********************************************************************

int main(int argc, char * argv[])   {
    
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
    state.chunks = 1; // Assume 1 chunk unless stated otherwise

    //setup transation
    aes_t transaction = {
            .key = &(state.acc_addr[13]),       // Addr where key is stored
            .data = state.ocm_addr,             // Addr where data is stored
            .writeback_bram_addr = &(state.acc_addr[29]), // writeback address
            .bram_addr = TRANSFER_SIZE(state.chunks)      // actual bram address
    };

    if (state.mode == STRING) { // assumes 1 chunk string input
        #ifdef DEBUG
            printf("String mode\n");
        #endif

        string_setup(&state, &transaction);

    } else if (state.mode == FILE_MODE) {
        #ifdef DEBUG
            printf("File mode\n");
        #endif

        file_setup(&state, &transaction);

    } else if(state.mode == TESTBENCH) {
        #ifdef DEBUG
            printf("Testbench mode\n");
        #endif
        
        // Setup testbench data
        testbench_setup(&transaction);

        // Setup state
        state.key_string = (unsigned char*)key;
        state.aes_string = text_openssl;
    } else {
        printf("Idk how you got here\n");
        return -1;
    }

    
// ****************************************************************************
// The main meat
//
    // Timer reset
    smb(GPIO_LED, GPIO_LED_NUM, 0x0); 
    smb(GPIO_LED, 0x5, 0x0);            // Clear error indicator

    // RESET DMA
    address_set(state.cdma_addr, CDMACR, 0x0004);	    

    // ********************************************************************
    // Fork off a child process to start the DMA process
    // 
    #ifdef DEBUG 
        printf("Fork\n");
    #endif

    pid_t childpid = vfork();     // Need to use vfork to prevent race condition
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

            cdma_transfer(&state, BRAM1, OCM, TRANSFER_SIZE(state.chunks));
               
            #ifdef DEBUG
                printf("CDMA done, starting accelerator now\n");
            #endif

            // setup aes, one for now
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
            // Check for interrupt 
            while (!get_det_int());
        
            address_set(state.acc_addr, FIRST_REG, 0x2);        // Disable aes conversion

            // Timer Stop plus store value
            state.timer_value = rm(GPIO_TIMER_VALUE);           // Read the timer value
            smb(GPIO_TIMER_CR, GPIO_TIMER_EN_NUM, 0x0);         // Disable timer
            smb(GPIO_LED, GPIO_LED_NUM, 0x0);                   // Turn off the LED

            // do cmda read back
            cdma_transfer(&state,
                          (OCM + TRANSFER_SIZE(state.chunks)),  // Dest is ocm
                          (BRAM1 + TRANSFER_SIZE(state.chunks)),// Source is BRAM
                          TRANSFER_SIZE(state.chunks));
               
            /* #ifdef DEBUG */
                printf("CDMA write back done, printing\n");
            /* #endif */
                
            // Print value of ecb mode aes from hw
            char aes[80] = {0};
            print_aes(aes, &(state.ocm_addr[4]), 0);
            printf("HW AES is: %s\n", aes);
                
            
            // Calculate in sw and see time
            if(state.mode == STRING) {
                char sw_aes[80] = {0};
                int diff = software_time(sw_aes, &state);
            } else if(state.mode == TESTBENCH) {
                char sw_aes[80] = {0};
                // diff is in us
                int diff = software_time(sw_aes, &state);
                compare_aes_values(aes, sw_aes, &state, diff);
            }

            #ifdef DEBUG
                printf("Done with calculations\n");
            #endif
                
            /* det_int = 0;                    // Clear interrupt detected flag */
            /* address_set(state.cdma_addr, CDMACR, 0x0000);  // Disable interrupts */
                
        } // if chilpid ==0
    } // if childpid >=0

    else  // fork failed
    {
        perror("Fork failed");
        exit(0);
    } // if childpid >=0
    
    // **************** UNMAP all open Memory Blocks *******************
    //
EXIT:
    munmap(state.ocm_addr,65536);
    munmap(state.cdma_addr,4096);
    munmap(state.bram_addr,4096);
    munmap(state.acc_addr, 4096);
    return 0; 
    
}   // END OF main


