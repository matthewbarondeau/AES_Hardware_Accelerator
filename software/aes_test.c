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
#include <openssl/aes.h>

// ******************************************************************** 
//                              DEFINES
// ******************************************************************** 

// ******************************************************************** 
// Debug defines
#undef DEBUG 
#undef DEBUG1
#undef DEBUG2
#undef DEBUG_BRAM
/* #define DEBUG                    // Comment out to turn off debug messages */
/* #define DEBUG1                   // Comment out to turn off debug messages */
//#define DEBUG2
//#define DEBUG_BRAM

// ******************************************************************** 
// Memomoy Map
#define CDMA                0x70000000
#define BRAM0               0x40000000
#define BRAM1               0x40000000
#define ACC                 0x44000000
#define OCM                 0xFFFC0000

// ******************************************************************** 
// Modes
#define STRING 		    1
#define FILE_MODE	    2
#define TESTBENCH           3

// ******************************************************************** 
// Regs for CDMA
#define CDMACR              0x00
#define CDMASR              0x04
#define CURDESC_PNTR        0x08
#define CURDESC_PNTR_MSB    0x0C
#define TAILDESC_PNTR       0x10
#define TAILDESC_PNTR_MSB   0x14
#define SA                  0x18
#define SA_MSB              0x1C
#define DA                  0x20
#define DA_MSB              0x24
#define BTT                 0x28

// ******************************************************************** 
// Regs for Acclerator
#define CHUNK_SIZE          0x10 // 16 bytes per chunk
#define NUM_CHUNKS          0x14
#define START_ADDR          0x18
#define FIRST_REG           0x00

// ******************************************************************** 
// One-bit masks for bits 0-31

#define ONE_BIT_MASK(_bit)    (0x00000001 << (_bit))

// ******************************************************************** 

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

// ******************************************************************** 
// Device path name for the GPIO device

#define GPIO_TIMER_EN_NUM   0x7             // Set bit 7 to enable timere             
#define GPIO_TIMER_CR       0x43C00000      // Control register
#define GPIO_LED_NUM        0x7
#define GPIO_LED            0x43C00004      // LED register
#define GPIO_TIMER_VALUE    0x43C0000C      // Timer value

// *********************************************************************
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

// *********************************************************************
//  Time stamp set in the last sigio_signal_handler() invocation:

struct timeval sigio_signal_timestamp;

// *********************************************************************
//  Array of interrupt latency measurements

unsigned long intr_latency_measurements[3000];


// ************************  FUNCTION PROTOTYPES ***********************
//       

int smb(unsigned int target_addr, unsigned int pin_number, unsigned int bit_val);
int pm( unsigned int target_addr, unsigned int value );
unsigned int rm( unsigned int target_addr);

unsigned long int_sqrt(unsigned long n);

void compute_interrupt_latency_stats( unsigned long   *min_latency_p, 
                                      unsigned long   *max_latency_p, 
                                      unsigned long   *average_latency_p, 
                                      unsigned long   *std_deviation_p); 


// *************************  ADDRESS_SET ************************************   
//

unsigned int address_set(unsigned int* virtual_address, int offset, unsigned int value) {
    virtual_address[offset>>2] = value;
}

// ***************************  DMA_GET ************************************ 

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
static volatile unsigned int det_int=0;  // Global flag that is volatile i.e., no caching

void sighandler(int signo)
{
    if (signo==SIGIO)
        det_int++;      // Set flag
    #ifdef DEBUG1        
         printf("Interrupt captured by SIGIO\n");  // DEBUG
    #endif
   
    return;  /* Return to main loop */

}

volatile unsigned int  data_cnt, lp_cnt; 

// *************************  COMPUTE INT LATENCY ******************************
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

void compute_interrupt_latency_stats( unsigned long   *min_latency_p, 
                                      unsigned long   *max_latency_p, 
                                      unsigned long   *average_latency_p, 
                                      unsigned long   *std_deviation_p)
{
	int number_of_ones = 0;
	*min_latency_p = INT_MAX;
	*max_latency_p = 0;
	unsigned long sum = 0;
	for(int index = 0;index < lp_cnt;index++)
	{
		if(intr_latency_measurements[index] < *min_latency_p)
			*min_latency_p = intr_latency_measurements[index];
		if(intr_latency_measurements[index] > *max_latency_p)
			*max_latency_p = intr_latency_measurements[index];
		if(intr_latency_measurements[index] == 1)
			number_of_ones++;
		sum += intr_latency_measurements[index];
#ifdef DEBUG
printf("DEBUG: Number of ones: %ld\n", intr_latency_measurements[index]);
#endif
	}
	*average_latency_p = sum / lp_cnt;
//	printf("DEBUG: Number of ones: %d\n", number_of_ones);
	//Standard Deviation
	sum = 0;
	for(int index = 0;index < lp_cnt;index++)
	{
		int temp = intr_latency_measurements[index] - *average_latency_p;
		sum += temp*temp;
	}
	*std_deviation_p = int_sqrt(sum/lp_cnt);
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

// ***********************************************************************
//                              MAIN 
// ***********************************************************************

int main(int argc, char * argv[])   {
    unsigned int rx_cnt;        // Receive count
    struct sigaction action;    // Structure for signalling
    int fd;                     // File descriptor
    int rc;
    int fc;
    char * aes_string;
    char * key_string;
    FILE* input_file;
    int mode;
    
    if (argc < 4 && (strcmp(argv[1], "-tb") != 0)) {
		printf("Wrong number of args for program \n");
		return -1;
	}
	
    if (strcmp(argv[1], "-s") == 0) {
	mode = STRING;
        aes_string = argv[2];
        key_string = argv[3];
	printf("String mode, using string \"%s\" \n", aes_string);
    }
    else if (strcmp(argv[1], "-f") == 0) {
	/* printf("File mode \n"); */
	mode = FILE_MODE;
        aes_string = argv[2];
    } else if (strcmp(argv[1], "-tb") == 0) {
        // testbench mode, ie inputs from secwork testbench
        mode = TESTBENCH;
    }
    else {
	printf("No mode specifier\n");
	return -1;
    }
	

//    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGIO);
    
    action.sa_handler = sighandler;
    action.sa_flags = 0;

    sigaction(SIGIO, &action, NULL);

// *************************************************************************
// Open the device file
//     
    
    fd = open("/dev/acc_int", O_RDWR);
    
    if (fd == -1) {
    	perror("Unable to open /dev/acc_interrupt");
    	rc = fd;
    	exit (-1);
    }
    
    #ifdef DEBUG1
        printf("/dev/dma_int opened successfully \n");    	
    #endif
    
    fc = fcntl(fd, F_SETOWN, getpid());
    
    #ifdef DEBUG1
        printf("Made it through fcntl\n");
    #endif
    
        
    if (fc == -1) {
    	perror("SETOWN failed\n");
    	rc = fd;
    	exit (-1);
    } 
    
    fc= fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_ASYNC);

    if (fc == -1) {
    	perror("SETFL failed\n");
    	rc = fd;
    	exit (-1);
    }   


// *************************************************************************
// Open up /dev/mem for mmap operations
//
    int dh = open("/dev/mem", O_RDWR | O_SYNC); 
    
    if(dh == -1)
	{
		printf("Unable to open /dev/mem.  Ensure it exists (major=1, minor=1)\n");
		printf("Must be root to run this routine.\n");
		return -1;
	}
	                                          
    
    #ifdef DEBUG1                                     
        printf("Getting ready to mmap cdma_virtual_address \n");    
    #endif
    
    uint32_t* cdma_virtual_address = mmap(NULL, 
                                          4096, 
                                          PROT_READ | PROT_WRITE, 
                                          MAP_SHARED, 
                                          dh, 
                                          CDMA); // Memory map AXI Lite register block
    #ifdef DEBUG1                              
        printf("cdma_virtual_address = 0x%.8x\n", cdma_virtual_address); 
    #endif
    
    #ifdef DEBUG1                              
        printf("Getting ready to mmap BRAM_virtual_address \n");    
    #endif
        
    uint32_t* BRAM_virtual_address = mmap(NULL, 
                                          4096, 
                                          PROT_READ | PROT_WRITE, 
                                          MAP_SHARED, 
                                          dh, 
                                          BRAM0); // Memory map AXI Lite register block
    #ifdef DEBUG1        
        printf("BRAM_virtual_address = 0x%.8x\n", BRAM_virtual_address); 
    #endif                                    
        
// *************************************************************************
// Set up the virtual address for the accelerator
//
 
    uint32_t* acc_virtual_addr = mmap(NULL,
                                 4096,
                                 PROT_READ | PROT_WRITE, 
                                 MAP_SHARED, 
                                 dh, 
                                 ACC); 
    
    
// *************************************************************************
// Set up the OCM with data to be transferred to the BRAM
//
    
    uint32_t* ocm = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, dh, OCM);

    // clear memory
    
    // Set up memory to hold data
    int size;
    int chunks = 1;
    char buffer[CHUNK_SIZE] = {0}; // 1 chunk
    if (mode == STRING) { // assumes 1 chunk string input
        printf("String mode\n");
        // Key
        assert(strlen(key_string) == 32); // make sure key is 256 bits plus null terminator
        // put in ocm
        char *pEnd = (char*)key_string;
        pEnd += 4;
        ocm[0] = (uint32_t)strtol(key_string, &pEnd, 16); // big endian
        /* ocm[0] = (uint32_t)key_string[0]; */
        for(int i = 1; i < 8; i++){
                pEnd+=4;
                ocm[i] = (uint32_t)strtol(pEnd, &pEnd, 16); // big endian
                /* ocm[i] = htobe32(((uint32_t*)key_string)[i]); */ 
        }

        // Data
        printf("Put string in ocm\n");
	size = strlen(aes_string); // aes_string in bytes
	if(size < CHUNK_SIZE + 1) { // null terminator
                for (int i = 0; i < size; i++) {
                        buffer[i] = aes_string[i];
                }
                /* buffer[size] = 0x0a; // line feed */
                /* buffer[size + 1] = 0x80; // 1 */
                /* buffer[63] = (size+1)*8; // size in bytes */
	} else {
                printf("Does not support more than 1 chunk rn :(\n");
                exit(-1);
        }
        // Write chunk to ocm for cdma
        for(int i=0; i<4; i++)  {
                ocm[i + 7] = htobe32(((uint32_t*)buffer)[i]); // data goes in ocm[8-11]
        }
    } else if (mode == FILE_MODE) {
        input_file = fopen(aes_string, "r");    
        
        // check file
        if (input_file == 0) {
                printf("Unable to open\n");
                exit(-1);
        }

        // do stuff here
        else {
                // read file
                /* printf("Reading file\n"); */
                int index = 0;
                int c;
                while ((c = fgetc(input_file)) != EOF) {
                        buffer[index] = c;
                        ++index;
                }
                fclose(input_file);
                buffer[index] = 0x80;

                // figure out size
                int size = index << 3; // in bits
                chunks = size/512;
                /* if(size % 512 != 0) */
                        chunks++;
                /* printf("chunks: %d, size: %d\n", chunks, size); */
                /* buffer[(chunks - 1)*64 + 60] = size; */
                // Write chunk to ocm for cdma
                for(int i=0; i<chunks*16; i++)  {
                        ocm[i] = htobe32(((uint32_t*)buffer)[i]);
                }
                /* printf("finished writing\n"); */
                ocm[(chunks - 1)*16 + 15] = size;
        }

    } else if (mode == TESTBENCH) {
        /* // key */
        /* ocm[0] = 0x603deb10; */
        /* ocm[1] = 0x15ca71be; */
        /* ocm[2] = 0x2b73aef0; */
        /* ocm[3] = 0x857d7781; */
        /* ocm[4] = 0x1f352c07; */
        /* ocm[5] = 0x3b6108d7; */
        /* ocm[6] = 0x2d9810a3; */
        /* ocm[7] = 0x0914dff4; */

        /* // chunk */
        /* ocm[8] = 0x6bc1bee2; */
        /* ocm[9] = 0x2e409f96; */
        /* ocm[10] = 0xe93d7e11; */
        /* ocm[11] = 0x7393172a; */

        #ifdef DEBUG
            printf("Testbench mode\n");
        #endif
        // Key
        /* memcpy(&(acc_virtual_addr[13]), key, sizeof(key)); */
        acc_virtual_addr[13] = 0x603deb10;
        acc_virtual_addr[14] = 0x15ca71be;
        acc_virtual_addr[15] = 0x2b73aef0;
        acc_virtual_addr[16] = 0x857d7781;
        acc_virtual_addr[17] = 0x1f352c07;
        acc_virtual_addr[18] = 0x3b6108d7;
        acc_virtual_addr[19] = 0x2d9810a3;
        acc_virtual_addr[20] = 0x0914dff4;

        // chunk
        /* memcpy(&(acc_virtual_addr[21]), text_openssl, sizeof(text_openssl)); */
        acc_virtual_addr[21] = 0x6bc1bee2;
        acc_virtual_addr[22] = 0x2e409f96;
        acc_virtual_addr[23] = 0xe93d7e11;
        acc_virtual_addr[24] = 0x7393172a;

    } else {
            printf("Idk how you got here\n");
            return -1;
    }

    
    
    // RESET DMA
    address_set(cdma_virtual_address, CDMACR, 0x0004);	    

    pid_t               childpid;
    int                 cnt;
    int                 status;
    int                 wpid;
    unsigned int        timer_value;

  
// ****************************************************************************
// The main meat
//
        smb(GPIO_LED, GPIO_LED_NUM, 0x0); 
        smb(GPIO_LED, 0x5, 0x0);            // Clear error indicator

        #ifdef DEBUG 
                printf("Fork\n");
        #endif

        // ********************************************************************
        // Fork off a child process to start the DMA process
        // 
        
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

                /* int transfer_size = chunks*64; //64 bytes per chunk */
                int transfer_size = 96; //key + 1 chunk
                address_set(cdma_virtual_address, DA, BRAM1);       // Write destination address
                address_set(cdma_virtual_address, SA, OCM);         // Write source address
                address_set(cdma_virtual_address, BTT, transfer_size); // Start transfer
                cdma_sync(cdma_virtual_address);
                
                #ifdef DEBUG
                        printf("CDMA done, starting accelerator now\n");
                #endif

                // setup sha, one for now
                if(chunks > 1)
                        chunks++;
                address_set(acc_virtual_addr, NUM_CHUNKS, chunks); // 1 chunk
                address_set(acc_virtual_addr, START_ADDR, 0x0); // start addr
                address_set(acc_virtual_addr, FIRST_REG, 0x0);  // Reset sha unit
                // start sha and timer 
                smb(GPIO_TIMER_CR, GPIO_TIMER_EN_NUM, 0x1);     // Start timer
                smb(GPIO_LED, GPIO_LED_NUM, 0x1);               // Turn on the LED
                address_set(acc_virtual_addr, FIRST_REG, 0x3);  // Enable aes conversion
                address_set(acc_virtual_addr, FIRST_REG, 0x2);  // Turn off start
                
                #ifdef DEBUG
                        printf("Exiting child process\n");
                #endif

                exit(0);  // Exit the child process
            }
            
            // *************************************************************************
            // This code runs in the parent process as the childpid != 0
            // 
          
           else 
           {
                // *********************************************************************
                // Wait for child process to terminate before checking for interrupt 
                // 
                                
                while (!det_int);
                
                
                address_set(acc_virtual_addr, FIRST_REG, 0x2);  // Disable sha conversion
                timer_value = rm(GPIO_TIMER_VALUE);             // Read the timer value
                smb(GPIO_TIMER_CR, GPIO_TIMER_EN_NUM, 0x0);     // Disable timer
                smb(GPIO_LED, GPIO_LED_NUM, 0x0);               // Turn off the LED
                
                // Print value of ecb mode aes from hw
                char aes[80] = {0};
                print_aes(aes, &(acc_virtual_addr[8]), 0);
                printf("HW AES is: %s\n", aes);
                
                // Calculate in sw and see time
                int diff = 1;
                if(mode == TESTBENCH) {
                        // time setup
                        struct timeval first, last;
                        gettimeofday(&first, 0);

                        // Encrypt in software
                        char enc_out[80];
                        AES_KEY enc_key;
                        AES_set_encrypt_key(key, 256, &enc_key);
                        AES_ecb_encrypt(text_openssl, enc_out, &enc_key, 1);

                        // time again
                        gettimeofday(&last, 0);
                        diff = (last.tv_sec - first.tv_sec) * 100000 +
                                (last.tv_usec - first.tv_usec);
                        diff *= 1000; //convert to ns

                        // print
                        char sw_aes[80] = {0};
                        print_aes(sw_aes, (uint32_t*)enc_out, 1);
                        printf("SW aes is: %s\n", sw_aes);
                        int compare = strcmp(aes, sw_aes);
                        if(compare != 0) {
                                printf("SW and HW values not the same!!!\n");
                        } else {
                                printf("SW and HW AES values the same :)\n");
                        }
                        printf("Time SW = %d ns\n", diff);
                }

                /* int hw_nsecs = timer_value*13; // 75Mhz = 13,333 us per */ 
                /* int sw_nsecs = diff; */
                /* printf("Time HW =  %d ns\n", timer_value*13); */
                /* printf("Speedup: %d\n", sw_nsecs/hw_nsecs); */

                /* printf("Done with all that stuff\n"); */
                
                /* det_int = 0;                    // Clear interrupt detected flag */
                /* address_set(cdma_virtual_address, CDMACR, 0x0000);  // Disable interrupts */
                
                /* if(timer_value < 10) smb(GPIO_LED, 0x3, 0x1); */
                /* intr_latency_measurements[cnt] = timer_value; */
                
/*                 // ********************************************************************* */
/*                 // Check to make sure transfer was correct */
/*                 // */ 
                           
                               
/*                 for(int i=0; i < data_cnt; i++) */
/*                 { */
/*                     if(BRAM_virtual_address[i] != ocm[i]) */
/*                     { */
/*                         printf("test failed!!\n"); */ 
/*                         smb(GPIO_LED, 0x5, 0x1); */
/*                         #ifdef DEBUG2 */
/*                         printf("BRAM result: 0x%.8x and c result is 0x%.8x  element %4d\n", */ 
/*                                BRAM_virtual_address[i], ocm[i], i); */
/*                         //printf("data_cnt = 0x%.8x\n", i); */ 
/*                         #endif */
                        
/*                         munmap(ocm,65536); */
/*                         munmap(cdma_virtual_address,4096); */
/*                         munmap(BRAM_virtual_address,4096); */
/*                         munmap(acc_virtual_addr, 4096); */
/*                     return -1; */
/*                     } */
/*                 } */
/*                 #ifdef DEBUG */
/*                 printf("test passed!!\n"); */
/*                 #endif */
                

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
    munmap(ocm,65536);
    munmap(cdma_virtual_address,4096);
    munmap(BRAM_virtual_address,4096);
    munmap(acc_virtual_addr, 4096);
    return 0; 
    
}   // END OF main


// ***********************  SET MEMORY BIT (SMB) ROUTINE  *****************************
// 

int smb(unsigned int target_addr, unsigned int pin_number, unsigned int bit_val)
{
    unsigned int reg_data;

    int fd = open("/dev/mem", O_RDWR|O_SYNC);
    
    if(fd == -1)
    {
        printf("Unable to open /dev/mem. Ensure it exists (major=1, minor=1)\n");
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
    printf("Address        = 0x%.8x\n", address);       // display address value      
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

