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
    output reg  [31:0]  write_bram_state

  );

// --------------------[  WIRES and REGISTERS  ]-----------------------------------

    // Common Signals
    reg             first_chunk;
    reg   [5:0]     STATE;
    reg   [5:0]     NXT_STATE;
    reg   [255:0]   old_aes_key;
    reg             aes_core_rst_n;
    reg   [31:0]    aes_loop_ctr;
    reg   [31:0]    aes_loop_ctr_nxt;
    reg   [31:0]    block_reg_core1 [0:15];
    reg   [31:0]    write_reg_core1 [0:15];

    // BRAM Signals
    reg   [31:0]    aes_bram_addr_nxt;
    reg   [31:0]    aes_bram_write_addr_nxt;
    reg   [31:0]    aes_bram_addr_read;
    reg   [31:0]    aes_bram_addr_read_nxt;

    // Core 1 Signals
    wire  [127:0]   block_core1;
    wire            aes_idle_core1;
    reg             next_chunk_core1;
    wire  [127:0]   aes_result_core1;
    reg             aes_data_valid_core1;

    // Core 2 Signals
    wire  [127:0]   block_core2;
    wire            aes_idle_core2;
    reg             next_chunk_core2;
    wire  [127:0]   aes_result_core2;
    reg             aes_data_valid_core2;

    // Core 3 Signals
    wire  [127:0]   block_core3;
    wire            aes_idle_core3;
    reg             next_chunk_core3;
    wire  [127:0]   aes_result_core3;
    reg             aes_data_valid_core3;

    // Core 4 Signals
    wire  [127:0]   block_core4;
    wire            aes_idle_core4;
    reg             next_chunk_core4;
    wire  [127:0]   aes_result_core4;
    reg             aes_data_valid_core4;

    assign block_core1  = { block_reg_core1[0], block_reg_core1[1],
                            block_reg_core1[2], block_reg_core1[3]};

    assign block_core2  = { block_reg_core1[4], block_reg_core1[5],
                            block_reg_core1[6], block_reg_core1[7]};
                            
    assign block_core3  = { block_reg_core1[8], block_reg_core1[9],
                            block_reg_core1[10], block_reg_core1[11]};
                            
    assign block_core4  = { block_reg_core1[12], block_reg_core1[13],
                            block_reg_core1[14], block_reg_core1[15]};

    localparam
        INIT        = 0,
        READ1       = 1,
        READ2       = 2,
        READ3       = 3,
        WAIT_AES1   = 4,
        START_AES1  = 5,
        CHECK_AES1  = 6,
        WRITE1      = 7,
        WRITE2`     = 8,
        WRITE3      = 9;

    // AES State Machine
    reg [3:0]   reg_num;
    reg [3:0]   reg_num_nxt;
    reg [3:0]   write_reg_num;
    reg [3:0]   write_reg_num_nxt;
    reg [7:0]   debug_1;

    always @( posedge aes_clk) 
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
            block_reg_core1[8]  <= 32'h0;
            block_reg_core1[9]  <= 32'h0;
            block_reg_core1[10] <= 32'h0;
            block_reg_core1[11] <= 32'h0;
            block_reg_core1[12] <= 32'h0;
            block_reg_core1[13] <= 32'h0;
            block_reg_core1[14] <= 32'h0;
            block_reg_core1[15] <= 32'h0;
            first_chunk         <= 1'b0;
            next_chunk_core1    <= 1'b0;
            next_chunk_core2    <= 1'b0;
            next_chunk_core3    <= 1'b0;
            next_chunk_core4    <= 1'b0;
            aes_bram_addr_read  <= 32'h0;
            aes_bram_write_addr <= 32'h0;
            aes_bram_write_data <= 32'h0;
            aes_data_valid_core1<= 1'h0;
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

        // INIT
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

        // READ1
        // Start Read from BRAM
            else if(STATE == READ1) begin
                first_chunk <= 1'b0;
                aes_start_read <= 1'b1;
                NXT_STATE <= READ2;
                read_bram_state <= read_bram_state + 1;
            end 

        // READ2
        // Wait for BRAM read to complete
            else if((STATE == READ2) && (~bram_complete)) begin
                NXT_STATE <= READ2;
                read_bram_state <= read_bram_state + 1;
            end else if((STATE == READ2) && (bram_complete)) begin
                aes_start_read <= 1'b0;
                NXT_STATE <= READ3;
                block_reg_core1[reg_num] <= aes_bram_read_data;
                read_bram_state <= read_bram_state + 1;
            end 

        // READ3
        // Repeat READ1 and READ2 until block for core 1 read in
            else if((STATE == READ3) && (reg_num[3:0] == 4'b1111)) begin
                NXT_STATE <= WAIT_AES1;
                read_bram_state <= read_bram_state + 1;
            end else if((STATE == READ3) && (reg_num[3:0] != 4'b1111)) begin
                reg_num_nxt	<= reg_num + 4'b0001;
                aes_bram_addr <= aes_bram_addr_nxt + 32'h4;
                aes_start_read <= 1'b1;
                NXT_STATE <= READ1;
                read_bram_state <= read_bram_state + 1;
            end 

        // WAIT_AES1
        // Wait for AES_CORES to load in key
            else if((STATE == WAIT_AES1) && (aes_idle_core1 == 1'b1)) begin
                NXT_STATE <= START_AES1;
                aes_key_init_state <= aes_key_init_state + 1;
            end else if((STATE == WAIT_AES1) && (aes_idle_core1 == 1'b0)) begin
                NXT_STATE <= WAIT_AES1;
                aes_key_init_state <= aes_key_init_state + 1;
            end 

        // START_AES1
        // Start AES_CORE1
            else if((STATE == START_AES1) && (aes_idle_core1 == 1'b0)) begin
                NXT_STATE <= START_AES1;
                aes_key_init_state <= aes_key_init_state + 1;
            end else if((STATE == START_AES1) && (aes_idle_core1 == 1'b1)) begin
                next_chunk_core1 <= 1'b1;
                next_chunk_core2 <= 1'b1;
                next_chunk_core3 <= 1'b1;
                next_chunk_core4 <= 1'b1;
                NXT_STATE   <= CHECK_AES1;
                reg_num_nxt <= reg_num + 1;
                aes_key_init_state <= aes_key_init_state + 1;
            end 

        // CHECK_AES1
        // Wait until AES_CORE1 completes encryption
            else if((STATE == CHECK_AES1) && (~aes_idle_core1 || ~aes_idle_core2 || ~aes_idle_core3 || ~aes_idle_core4)) begin
                NXT_STATE <= CHECK_AES1;
                next_chunk_core4 <= 1'b0;
                aes_process_state <= aes_process_state + 1;
            end else if((STATE == CHECK_AES1) && (aes_idle_core1 && aes_idle_core2 
                                              &&  aes_idle_core3 && aes_idle_core4)) begin
                NXT_STATE <= WRITE1;
                next_chunk_core4 <= 1'b0;
                aes_process_state <= aes_process_state + 1;
                aes_bram_addr <= aes_bram_write_addr;
                write_reg_core1[0] <= aes_result_core1[127:96];
                write_reg_core1[1] <= aes_result_core1[95:64];
                write_reg_core1[2] <= aes_result_core1[63:32];
                write_reg_core1[3] <= aes_result_core1[31:0];
                write_reg_core1[4] <= aes_result_core2[127:96];
                write_reg_core1[5] <= aes_result_core2[95:64];
                write_reg_core1[6] <= aes_result_core2[63:32];
                write_reg_core1[7] <= aes_result_core2[31:0];
                write_reg_core1[8] <= aes_result_core3[127:96];
                write_reg_core1[9] <= aes_result_core3[95:64];
                write_reg_core1[10] <= aes_result_core3[63:32];
                write_reg_core1[11] <= aes_result_core3[31:0];
                write_reg_core1[12] <= aes_result_core4[127:96];
                write_reg_core1[13] <= aes_result_core4[95:64];
                write_reg_core1[14] <= aes_result_core4[63:32];
                write_reg_core1[15] <= aes_result_core4[31:0];
            end

        // WRITE1
        // Select which word to write to BRAM
            else if (STATE == WRITE1) begin
                aes_start_write <= 1'b1;
                write_bram_state <= write_bram_state + 1;
                
                case(write_reg_num)
                    0: begin
                        aes_bram_write_data <= write_reg_core1[0];
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
                    
                    8: begin
                        aes_bram_write_data <= write_reg_core1[8];
                    end
                    
                    9: begin
                        aes_bram_write_data <= write_reg_core1[9];
                    end
                    
                    10: begin
                        aes_bram_write_data <= write_reg_core1[10];
                    end
                    
                    11: begin
                        aes_bram_write_data <= write_reg_core1[11];
                    end

                    12: begin
                        aes_bram_write_data <= write_reg_core1[12];
                    end
                    
                    13: begin
                        aes_bram_write_data <= write_reg_core1[13];
                    end
                    
                    14: begin
                        aes_bram_write_data <= write_reg_core1[14];
                    end
                    
                    15: begin
                        aes_bram_write_data <= write_reg_core1[15];
                    end
                    
                endcase
                NXT_STATE <= WRITE2;
            end

        // WRITE2
        // Wait for BRAM write to complete
            else if ((STATE == WRITE2) && (~bram_complete)) begin
                NXT_STATE <= WRITE2;
                write_bram_state <= write_bram_state + 1;
            end else if((STATE == WRITE2) && (bram_complete)) begin
                aes_start_write <= 1'b0;
                NXT_STATE <= WRITE3;
                write_bram_state <= write_bram_state + 1;
            end

        // WRITE12
            else if ((STATE == WRITE3) && (write_reg_num[3:0] == 4'b1111)) begin
                write_bram_state <= write_bram_state + 1;
                if(aes_loop_ctr > 32'b1) begin
                    aes_loop_ctr <= aes_loop_ctr_nxt - 32'h1;
                    aes_bram_write_addr <= aes_bram_write_addr + 32'h10;
                    aes_bram_addr <= aes_bram_addr_read_nxt + 32'h10;  
                    aes_bram_addr_read <= aes_bram_addr_read_nxt + 32'h10;
                    write_reg_num_nxt <= 4'h0;
                    NXT_STATE <= READ1;
                end else begin
                    NXT_STATE <= INIT;
                    aes_complete <= 1'b1;
                end
            end else if((STATE == WRITE3) && (write_reg_num[3:0] != 4'b1111)) begin
                write_bram_state <= write_bram_state + 1;
                write_reg_num_nxt <= write_reg_num + 4'b0001;
                aes_bram_addr <= aes_bram_addr_nxt + 32'h4;
                NXT_STATE <= WRITE1;
            end
        end
    end

    // Update signals on negedge to avoid race condition
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

    // Delay core reset by one edge to prevent race
    always @(posedge aes_clk) begin
        aes_core_rst_n <= aes_rst_n;
    end

    // Instantiate AES_CORES
    aes_core aes_core1(
        .clk(aes_clk),
        .reset_n(aes_core_rst_n),

        .encdec(aes_encdec),
        .init(first_chunk),
        .next(next_chunk_core1),
        .ready(aes_idle_core1),

        .key(aes_key_input1),
        .keylen(1'b1),

        .block(block_core1),
        .result(aes_result_core1),
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
        .keylen(1'b1),

        .block(block_core2),
        .result(aes_result_core2),
        .result_valid(aes_digest_valid_core2)
    );

    aes_core aes_core3(
        .clk(aes_clk),
        .reset_n(aes_core_rst_n),

        .encdec(aes_encdec),
        .init(first_chunk),
        .next(next_chunk_core3),
        .ready(aes_idle_core3),

        .key(aes_key_input1),
        .keylen(1'b1),

        .block(block_core3),
        .result(aes_result_core3),
        .result_valid(aes_digest_valid_core3)
    );

    aes_core aes_core4(
        .clk(aes_clk),
        .reset_n(aes_core_rst_n),

        .encdec(aes_encdec),
        .init(first_chunk),
        .next(next_chunk_core4),
        .ready(aes_idle_core4),

        .key(aes_key_input1),
        .keylen(1'b1),

        .block(block_core4),
        .result(aes_result_core4),
        .result_valid(aes_digest_valid_core4)
    );
 
endmodule

