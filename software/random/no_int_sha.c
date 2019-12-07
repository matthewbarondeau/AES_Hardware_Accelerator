/*
 * DMA_test.c: Test the sha256 on the ZED board
 *
 *  AUTHOR: 	Mark McDermott
 *  CREATED: 	Nov 12, 2019
 *
 *
 *  DESCRIPTION: This program sets up the CDMA in the PL and performs a
 *               multiple DMA transfers using random data
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

// ******************************************************************** 

#undef DEBUG 
#undef DEBUG1
#undef DEBUG2
#undef DEBUG_BRAM
#define DEBUG                    // Comment out to turn off debug messages
//#define DEBUG1                   // Comment out to turn off debug messages
//#define DEBUG2
//#define DEBUG_BRAM

#define CDMA                0x70000000
#define BRAM0               0x40000000
#define BRAM1               0x40000000
#define ACC                 0x44000000
#define OCM                 0xFFFC0000

#define STRING 		    1
#define FILE		    2

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

// Acclerator define
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

// ***********************************************************************
//                              MAIN 
// ***********************************************************************

int main(int argc, char * argv[])   {
    unsigned int rx_cnt;        // Receive count
    struct sigaction action;    // Structure for signalling
    int fd;                     // File descriptor
    int rc;
    int fc;
    char * sha_string;
    int mode;
    
    if ((argc < 3)) {
		printf("Wrong number of args for program \n");
		return -1;
	}
	
    if (strcmp(argv[1], "-s") == 0) {
	mode = STRING;
        sha_string = argv[2];
	printf("String mode, using string \"%s\" \n", sha_string);
    }
    else if (strcmp(argv[1], "-f") == 0) {
	printf("File mode \n");
	mode = FILE;
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
    
    /* fd = open("/dev/acc_int", O_RDWR); */
    
    /* if (fd == -1) { */
    /* 	perror("Unable to open /dev/acc_interrupt"); */
    /* 	rc = fd; */
    /* 	exit (-1); */
    /* } */
    
    /* #ifdef DEBUG1 */
    /*     printf("/dev/dma_int opened successfully \n"); */    	
    /* #endif */
    
    /* fc = fcntl(fd, F_SETOWN, getpid()); */
    
    /* #ifdef DEBUG1 */
    /*     printf("Made it through fcntl\n"); */
    /* #endif */
    
        
    /* if (fc == -1) { */
    /* 	perror("SETOWN failed\n"); */
    /* 	rc = fd; */
    /* 	exit (-1); */
    /* } */ 
    
    /* fc= fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_ASYNC); */

    /* if (fc == -1) { */
    /* 	perror("SETFL failed\n"); */
    /* 	rc = fd; */
    /* 	exit (-1); */
    /* } */   


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
// Set up the OCM with data to be transferred to the BRAM
//
    
    uint32_t* ocm = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, dh, OCM);

    // clear memory
    memset(ocm, 0, 128);
    
    // Set up memory to hold data
    int size;
    uint32_t buffer[16] = {0}; // 1 chunk
    if (mode == STRING) {
	size = strlen(sha_string); // sha_string in bytes
	if(size < 512) {
		/* memcpy(buffer, sha_string, size); */
                buffer[0] = 0x6262630a;
                buffer[1] = 0x80000000;
                buffer[15] = 0x00000020;
		/* buffer[15] = size*4; // number of bits */
	}
    }	

    // 1 chunk right now
    for(int i=0; i<16; i++)  {
        ocm[i] = buffer[i];
        /* BRAM_virtual_address[i] = buffer[i]; */
        //ocm[i] = rand();
    }
    
    // RESET DMA
    address_set(cdma_virtual_address, CDMACR, 0x0004);	    

    pid_t               childpid;
    int                 cnt;
    int                 status;
    int                 wpid;
    unsigned int        timer_value;

// *************************************************************************
// Set up the virtual address for the accelerator
//
 
    uint32_t* acc_virtual_addr = mmap(NULL,
                                 4096,
                                 PROT_READ | PROT_WRITE, 
                                 MAP_SHARED, 
                                 dh, 
                                 ACC); 
   
// ****************************************************************************
// The main meat
//
        /* smb(GPIO_LED, GPIO_LED_NUM, 0x0); */ 
        /* smb(GPIO_LED, 0x5, 0x0);            // Clear error indicator */
        printf("Starting cdma transfer\n");

                int transfer_size = 16;
                address_set(cdma_virtual_address, DA, BRAM1);       // Write destination address
                address_set(cdma_virtual_address, SA, OCM);         // Write source address
                address_set(cdma_virtual_address, BTT, transfer_size*4); // Start transfer
                cdma_sync(cdma_virtual_address);

                // setup sha, one for now
                address_set(acc_virtual_addr, NUM_CHUNKS, 0x1); // 1 chunk
                address_set(acc_virtual_addr, START_ADDR, 0x0); // start addr
                address_set(acc_virtual_addr, FIRST_REG, 0x0);  // Reset sha unit
                address_set(acc_virtual_addr, FIRST_REG, 0x3);  // Enable sha conversion
                sleep(1);
                address_set(acc_virtual_addr, FIRST_REG, 0x2);  // Disable sha conversion
        
                // get sha
                // digest starts in reg8
                for(int index = 0; index < 8; index++) {
                        uint32_t temp = acc_virtual_addr[8+index];
                        printf("%.2x",(char)((temp&0xFF000000)>>24));
                        printf("%.2x",(char)((temp&0x00FF0000)>>16));
                        printf("%.2x",(char)((temp&0x0000FF00)>>8));
                        printf("%.2x",(char)((temp&0x000000FF)));
                }
                printf("\n");

                printf("Done with all that stuff\n");

   

        /*         // start sha and timer */
/*         smb(GPIO_TIMER_CR, GPIO_TIMER_EN_NUM, 0x1);     // Start timer */
/*         smb(GPIO_LED, GPIO_LED_NUM, 0x1);               // Turn on the LED */


        // ********************************************************************
        // Fork off a child process to start the DMA process
        // 
        
        /* childpid = vfork();     // Need to use vfork to prevent race condition */

        /* if (childpid >=0)       // Fork suceeded */    
        /* { */
        /*     // **************************************************************** */
        /*     // This code runs in the child process as the childpid == 0 */
        /*     // */ 
            
        /*     if (childpid == 0) */
        /*     { */
        /*                         printf("Exiting child process\n"); */

        /*         exit(0);  // Exit the child process */
        /*     } */
            
        /*     // ************************************************************************* */
        /*     // This code runs in the parent process as the childpid != 0 */
        /*     // */ 
          
        /*    else */ 
        /*    { */
        /*         // ********************************************************************* */
        /*         // Wait for child process to terminate before checking for interrupt */ 
        /*         // */ 
                                
        /*         //waitpid(childpid, &status, WCONTINUED); */

        /*         //if (!det_int) continue;       // Go back to top of while loop. */
        /*         while (!det_int); */

                
                /* timer_value = rm(GPIO_TIMER_VALUE);             // Read the timer value */
                
                /* det_int = 0;                    // Clear interrupt detected flag */
                /* address_set(cdma_virtual_address, CDMACR, 0x0000);  // Disable interrupts */
                
                /* smb(GPIO_TIMER_CR, GPIO_TIMER_EN_NUM, 0x0);     // Disable timer */
                /* smb(GPIO_LED, GPIO_LED_NUM, 0x0);               // Turn off the LED */
                
                /* //printf("Timer register = 0x%.8x\n", timer_value); */
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
                

/*              } // if chilpid ==0 */
/*        } // if childpid >=0 */

/*        else  // fork failed */
/*        { */
/*            perror("Fork failed"); */
/*            exit(0); */
/*        } // if childpid >=0 */
       
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

