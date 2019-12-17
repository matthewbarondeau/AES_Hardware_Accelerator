// acc_helper.c
// Matthew Davis

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <argp.h>
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

    *address = value;                           // Perform write              

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
static volatile unsigned int det_int=0;     // Global flag that is volatile

// getter for flag
unsigned int get_det_int()
{
        return det_int;
}
// interrupt handler (increments flag)
void sighandler(int signo)
{
    if (signo==SIGIO)
        det_int++;      // Set flag
    #ifdef DEBUG1        
         printf("Interrupt captured by SIGIO\n");  // DEBUG
    #endif
   
    return;  /* Return to main loop */

}

// *************************  Init State ******************************
// Puts state of program into pstate based on cmd args

// stuff for argp
const char *argp_program_version =
  "aes-test 0.2";

/* Program documentation. */
static char doc[] =
    "AES Test -- a program that does AES Encryption with HW acceleration";

/* A description of the arguments we accept. */
static char args_doc[] = "ENC/DEC DATA KEY";

/* The options we understand. */
static struct argp_option options[] = {
  {"verbose",  'v', 0,      0,  "Produce verbose output" },
  {"quiet",    'q', 0,      0,  "Don't produce any output" },
  {"testbench",'t', 0, 	    0,  "Use testbench inputs"},
  {"string",   's', 0,      0,  "Use string as key and data input" },
  {"file",     'f', 0,      0,  "Use file as key and data input"},
  {"padding",  'p', "PADDING", 0, "Type of padding to use (no-padding, PKCS7)"},
  {"output",   'o', "FILE", 0,
   "Output to FILE instead of standard output" },
  { 0 }
};

/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  pstate* ps = state->input;

  switch (key)
    {
    case 'v':
        ps->verbose = 1;
      break;
    case 'q': 
        ps->silent = 1;
        break;
    case 't':
	ps->mode = TESTBENCH;
        break;
    case 's':
	ps->mode = STRING;
        break;
    case 'f':
	ps->mode = FILE_MODE;
        break;
    case 'p': 
        if(arg[0] == 'n') // no padding
            ps->padding = NO_PADDING;
        break;
    case 'o':
	ps->output_file = arg;
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= 3)
            /* Too many arguments. */
            argp_usage (state);

	if(state->arg_num == 0) {
            if(arg[0] == 'e')
                ps->encdec = ENCRYPT;
            else
                ps->encdec = DECRYPT;
        } else if (state->arg_num == 1)
            ps->aes_string = arg;
	else
            ps->key_string = arg;

        break;
    case ARGP_KEY_END:
	// Do nothing, no minimum number of args
        break;

    default:
          return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

void init_state(int argc, char* argv[], pstate* state)
{

    /* Default values. */
    state->silent = 0;
    state->verbose = 0;
    state->padding = PKCS7;
    state->encdec = ENCRYPT;
    state->output_file = "-";

    /* Parse our arguments; every option seen by parse_opt will
     be reflected in arguments. */
    argp_parse (&argp, argc, argv, 0, 0, state);	

}

// *************************  Interrupt setup ****************************
// Sets up sighandler for interrupt 
//
void interrupt_setup(struct sigaction* pAction)
{
    // Setup signal SIGIO to call sighandler()
    sigemptyset(&(pAction->sa_mask));
    sigaddset(&(pAction->sa_mask), SIGIO);
    pAction->sa_handler = sighandler;
    pAction->sa_flags = 0;
    sigaction(SIGIO, pAction, NULL);


    int fd, fc, rc;                     // File descriptor

    // Open interrupt as file descriptor
    fd = open("/dev/acc_int", O_RDWR);
    if (fd == -1) {
        perror("Unable to open /dev/acc_interrupt");
        rc = fd;
        exit (-1);
    }
    
    #ifdef DEBUG1
        printf("/dev/dma_int opened successfully \n");          
    #endif
    
    // Set the interrupt to own this pid
    fc = fcntl(fd, F_SETOWN, getpid());
    
    #ifdef DEBUG1
        printf("Made it through fcntl\n");
    #endif
    
    if (fc == -1) {
        perror("SETOWN failed\n");
        rc = fd;
        exit (-1);
    } 
    
    // Set the interrupt to send SIGIO
    fc= fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_ASYNC);

    if (fc == -1) {
        perror("SETFL failed\n");
        rc = fd;
        exit (-1);
    }
}

// *************************  MM I/O setup ***************************
// Map Memory Mapped I/O registers to correct address
// Open up /dev/mem for mmap operations
//
void mm_setup(pstate* state)
{
    int dh = open("/dev/mem", O_RDWR | O_SYNC); 
    
    if(dh == -1) {
        printf("Unable to open /dev/mem.  \
                Ensure it exists (major=1, minor=1)\n");
        printf("Must be root to run this routine.\n");
        exit(-1);
    }
                                                  
    // Open CMDA Address  
    #ifdef DEBUG1                                     
        printf("Getting ready to mmap cdma_virtual_address \n");    
    #endif
    
    state->cdma_addr = mmap(NULL, 
                            4096, 
                            PROT_READ | PROT_WRITE, 
                            MAP_SHARED, 
                            dh, 
                            CDMA); // Memory map AXI Lite register block
    #ifdef DEBUG1                              
        printf("cdma_virtual_address = 0x%.8x\n", cdma_virtual_address); 
    #endif
    
    // Open BRAM Address  
    #ifdef DEBUG1                              
        printf("Getting ready to mmap BRAM_virtual_address \n");    
    #endif
        
    state->bram_addr = mmap(NULL, 
                            4096, 
                            PROT_READ | PROT_WRITE, 
                            MAP_SHARED, 
                            dh, 
                            BRAM0); // Memory map AXI Lite register block
    #ifdef DEBUG1        
        printf("BRAM_virtual_address = 0x%.8x\n", BRAM_virtual_address); 
    #endif                                    

    // Set up the virtual address for the accelerator
    state->acc_addr = mmap(NULL,
                           4096,
                           PROT_READ | PROT_WRITE, 
                           MAP_SHARED, 
                           dh, 
                           ACC); 

    // Set up the OCM with data to be transferred to the BRAM
    state->ocm_addr = mmap(NULL, 
                           65536, 
                           PROT_READ | PROT_WRITE, 
                           MAP_SHARED, 
                           dh, 
                           OCM);

}  

// *************************  String setup ***************************
// Set key and chunk data to correct value
// Assumes key and data values are ascii not hex (ie 'a' is 0x61, not 0x0A)

void string_setup(pstate* state, aes_t* transaction)
{

    // Key
    assert(strlen(state->key_string) == 32); // 32*8= 256 bits 

    // put key in key buffer
    for(int i = 0; i < 8; i++){
        transaction->key[i] = htobe32(((uint32_t*)(state->key_string))[i]);
    }

    // Data
    #ifdef DEBUG
        printf("Put string in data buffer\n");
    #endif
    int size = strlen(state->aes_string); // aes_string in bytes
    char buffer[CHUNK_SIZE] = {0}; // 1 chunk can probably to straight to ocm
    // If size is less than 1 chunk
    if(size < CHUNK_SIZE + 1) { 
        for (int i = 0; i < size; i++) {
            buffer[i] = state->aes_string[i];
            #ifdef DEBUG
		if(state->verbose)
		    printf("Buffer[%d]: %x\n", i, buffer[i]);
            #endif
        }

        if(state->padding == PKCS7) { 
            // pading using PKCS7
            int number_of_zeroes = CHUNK_SIZE - size;
            for(int i = size; i < CHUNK_SIZE; i++) {
                buffer[i] = (char)number_of_zeroes;
                #ifdef DEBUG
                    if(state->verbose)
                        printf("Buffer[%d]: %x\n", i, buffer[i]);
                #endif
            }
        } else { // padding is no padding
            for(int i = size + 1; i < CHUNK_SIZE; i++) {
                buffer[i] = '0';
                #ifdef DEBUG
                    if(state->verbose)
                        printf("Buffer[%d]: %x\n", i, buffer[i]);
                #endif
            }
        }
    } else {
        printf("Size: %d\n", size);
        printf("Does not support more than 1 chunk rn :(\n");
        exit(-1);
    }

    // Write chunk to data_buffer for cdma
    for(int i=0; i<transaction->chunks*4; i++)  {
        transaction->data[i] = htobe32(((uint32_t*)buffer)[i]);
    }

    *(transaction->writeback_bram_addr) = transaction->bram_addr;

}

// *************************  File Mode setup ***************************
// Setup File mode memory
void file_setup(pstate* state, aes_t* transaction)
{

    // Need to get key input
    FILE* key_input = fopen(state->key_string, "r");

    // check file
    if (key_input == 0) {
        printf("Unable to open key file\n");
    } else { 
         // read file
         #ifdef DEBUG
            printf("Reading data file\n");
         #endif
         int index = 0;
         int c;

         // Get key
         while ((c = fgetc(key_input)) != EOF && c != '\n') {
             ((char*)transaction->key)[index] = c;
             ++index;
             if(index > 32) {
                 // Key too big
                 printf("Key file too big\n");
                 exit(-1);
             }
         }
         fclose(key_input);

         // figure out size
    }

    // open file
    FILE* data_input = fopen(state->aes_string, "r");    

    char buffer[PAGE_SIZE + CHUNK_SIZE] = {0}; // 1 page + 1 chunk in case of padding
    
    // check file
    if (data_input == 0) {
        printf("Unable to open data file\n");
        exit(-1);
    } else { 
         // read file
         #ifdef DEBUG
            printf("Reading data file\n");
         #endif

         // try using fread
         long size, pages;
         fseek(data_input, 0, SEEK_END);
         size = ftell(data_input);
         rewind(data_input);

         // figure out size
         transaction->chunks = size/CHUNK_SIZE;
         pages = size/PAGE_SIZE;
         if(size%PAGE_SIZE)
             pages++;

         #ifdef DEBUG 
             printf("Chunks: %d, size: %lu, pages: %lu\n", 
                     transaction->chunks, size, pages);
         #endif

         for(int page = 0; page < pages; page++) {
             // Put stuff in Buffer
             int length = size >= PAGE_SIZE ? PAGE_SIZE : size; 
             if(size < PAGE_SIZE) { // Last data
                fread(buffer, 1, size, data_input);

                // Padding
                uint8_t padded_bytes = CHUNK_SIZE - (size % CHUNK_SIZE);
                if( state->padding == PKCS7) { 
                    // PKCS7 padding
                    for(int i = 0; i < padded_bytes; i++) {
                        buffer[size + i] = padded_bytes;
                    }
                    length += padded_bytes;
                    transaction->chunks++;
                } else { // No padding
                    if(size % CHUNK_SIZE != 0) { // will have to pad last chunk
                        transaction->chunks++;
                        // do padding here
                        for(int i = 0; i < padded_bytes; i++) {
                            buffer[size + i] = 0;
                        }
                    }
                }
             } else { // read page
                fread(buffer, 1, PAGE_SIZE, data_input);
                size -= PAGE_SIZE;
             }

             // Write chunk to buffer for cdma
             for(int i=0; i < length/4; i++)  { // 4 32 bit words in a chunk
                 transaction->data[i + page*PAGE_SIZE/4] = htobe32(((uint32_t*)buffer)[i]);
             }
         }
         
         

         #ifdef DEBUG
             printf("finished writing\n");
         #endif

         // Close file
         fclose(data_input);

         transaction->bram_addr = TRANSFER_SIZE(transaction->chunks);
         transaction->writeback_bram_addr[0] = transaction->bram_addr;
    }
}


// *************************  Testbench setup ***************************
// Set key and chunk data to correct value 

void testbench_setup(aes_t* transaction)
{
    // Set key
    transaction->key[0] = 0x603deb10;
    transaction->key[1] = 0x15ca71be;
    transaction->key[2] = 0x2b73aef0;
    transaction->key[3] = 0x857d7781;
    transaction->key[4] = 0x1f352c07;
    transaction->key[5] = 0x3b6108d7;
    transaction->key[6] = 0x2d9810a3;
    transaction->key[7] = 0x0914dff4;

    // Set data
    transaction->data[0] = 0x6bc1bee2;
    transaction->data[1] = 0x2e409f96;
    transaction->data[2] = 0xe93d7e11;
    transaction->data[3] = 0x7393172a;


    transaction->data[4] = 0x5bc1bee2;
    transaction->data[5] = 0x2e409f96;
    transaction->data[6] = 0xe93d7e11;
    transaction->data[7] = 0x7393172a;

    
    // Set number of chunks
    transaction->chunks = 2;

    // Set BRAM addr chunks written back to
    transaction->bram_addr = TRANSFER_SIZE(transaction->chunks);
    transaction->writeback_bram_addr[0] = transaction->bram_addr;

}

// *************************  print_aes ******************************
// Puts hex from encrypt array into aes as a string for printing
// Assumes that aes is 33 bytes (32 bytes for each hex + null)

void print_aes(char *aes, uint32_t *encrypt, int endian_switch) {
    char tstr[20];
    for(int index = 0; index < 4; index++) {
        if(endian_switch) {
            sprintf(tstr, "%.8x", be32toh(encrypt[index]));
        } else {
            sprintf(tstr, "%.8x", encrypt[index]);
        }
        strcat(aes, tstr);
    }
}


// *************************  AES Software ***************************
// Does AES in software with key and text
// Returns time, puts data out in aes_out

int software_time(char* aes_out, pstate* state)
{
    // time setup
    struct timeval first, last;
    gettimeofday(&first, 0);
    int diff;

    // Encrypt in software
    char enc_out[80];
    encrypt_string(enc_out, state);

    // time again
    gettimeofday(&last, 0);
    diff = (last.tv_sec - first.tv_sec) * 100000 +
    (last.tv_usec - first.tv_usec);
    diff *= 1000; //convert to ns

    // print
    print_aes(aes_out, (uint32_t*)enc_out, 1);
    printf("SW aes is: %s\n", aes_out);
    /* printf("Time SW = %d ns\n", diff); */
    
    // Make sure encrypted text decrypts
    char decrypt_out[80], temp_string[80] = {0};
    decrypt_string(decrypt_out, state, enc_out);
    print_aes(temp_string, (uint32_t*)decrypt_out, 1);
    printf("Decrypted SW aes is: %s\n", temp_string);
}

// encrypt text
void encrypt_string(unsigned char* encrypt_out, pstate* state)
{
    AES_KEY enc_key;
    AES_set_encrypt_key(state->key_string, 256, &enc_key);
    AES_ecb_encrypt(state->aes_string, encrypt_out, &enc_key, 1);
}

// decrypt string
void decrypt_string(unsigned char* decrypt_out, pstate* state, unsigned char* in)
{
    AES_KEY enc_key;
    AES_set_decrypt_key(state->key_string, 256, &enc_key);
    AES_ecb_encrypt(in, decrypt_out, &enc_key, 0);
}

// Compare 2 AES values
void compare_aes_values(char* hw_aes, char* sw_aes, pstate* state,
                        int diff)
{
    if(state->mode != FILE_MODE) {
    int compare = strcmp(hw_aes, sw_aes);
    if(compare != 0) {
        printf("SW and HW values not the same!!!\n");
        /* exit(-1); */
    } 
    #ifdef DEBUG 
        else {
            printf("SW and HW AES values the same :)\n");
        }
    }
    
    int hw_nsecs = state->timer_value*10; // 100Mhz = 10 ns per 
    int sw_nsecs = diff;
    printf("Time HW =  %d ns\n", state->timer_value*13);
    printf("Speedup: %d\n", sw_nsecs/hw_nsecs);

    #endif

    #ifndef DEBUG
    }
    #endif
}

// *************************  Output File ******************************
// Puts encrypted data into file

void output_file_stuff(pstate* state, aes_t* transaction) {
    
    // open file
    FILE* output_file = fopen(state->output_file, "w");

    // Pointer where data is
    uint32_t* encrypted_data = &(transaction->data[transaction->chunks*4]);

    uint32_t buffer[PAGE_SIZE/4] = {0}; // 1 page
    // just doing 1 chunk for now
    
    // check file
    if (output_file == 0) {
        printf("Unable to open output file\n");
        exit(-1);
    } else { 
         // read file
         #ifdef DEBUG
            printf("Starting output file \n");
         #endif
         
         // Figure out size
         long size, pages;
         size = TRANSFER_SIZE(transaction->chunks);
         pages = size/PAGE_SIZE;
         if(size%PAGE_SIZE != 0)
             pages++;

         #ifdef DEBUG 
             printf("Chunks: %d, bytes: %lu, pages: %lu\n", 
                     transaction->chunks, size, pages);
         #endif

         for(int page = 0; page < pages; page++) {
             // Put stuff in Buffer
             int length = size > PAGE_SIZE ? PAGE_SIZE : size; 

             // Write chunk to buffer for cdma
             for(int i=0; i < length/4; i++)  { // 4 32 bit words in a chunk
                 buffer[i] = be32toh((encrypted_data)[i + page*(PAGE_SIZE/4)]);
             }
             
             // Write buffer to disk
             if(size < PAGE_SIZE) {
                fwrite(buffer, 1, size, output_file);
             } else {
                fwrite(buffer, 1, PAGE_SIZE, output_file);
                size -= PAGE_SIZE;
             }
         }
         
         /* // Padding */
         /* if(size % CHUNK_SIZE != 0) { // will have to pad last chunk */
         /*    transaction->chunks++; */
         /* } */

         // Close file
         fclose(output_file);

         #ifdef DEBUG
             printf("finished writing\n");
         #endif
    }
}

// *************************  CDMA Transfer ******************************
// Does cmda transfer from dest to src for size number of bytes
void cdma_transfer(pstate* state, unsigned int dest, unsigned int src, int size)
{
     address_set(state->cdma_addr, DA, dest); // Write destination address
     address_set(state->cdma_addr, SA, src);   // Write source address
     address_set(state->cdma_addr, BTT, size); // Start transfer
     cdma_sync(state->cdma_addr);
}

// *************************  CDMA Transfer ******************************
// Does cmda transfer from dest to src for size number of bytes

int time_diff(struct timeval first, struct timeval last) {
    // time again
    return (last.tv_sec - first.tv_sec) * 100000 +
    (last.tv_usec - first.tv_usec);
}

// *************************  COMPUTE INT LATENCY ***************************
//  This routine does the interrupt handling for the main loop.
//
/* unsigned long int_sqrt(unsigned long n) */
/* { */
/*      for(unsigned int i = 1; i < n; i++) */
/*      { */
/*              if(i*i > n) */
/*                      return i - 1; */
/*      } */
/* } */

/* void compute_interrupt_latency_stats( unsigned long   *min_latency_p, */ 
/*                                       unsigned long   *max_latency_p, */ 
/*                                       unsigned long   *average_latency_p, */ 
/*                                       unsigned long   *std_deviation_p, */
/*                                       unsigned long   *intr_latency_measurements; */
/*                                       unsigned int     lp_cnt) */
/* { */
/*      int number_of_ones = 0; */
/*      *min_latency_p = INT_MAX; */
/*      *max_latency_p = 0; */
/*      unsigned long sum = 0; */
/*      for(unsigned int index = 0;index < lp_cnt;index++) */
/*      { */
/*              if(intr_latency_measurements[index] < *min_latency_p) */
/*                      *min_latency_p = intr_latency_measurements[index]; */
/*              if(intr_latency_measurements[index] > *max_latency_p) */
/*                      *max_latency_p = intr_latency_measurements[index]; */
/*              if(intr_latency_measurements[index] == 1) */
/*                      number_of_ones++; */
/*              sum += intr_latency_measurements[index]; */
                
/*                 #ifdef DEBUG */
/*                         printf("DEBUG: Number of ones: %ld\n", */ 
/*                                intr_latency_measurements[index]); */
/*                 #endif */
/*      } */
/*      *average_latency_p = sum / lp_cnt; */

/*      //Standard Deviation */
/*      sum = 0; */
/*      for(int index = 0;index < lp_cnt;index++) */
/*      { */
/*              int temp = intr_latency_measurements[index] - */ 
/*                            *average_latency_p; */
/*              sum += temp*temp; */
/*      } */
/*      *std_deviation_p = int_sqrt(sum/lp_cnt); */
/* } */
   
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
// SIGHANDLER Var
/* volatile unsigned int  data_cnt; */ 
//  Array of interrupt latency measurements
/* unsigned long intr_latency_measurements[3000]; */
// ************************  Functions for latency testing
/* unsigned long int_sqrt(unsigned long n); */
/* void compute_interrupt_latency_stats( unsigned long   *min_latency_p, */ 
/*                                       unsigned long   *max_latency_p, */ 
/*                                       unsigned long   *average_latency_p, */ 
/*                                       unsigned long   *std_deviation_p, */
/*                                       unsigned long   *intr_latency_measurements; */
/*                                       unsigned int    lp_cnt); */ 


