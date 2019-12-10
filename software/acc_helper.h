// acc_helper.h
// For helper functions for hw accerlaotr
// Matthew Davis Fall 2019



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


