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
	input  wire 		aes_clk,
	input  wire			aes_rst_n,
	input  wire			axi_start_aes,
	input  wire			bram_complete,
	input  wire	[31:0]	aes_num_chunks,
	input  wire	[31:0]	aes_bram_addr_start,
  input  wire [31:0]  aes_bram_write_addr_start,
	output reg	[127:0]	aes_result_reg,
	output reg			aes_complete,
	output wire			aes_digest_valid,

	output reg			    aes_start_read,
	output reg	[31:0]	aes_bram_addr,
	input  wire	[31:0]	aes_bram_read_data,
  output reg  [31:0]  aes_bram_write_addr,
  output reg          aes_start_write,
  output reg  [31:0]  aes_bram_write_data,
  
  input  wire [255:0] aes_key_input1,
  input  wire [255:0] aes_key_input2
  
  );

// --------------------[  WIRES and REGISTERS  ]-----------------------------------
//              

	wire 			    aes_idle;
	wire  [127:0] aes_result;
	reg				    first_chunk;
	reg				    next_chunk;
	reg				    aes_data_valid;
	reg				    aes_core_rst_n;
		
	reg	  [31:0]	aes_bram_addr_nxt;
  reg   [31:0]  aes_bram_write_addr_nxt;
	reg	  [4:0]		STATE;
	reg	  [4:0]		NXT_STATE;	

	reg	  [31:0]	block_reg_core1	[0:3];
	wire  [127:0] core1_block;

  assign core1_block  	= {	block_reg_core1[0], block_reg_core1[1],
                            block_reg_core1[2], block_reg_core1[3]};
	
  localparam
			INIT		    = 0,
			AES_READ1	  = 1,
			AES_READ2	  = 2,
			AES_READ3	  = 3,
			START_AES	  = 4,
			WAIT_AES	  = 5,
			START_AES2  = 6,
			WAIT_AES2   = 7,
			LOOP_AES	  = 8,
      WAIT_AES3   = 10,
			HOLD		    = 9,
      AES_WRITE1  = 11,
      AES_WRITE2  = 12,
      AES_WRITE3  = 13;
      

	// AES State Machine
	reg	[3:0]	reg_num;
	reg	[3:0]	reg_num_nxt;
  reg [3:0] write_reg_num;
	reg [3:0] write_reg_num_nxt;
  reg	[7:0]	debug_1;

	always @( posedge aes_clk or negedge aes_rst_n) 
	begin : reg_reset
	integer i;
		if(aes_rst_n == 1'b0) begin
			block_reg_core1[0]		<= 32'h0;
			block_reg_core1[1] 		<= 32'h0;
			block_reg_core1[2]		<= 32'h0;
			block_reg_core1[3]		<= 32'h0;
			
			first_chunk			<= 1'b0;
			next_chunk			<= 1'b0;
			aes_bram_addr		<= 32'h0;
			aes_bram_write_addr <= 32'h0;
			aes_data_valid  <= 1'h0;
			reg_num_nxt			<= 4'h0;
			aes_start_read	<= 1'b0;
			aes_complete		<= 1'b0;			
			NXT_STATE			  <= INIT;			
		end else if(aes_rst_n == 1'b1) begin
			if((STATE == INIT) && (axi_start_aes == 1'b1)) begin
				aes_bram_addr 	<= aes_bram_addr_start;
				aes_bram_write_addr <= aes_bram_write_addr_start;
				aes_complete  	<= 1'b0;
				NXT_STATE		<= AES_READ1;
			end else if((STATE == INIT) && (axi_start_aes == 1'b0)) begin
				NXT_STATE <= INIT;
				aes_complete <= 1'b0;
			end else if(STATE == AES_READ1) begin
				aes_start_read <= 1'b1;
				NXT_STATE <= AES_READ2;
			end else if((STATE == AES_READ2) && (~bram_complete)) begin
				NXT_STATE <= AES_READ2;
			end else if((STATE == AES_READ2) && (bram_complete)) begin
				aes_start_read <= 1'b0;
				block_reg_core1[reg_num] <= aes_bram_read_data;
				NXT_STATE <= AES_READ3;
			end else if((STATE == AES_READ3) && (reg_num[3:0] == 4'b0011)) begin
				reg_num_nxt	<= 4'b0000;
				NXT_STATE <= START_AES;
			end else if((STATE == AES_READ3) && (reg_num[3:0] != 4'b0011)) begin
				reg_num_nxt	<= reg_num + 4'b0001;
				aes_bram_addr	<= aes_bram_addr_nxt + 32'h4;
				aes_start_read	<= 1'b1;
				NXT_STATE	<= AES_READ1;
			end else if((STATE == START_AES) && (aes_idle == 1'b1)) begin
				first_chunk <= 1'b1;
				NXT_STATE	<= WAIT_AES;
			end else if((STATE == START_AES) && (aes_idle == 1'b0)) begin
				NXT_STATE <= START_AES;
			end else if((STATE == WAIT_AES) && (aes_idle == 1'b0)) begin
				NXT_STATE <= WAIT_AES;
				first_chunk <= 1'b0;
			end else if((STATE == WAIT_AES) && (aes_idle == 1'b1)) begin
				first_chunk 	<= 1'b0;
				next_chunk		<= 1'b0;
				NXT_STATE <= START_AES2;
			end else if((STATE == START_AES2) && (aes_idle == 1'b1)) begin	
				next_chunk <= 1'b1;
				NXT_STATE <= WAIT_AES2;
		  end else if((STATE == START_AES2) && (aes_idle == 1'b0)) begin
		    NXT_STATE <= START_AES2;
		  end else if((STATE == WAIT_AES2) && (aes_idle == 1'b0)) begin
        NXT_STATE <= WAIT_AES2;
				next_chunk <= 1'b0;
		  end else if((STATE == WAIT_AES2) && (aes_idle == 1'b1)) begin
        NXT_STATE <= WAIT_AES3;
        next_chunk <= 1'b0;
      end else if((STATE == WAIT_AES3) && (aes_idle == 1'b0)) begin
        NXT_STATE <= WAIT_AES3;
      end else if((STATE == WAIT_AES3) && (aes_idle == 1'b1)) begin
        NXT_STATE <= INIT;
			  //aes_complete <= 1'b1;
        aes_result_reg <= aes_result;
      end else if(STATE == AES_WRITE1) begin
        aes_start_write <= 1'b1;
        aes_bram_write_data <= aes_result;
        NXT_STATE <= AES_WRITE2;
      end else if((STATE == AES_WRITE2) && (~bram_complete)) begin
        NXT_STATE <= AES_WRITE2;
      end else if((STATE == AES_WRITE2) && (bram_complete)) begin
        aes_start_write <= 1'b0;
        NXT_STATE <= AES_WRITE3;
      end else if((STATE == AES_WRITE3) && (write_reg_num[3:0] == 4'b0011)) begin
        NXT_STATE <= INIT;
        aes_complete <= 1'b1;
      end else if((STATE == AES_WRITE3) && (write_reg_num[3:0] != 4'b0011)) begin
        write_reg_num_nxt <= write_reg_num + 4'b0001;
        aes_bram_write_addr <= aes_bram_write_addr_nxt + 32'h4;
        aes_start_write <= 1'b1;
        NXT_STATE <= AES_WRITE1;
			end
		end
	end

	always @(negedge aes_clk or negedge aes_rst_n) begin
		if(aes_rst_n == 1'b0) begin
			reg_num <= 4'b0;
      write_reg_num <= 4'b0;
			aes_bram_addr_nxt <= aes_bram_addr;
			aes_bram_write_addr_nxt <= aes_bram_write_addr;
			STATE <= INIT;
		end else if(aes_rst_n == 1'b1) begin
			reg_num 	<= reg_num_nxt;
      write_reg_num <= write_reg_num_nxt;
			aes_bram_addr_nxt <= aes_bram_addr;
      aes_bram_write_addr_nxt <= aes_bram_write_addr;
			STATE <= NXT_STATE;
		end
	end

	always @(posedge aes_clk) begin
		aes_core_rst_n <= aes_rst_n;
	end


	aes_core aes_core(
		.clk(aes_clk),
		.reset_n(aes_core_rst_n),
		
		.encdec(1'b1),
		.init(first_chunk),
		.next(next_chunk),
		.ready(aes_idle),
		
		.key(aes_key_input1),
		.keylen(1'b1),	// 1 for 256 bit, 0 for 128
		
		.block(core1_block),
		.result(aes_result),
		.result_valid(aes_digest_valid)
	);

 
endmodule
