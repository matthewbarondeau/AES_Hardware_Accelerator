`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 11/20/2019 11:34:16 PM
// Design Name: 
// Module Name: AES_TOP
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module AES_TOP(
    input  wire         aes_clk,
    input  wire         aes_rst_n,
    input  wire         axi_start_aes,
    input  wire         bram_complete,
    input  wire [31:0]  aes_num_chunks,
    input  wire [31:0]  aes_bram_addr_start,
    input  wire [31:0]  aes_bram_write_addr_start,
    output reg  [127:0] aes_result_reg,
    output reg          aes_complete,
    output wire         aes_digest_valid,

    output reg          aes_start_read,
    output reg  [31:0]  aes_bram_addr,
    input  wire [31:0]  aes_bram_read_data,
    output reg  [31:0]  aes_bram_write_addr,
    output reg          aes_start_write,
    output reg  [31:0]  aes_bram_write_data,
  
    input  wire [255:0] aes_key_input1,
    input  wire         aes_encdec,
    output reg  [31:0]  read_bram_state,
    output reg  [31:0]  aes_process_state,
    output reg  [31:0]  aes_key_init_state,
    output reg  [31:0]  write_bram_state,
    
    output reg  [127:0] aes_result_core2_debug,
    output reg  [127:0] aes_core1_input_debug,
    output reg  [127:0] aes_core2_input_debug
  
  );

// --------------------[  WIRES and REGISTERS  ]-----------------------------------

    // Common Signals
    reg             first_chunk;
    reg   [4:0]     STATE;
    reg   [4:0]     NXT_STATE;
    reg   [255:0]   old_aes_key;
    reg             aes_core_rst_n;
    // BRAM Signals
    reg   [31:0]    aes_bram_addr_nxt;
    reg   [31:0]    aes_bram_write_addr_nxt;
    reg   [31:0]    aes_bram_addr_read;
    reg   [31:0]    aes_bram_addr_read_nxt;
    
    // Core 1 Signals
    reg   [31:0]    aes_loop_ctr;
    reg   [31:0]    aes_loop_ctr_nxt;
    reg             next_chunk;
    reg             aes_data_valid;
    reg   [31:0]    block_reg_core1 [0:7];
    reg   [31:0]    write_reg_core1 [0:7];
    wire  [127:0]   core1_block;
    wire            aes_idle;
    wire  [127:0]   aes_result;
    
    // Core 2 Signals
    reg   [31:0]    aes_loop_ctr_core2;
    reg   [31:0]    aes_loop_ctr_next_core2;
    reg             next_chunk_core2;
    reg             aes_data_valid_core2;
    reg   [31:0]    block_reg_core2 [0:3];
    reg   [31:0]    write_reg_core2 [0:3];
    wire  [127:0]   block_core2;
    wire            aes_idle_core2;
    wire  [127:0]   aes_result_core2;

    assign core1_block  = { block_reg_core1[0], block_reg_core1[1],
                            block_reg_core1[2], block_reg_core1[3]};

    assign block_core2  = { block_reg_core1[4], block_reg_core1[5],
                            block_reg_core1[6], block_reg_core1[7]};

    localparam
        INIT        = 0,
        READ1       = 1,
        READ2       = 2,
        READ3       = 3,
        START_AES1  = 4,
        WAIT_AES1   = 5,
        READ4       = 6,
        READ5       = 7,
        READ6       = 8,
        START_AES2  = 9,
        WAIT_AES2   = 10,
        WRITE1      = 11,
        WRITE2      = 12,
        WRITE3      = 13,
        WAIT_AES3   = 14,
        WRITE4      = 15,
        WRITE5      = 16,
        WRITE6      = 17;

    // AES State Machine
    reg [3:0]   reg_num;
    reg [3:0]   reg_num_nxt;
    reg [3:0]   write_reg_num;
    reg [3:0]   write_reg_num_nxt;
    reg [7:0]   debug_1;

    always @( posedge aes_clk or negedge aes_rst_n) 
    begin : reg_reset
    integer i;
        if(aes_rst_n == 1'b0) begin
            block_reg_core1[0]  <= 32'h0;
            block_reg_core1[1]  <= 32'h0;
            block_reg_core1[2]  <= 32'h0;
            block_reg_core1[3]  <= 32'h0;
            block_reg_core1[4]  <= 32'h0;
            block_reg_core1[5]  <= 32'h0;
            block_reg_core1[6]  <= 32'h0;
            block_reg_core1[7]  <= 32'h0;
            first_chunk         <= 1'b0;
            next_chunk          <= 1'b0;
            next_chunk_core2    <= 1'b0;
            aes_bram_addr       <= 32'h0;
            aes_bram_addr_read  <= 32'h0;
            aes_bram_write_addr <= 32'h0;
            aes_bram_write_data <= 32'h0;
            aes_data_valid      <= 1'h0;
            aes_data_valid_core2<= 1'h0;
            aes_loop_ctr        <= 32'h0;
            reg_num_nxt         <= 4'h0;
            write_reg_num_nxt   <= 4'b0;
            aes_start_read      <= 1'b0;
            aes_start_write     <= 1'b0;
            aes_complete        <= 1'b0;
            NXT_STATE           <= INIT;
            old_aes_key         <= 256'h0;
            read_bram_state     <= 0;
            aes_process_state   <= 0;
            aes_key_init_state  <= 0;
            write_bram_state    <= 0;
        end else if(aes_rst_n == 1'b1) begin
        
        // INIT STATE
        // RESET STATE
            if((STATE == INIT) && (axi_start_aes == 1'b1)) begin
                if(old_aes_key != aes_key_input1 || old_aes_key == 256'h0) begin
                    first_chunk <= 1'b1;
                    old_aes_key <= aes_key_input1;
                end else begin
                    first_chunk <= 1'b0;
                end
                aes_complete <= 1'b0;       // Set interrupt low
                NXT_STATE <= READ1;     // Start reading in blocks
                aes_loop_ctr <= aes_num_chunks;
                aes_bram_addr <= aes_bram_addr_start;
                aes_bram_addr_read <= aes_bram_addr_start;
                aes_bram_write_addr <= aes_bram_write_addr_start;
            end else if((STATE == INIT) && (axi_start_aes == 1'b0)) begin
                NXT_STATE <= INIT;
                aes_complete <= 1'b0;
            end 
            
        // AES_READ1
            else if(STATE == READ1) begin
                first_chunk <= 1'b0;
                aes_start_read <= 1'b1;
                NXT_STATE <= READ2;
                read_bram_state <= read_bram_state + 1;
            end 
            
        // AES_READ2
        // read value
            else if((STATE == READ2) && (~bram_complete)) begin
                NXT_STATE <= READ2;
                read_bram_state <= read_bram_state + 1;
            end else if((STATE == READ2) && (bram_complete)) begin
                aes_start_read <= 1'b0;
                NXT_STATE <= READ3;
                block_reg_core1[reg_num] <= aes_bram_read_data;
                read_bram_state <= read_bram_state + 1;
            end 
            
        // AES_READ3
        // loop condition
            else if((STATE == READ3) && (reg_num[3:0] == 4'b0011)) begin
                NXT_STATE <= START_AES1;
                read_bram_state <= read_bram_state + 1;
            end else if((STATE == READ3) && (reg_num[3:0] != 4'b0011)) begin
                reg_num_nxt	<= reg_num + 4'b0001;
                aes_bram_addr <= aes_bram_addr_nxt + 32'h4;
                aes_start_read <= 1'b1;
                NXT_STATE <= READ1;
                read_bram_state <= read_bram_state + 1;
            end 
            
        // START_AES
        // Wait for keys to finish initialization
            else if((STATE == START_AES1) && (aes_idle == 1'b1)) begin
                NXT_STATE <= WAIT_AES1;
                aes_key_init_state <= aes_key_init_state + 1;
            end else if((STATE == START_AES1) && (aes_idle == 1'b0)) begin
                NXT_STATE <= START_AES1;
                aes_key_init_state <= aes_key_init_state + 1;
            end 
            
        // WAIT_AES
        // Start core 1
            else if((STATE == WAIT_AES1) && (aes_idle == 1'b0)) begin
                NXT_STATE <= WAIT_AES1;
                aes_key_init_state <= aes_key_init_state + 1;
            end else if((STATE == WAIT_AES1) && (aes_idle == 1'b1)) begin
                next_chunk  <= 1'b1;
                aes_core1_input_debug <= core1_block;
                NXT_STATE   <= READ4;
                reg_num_nxt <= reg_num + 1;
                aes_key_init_state <= aes_key_init_state + 1;
            end 

        // READ4
        // Start BRAM
            else if((STATE == READ4)) begin
                next_chunk <= 1'b0;
                aes_start_read <= 1'b1;
                NXT_STATE <= READ5;
                read_bram_state <= read_bram_state + 1;
            end
        
        // READ5
        // read value
            else if((STATE == READ5) && (~bram_complete)) begin
                NXT_STATE <= READ5;
                read_bram_state <= read_bram_state + 1;
            end else if((STATE == READ5) && (bram_complete)) begin
                aes_start_read <= 1'b0;
                NXT_STATE <= READ6;
                block_reg_core1[reg_num] <= aes_bram_read_data;
                read_bram_state <= read_bram_state + 1;
            end
            
        // READ6
        // loop
            else if((STATE == READ6) && (reg_num[3:0] == 4'b0111)) begin
                reg_num_nxt <= 4'b0000;
                NXT_STATE <= START_AES2;
                read_bram_state <= read_bram_state + 1;
            end else if((STATE == READ6) && (reg_num[3:0] != 4'b0111)) begin
                reg_num_nxt <= reg_num + 4'b0001;
                aes_bram_addr <= aes_bram_addr_nxt + 32'h4;
                aes_start_read <= 1'b1;
                NXT_STATE <= READ4;
                read_bram_state <= read_bram_state + 1;
            end
        
        // START_AES2
        // start conversion
            else if((STATE == START_AES2) && (aes_idle_core2 == 1'b1)) begin
                next_chunk_core2 <= 1'b1;
                aes_core2_input_debug <= block_core2;
                NXT_STATE <= WAIT_AES2;
                aes_key_init_state <= aes_key_init_state + 1;
            end else if((STATE == START_AES2) && (aes_idle_core2 == 1'b0)) begin
                NXT_STATE <= START_AES2;
                aes_key_init_state <= aes_key_init_state + 1;
            end 
            
        // WAIT_AES2
        // wait for conversion to finish
            else if((STATE == WAIT_AES2) && (aes_idle == 1'b0)) begin
                NXT_STATE <= WAIT_AES2;
                next_chunk_core2 <= 1'b0;
                aes_process_state <= aes_process_state + 1;
            end else if((STATE == WAIT_AES2) && (aes_idle == 1'b1) ) begin
                NXT_STATE <= WRITE1;
                next_chunk_core2 <= 1'b0;
                aes_process_state <= aes_process_state + 1;
                aes_result_reg <= aes_result;
                aes_bram_addr <= aes_bram_write_addr;
            end 
        
        // WRITE1
        // Starts WRITE1
            else if (STATE == WRITE1) begin
                aes_start_write <= 1'b1;
                write_bram_state <= write_bram_state + 1;
                
                case(write_reg_num)
                    0: begin
                        aes_bram_write_data <= aes_result[127:96];
                        write_reg_core1[1] <= aes_result[95:64];
                        write_reg_core1[2] <= aes_result[63:32];
                        write_reg_core1[3] <= aes_result[31:0];
                    end
                    
                    1: begin
                        aes_bram_write_data <= write_reg_core1[1];
                    end
                    
                    2: begin
                        aes_bram_write_data <= write_reg_core1[2];
                    end
                    
                    3: begin
                        aes_bram_write_data <= write_reg_core1[3];
                    end
                endcase
                
                NXT_STATE <= WRITE2;
            end
        
        // WRITE2
            else if ((STATE == WRITE2) && (~bram_complete)) begin
                NXT_STATE <= WRITE2;
                write_bram_state <= write_bram_state + 1;
            end else if((STATE == WRITE2) && (bram_complete)) begin
                aes_start_write <= 1'b0;
                NXT_STATE <= WRITE3;
                write_bram_state <= write_bram_state + 1;
            end
        
        // WRITE3
            else if ((STATE == WRITE3) && (write_reg_num[3:0] == 4'b0011)) begin
                write_bram_state <= write_bram_state + 1;
                NXT_STATE <= WAIT_AES3;
            end else if((STATE == WRITE3) && (write_reg_num[3:0] != 4'b0011)) begin
                write_bram_state <= write_bram_state + 1;
                write_reg_num_nxt <= write_reg_num + 4'b0001;
                aes_bram_addr <= aes_bram_addr_nxt + 32'h4;
                NXT_STATE <= WRITE1;
            end
        
        // WAIT_AES3
        // start writing
            else if((STATE == WAIT_AES3) && (aes_idle_core2 == 1'b0)) begin
                NXT_STATE <= WAIT_AES3;
                aes_process_state <= aes_process_state + 1;
            end else if((STATE == WAIT_AES3) && (aes_idle_core2 == 1'b1)) begin
                NXT_STATE <= WRITE4;
                aes_result_core2_debug <= aes_result_core2;
                write_reg_num_nxt <= write_reg_num + 4'b0001;
                write_reg_core1[4] <= aes_result_core2[127:96];
                write_reg_core1[5] <= aes_result_core2[95:64];
                write_reg_core1[6] <= aes_result_core2[63:32];
                write_reg_core1[7] <= aes_result_core2[31:0];
                //aes_bram_addr <= aes_bram_write_addr + 32'h10;
                aes_process_state <= aes_process_state + 1;
            end 
            
        // WRITE4
            else if(STATE == WRITE4) begin
                aes_start_write <=  1'b1;
                write_bram_state <= write_bram_state + 1;
                
                case(write_reg_num)
                  4: begin
                    aes_bram_write_data <= write_reg_core1[4];
                  end
                  
                  5: begin
                    aes_bram_write_data <= write_reg_core1[5];
                  end
                  
                  6: begin
                    aes_bram_write_data <= write_reg_core1[6];
                  end
                  
                  7: begin
                    aes_bram_write_data <= write_reg_core1[7];
                  end
                endcase
                NXT_STATE <= WRITE5;
            end

        // WRITE5
            else if((STATE == WRITE5) && (~bram_complete)) begin
                NXT_STATE <= WRITE5;
                write_bram_state <= write_bram_state + 1;
            end else if((STATE == WRITE5) && (bram_complete)) begin
                aes_start_write <= 1'b0;
                NXT_STATE <= WRITE6;
                write_bram_state <= write_bram_state + 1;
            end
            
            
        // WRITE6
            else if((STATE == WRITE6) && (write_reg_num[3:0] == 4'b0111)) begin
                write_bram_state <= write_bram_state + 1;
                if(aes_loop_ctr > 32'b1) begin
                    aes_loop_ctr <= aes_loop_ctr_nxt - 32'h1;
                    aes_bram_write_addr <= aes_bram_write_addr + 32'h10; //Move 1 chunk
                    aes_bram_addr <= aes_bram_addr_read_nxt + 32'h10;  
                    aes_bram_addr_read <= aes_bram_addr_read_nxt + 32'h10;
                    write_reg_num_nxt <= 4'h0;
                    NXT_STATE <= READ1;
                end else begin
                    NXT_STATE <= INIT;
                    aes_complete <= 1'b1;
                end
            end else if((STATE == WRITE6) && (write_reg_num[3:0] != 4'b0111)) begin
                write_bram_state <= write_bram_state + 1;
                write_reg_num_nxt <= write_reg_num + 4'b0001;
                aes_bram_addr <= aes_bram_addr_nxt + 32'h4;
                NXT_STATE <= WRITE4;
            end
        end
    end

    always @(negedge aes_clk or negedge aes_rst_n) begin
        if(aes_rst_n == 1'b0) begin
            reg_num <= 4'b0;
            write_reg_num <= 4'b0;
            aes_bram_addr_nxt <= aes_bram_addr;
            aes_bram_write_addr_nxt <= aes_bram_write_addr;
            aes_bram_addr_read_nxt <= aes_bram_addr_start;
            aes_loop_ctr_nxt <= 0;
            STATE <= INIT;
        end else if(aes_rst_n == 1'b1) begin
            reg_num <= reg_num_nxt;
            write_reg_num <= write_reg_num_nxt;
            aes_bram_addr_nxt <= aes_bram_addr;
            aes_bram_write_addr_nxt <= aes_bram_write_addr;
            aes_bram_addr_read_nxt <= aes_bram_addr_read;
            aes_loop_ctr_nxt <= aes_loop_ctr;
            STATE <= NXT_STATE;
        end
    end

    always @(posedge aes_clk) begin
        aes_core_rst_n <= aes_rst_n;
    end

    aes_core aes_core1(
        .clk(aes_clk),
        .reset_n(aes_core_rst_n),

        .encdec(aes_encdec),
        .init(first_chunk),
        .next(next_chunk),
        .ready(aes_idle),

        .key(aes_key_input1),
        .keylen(1'b1), // 1 for 256 bit, 0 for 128

        .block(core1_block),
        .result(aes_result),
        .result_valid(aes_digest_valid)
    );

    aes_core aes_core2(
        .clk(aes_clk),
        .reset_n(aes_core_rst_n),

        .encdec(aes_encdec),
        .init(first_chunk),
        .next(next_chunk_core2),
        .ready(aes_idle_core2),

        .key(aes_key_input1),
        .keylen(1'b1), // 1 for 256 bit, 0 for 128

        .block(block_core2),
        .result(aes_result_core2),
        .result_valid(aes_digest_valid_core2)
    );

 
endmodule
