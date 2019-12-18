// acc_helper.h
// For helper functions for hw accerlaotr
// Matthew Davis Fall 2019

// ******************************************************************** 
//                              DEFINES
// ******************************************************************** 

// ******************************************************************** 
// Debug defines
#undef DEBUG 
#undef DEBUG_LOOP
#undef DEBUG1
#undef DEBUG2
#undef DEBUG_BRAM
/* #define DEBUG            // Comment out to turn off debug messages */
/* #define DEBUG_LOOP */
/* #define DEBUG1           // Comment out to turn off debug messages */
/* #define DEBUG2 */
/* #define DEBUG_BRAM */

#define PAGE_SIZE 4096
/* #define DMA_MAX_SIZE 16384 */
#define DMA_MAX_SIZE 8192

// ******************************************************************** 
// Memomoy Map
#define CDMA                0x70000000
#define BRAM0               0x40000000
#define BRAM1               0x40000000
#define ACC                 0x44000000
#define OCM                 0xFFFC0000

#define BRAM_LOW            0x00000000
#define BRAM_HIGH           0x00008000

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
#define TRANSFER_SIZE(size) (size<<4)

// ******************************************************************** 
// Regs for Acclerator
#define CHUNK_SIZE          0x10 // 16 bytes per chunk
#define NUM_CHUNKS          0x14
#define START_ADDR          0x18
#define FIRST_REG           0x00
#define SECOND_REG          0x04
#define ENCRYPT             0x08
#define DECRYPT             0x00

#define DMA_MUX             0x00
#define ACC_MUX             0x04

#define ECB_MODE            0x00
#define CTR_MODE            0x01

// ******************************************************************** 
// Device path name for the GPIO device

#define GPIO_TIMER_EN_NUM   0x7             // Set bit 7 to enable timer
#define GPIO_TIMER_CR       0x43C00000      // Control register
#define GPIO_LED_NUM        0x7
#define GPIO_LED            0x43C00004      // LED register
#define GPIO_TIMER_VALUE    0x43C0000C      // Timer value

// ***************  Struct for keeping state of program ******************
//

typedef enum program_mode {
        STRING,
        FILE_MODE,
        TESTBENCH
} pmode;

typedef enum padding_mode {
        NO_PADDING,
        PKCS7
} padding_t;

typedef struct program_state {
        char*           aes_string;
        char*           key_string;
        char*           iv_string;
        char*           output_file;
        pmode           mode;
        padding_t       padding;
        uint32_t        silent;
        uint32_t        verbose;
        uint32_t        encdec;
        uint32_t        timer_value;
        uint32_t*       cdma_addr;
        uint32_t*       bram_addr;
        uint32_t*       acc_addr;
        uint32_t*       ocm_addr;
} pstate;

// ***************  Struct for AES transaction  ******************
//

typedef struct iv {
    uint32_t            nonce[2] ; //
    uint32_t            counter[2] ;
} iv_t;

typedef struct aes_transaction {
        uint32_t*       key;
        uint32_t*       data;
        uint32_t*       writeback_bram_addr;
        uint32_t        bram_read;
        uint32_t        bram_write;
        uint32_t        chunks;
        uint8_t         padded_bytes;
        iv_t            init_vector;
} aes_t;

// ************************  FUNCTION PROTOTYPES ***********************
//       
// ************************  Memory Operations   ***********************
// Set Memory Bit Routine
int smb(unsigned int target_addr, unsigned int pin_number, 
        unsigned int bit_val);
// Put Memory (Put value in target_addr)
int pm(unsigned int target_addr, unsigned int value);
// Remove Memory
unsigned int rm( unsigned int target_addr);
// Set address + offset to specific value
unsigned int address_set(unsigned int* virtual_address, int offset, unsigned int value);
// CDMA get data at address + offset
unsigned int dma_get(unsigned int* dma_virtual_address, int offset);
// Poll cdma to wait for transaction to finish
int cdma_sync(unsigned int* dma_virtual_address);
// Gettter for det_int flag
unsigned int get_det_int();
// Reset int flag
void reset_det_int();
// Signal handler for interrupt
void sighandler(int signo);

// ******************  Init/Gerneral Helper Functions ******************
// Init state from program args
void init_state(int argc, char* argv[], pstate* state);
// Setup interrupt (interrupt handler)
void interrupt_setup(struct sigaction* pAction);
// Setup Memory Mapped I/)
void mm_setup(pstate* state);
// Setup String mode memory
void string_setup(pstate* state, aes_t* transaction);
// Setup File mode memory
void file_setup(pstate* state, aes_t* transaction);
// CTR setup
void ctr_mode_setup(pstate* state, aes_t* transaction, uint32_t* output_addr);
// Write file to address
void write_aes_data(pstate* state, aes_t* transaction, uint32_t* output_addr);
// Setup Testbench memory
void testbench_setup(aes_t* transaction);
// Start accelerator
void start_accelerator(pstate* state, aes_t* transaction);
// Stop Accelerator
void stop_accelerator(pstate* state, aes_t* transaction);
// For printing AES values
void print_aes(char *aes, uint32_t *encrypt, int endian_switch);
// Test SW time
int software_time(char* aes_out, pstate* state);
// Encrypt string (ecb mode, sw)
void encrypt_string(unsigned char* encrypt_out, pstate* state);
// Decrypt string (ecb mode)
void decrypt_string(unsigned char* decrypt_out, pstate* state, unsigned char* in);
// Check sw and hw values
void compare_aes_values(char* hw_aes, char* sw_aes, pstate* state, int diff);
// Output file
void output_file_stuff(pstate* state, aes_t* transaction);
// CDMA transfer
void cdma_transfer(pstate* state, unsigned int dest, unsigned int src, int size);
// Timeval difference
int time_diff(struct timeval first, struct timeval last);
