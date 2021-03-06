# AES_Hardware_Accelerator

## Overview

An implementation of AES encryption using hardware acceleration on the Zedboard. Using the AES implementation from [secworks](https://github.com/secworks/aes). Final project for the University of Texas at Austin graduate course EE382N-4 Fall 2019.

### Block Diagram
![Image of block diagram](https://github.com/matthewbarondeau/AES_Hardware_Accelerator/blob/master/images/HW_BLOCK_DIAGRAM.png)

### Address Map
![Image of Memory Mapping](https://github.com/matthewbarondeau/AES_Hardware_Accelerator/blob/master/images/ADDRESS_MAP.png)

### Register map
```
slv_reg0[31:0] = {329'b0, encdec, 0'b0, reset_n, start_axi}  
slv_reg1[31:0] = {29'b0, aes_bus_control, aes_complete, aes_digest_valid}  
slv_reg2[31:0] = axi_bram_addr  
slv_reg3[31:0] = axi_bram_read_data  
slv_reg4[31:0] = axi_bram_write_data  
slv_reg5[31:0] = aes_num_chunks       (128 bit chunks)  
slv_reg6[31:0] = aes_bram_addr_start  (starting address in bram  
slv_reg7[31:0] = 0xfeedbeef  
slv_reg8[31:0] = slv_reg8
slv_reg9[31:0] = slv_reg9  
slv_regA[31:0] = slv_reg10
slv_regB[31:0] = slv_reg11
slv_regC[31:0] = 32'hbeefface  
slv_regD[31:0] = aes_key_core1[255:224]  
slv_regE[31:0] = aes_key_core1[223:192]  
slv_regF[31:0]=  aes_key_core1[191:160]  
slv_reg10[31:0]= aes_key_core1[159:128]  
slv_reg11[31:0]= aes_key_core1[127:96]  
slv_reg12[31:0]= aes_key_core1[95:64]  
slv_reg13[31:0]= aes_key_core1[63:32]  
slv_reg14[31:0]= aes_key_core1[31:0]  
slv_reg15[31:0]= read_bram_state 
slv_reg16[31:0]= aes_process_state
slv_reg17[31:0]= aes_key_init_state
slv_reg18[31:0]= write_bram_state
slv_reg19[31:0]= slv_reg25  
slv_reg1A[31:0]= slv_reg26  
slv_reg1B[31:0]= slv_reg27  
slv_reg1C[31:0]= slv_reg28  
slv_reg1D[31:0]= aes_bram_write_addr_start  
```

### Key order
```
key   = {slv_reg13, slv_reg14, slv_reg15, slv_reg16,  
         slv_reg18, slv_reg18, slv_reg19, slv_reg20}  
```

### Software
The main software interface is `aes_test.c`. The software program can be compiled using the Makefile given, and instruction on how to use the program can be accessed using `--help`. Note: Encryption and decryption for CTR mode is the same, so for decrypting ctr mode data you need to specify `enc` instead of `dec`. Also, the maximum input size for the encryption/decryption file is 128kB. This is due to the use of OCM for the cdma, which is 256kB and is split into 2. Bigger sizes could be implemented but due to memory overhead would not be advantageous. Memory could be made for efficient through use of the AXI Stream interface, and DMA straight from DDR. Another way to optimize memory would be through ACP.

The kernel module for the interrupt is in the .\software\interrupts folder. It can be compiled using the Makefile in the program, and inserted using the setup script in the folder (ie. `sh setup.sh`).

### Testing
In the software folder there is a test_time.sh. This is a bash script that will measure the time of the hardware accelerated program with the openssl software version with data chunks from 1 to 100000 bytes. Timing will be the same for ecb and ctr mode, ecb was chosen arbitrarily.

Furthermore our program can be directly compared against the openssl version and match (with padding!). Our program works with `-aes-256-ecb` and `-aes-256-ctr`. Note however that for openssl you must specify the key/iv values in hex, whereas the `test_aes` program takes `char`s as input for the key and iv values.
