# AES_Hardware_Accelerator

## Overview

An implementation of AES encryption using hardware acceleration on the Zedboard. Using the AES implementation from [secworks](https://github.com/secworks/aes). Final project for the University of Texas at Austin graduate course EE382N-4 Fall 2019.

### Block Diagram
![Image of block diagram](https://github.com/matthewbarondeau/AES_Hardware_Accelerator/blob/master/images/382N_final_block_diagram.png)

### Address Map
![Image of Memory Mapping](https://github.com/matthewbarondeau/AES_Hardware_Accelerator/blob/master/images/382N_final_mapping.png)

### Register map
```
slv_reg0[31:0]  
slv_reg1[31:0] = {30'b0, aes_complete, aes_digest_valid}  
slv_reg2[31:0] = axi_bram_addr  
slv_reg3[31:0] = axi_bram_read_data  
slv_reg4[31:0] = axi_bram_write_data  
slv_reg5[31:0] = aes_num_chunks       (128 bit chunks)  
slv_reg6[31:0] = aes_bram_addr_start  (starting address in bram  
slv_reg7[31:0] = 0xfeedbeef  
slv_reg8[31:0] = aes_digest_reg[127:96]  
slv_reg9[31:0] = aes_digest_reg[95:64]  
slv_regA[31:0] = aes_digest_reg[63:32]  
slv_regB[31:0] = aes_digest_reg[31:0]  
```

### BRAM order
```
key 	= {block_reg[0], block_reg[1], block_reg[2], block_reg[3],  
         block_reg[4], block_reg[5], block_reg[6], block_reg[7]};  

chunk = {block_reg[8], block_reg[9],  
         block_reg[10], block_reg[11]};
```
