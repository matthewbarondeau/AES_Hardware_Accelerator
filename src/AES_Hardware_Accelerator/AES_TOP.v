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
	output reg	[127:0]	aes_result_reg,
	output reg			aes_complete,
	output wire			aes_digest_valid,

	output reg			aes_start_read,
	output reg	[31:0]	aes_bram_addr,
	input  wire	[31:0]	aes_bram_read_data
    );


// --------------------[  WIRES and REGISTERS  ]-----------------------------------
//              

	wire 			aes_idle;
	wire [127:0]	aes_result;
	reg				first_chunk;
	reg				next_chunk;
	reg				next_chunk_pending;
	reg				aes_data_valid;
	reg				aes_core_rst_n;
	
	reg	[31:0]		aes_chunk_ctr;
	reg	[31:0]		aes_chunk_ctr_nxt;
	
	reg	[31:0]		aes_bram_addr_nxt;
	reg	[4:0]		STATE;
	reg	[4:0]		NXT_STATE;	

	reg	[31:0]	block_reg	[0:11];
	//reg	[31:0]	key_reg		[0:7];
	wire[255:0] core_key;
	wire[127:0] core_block;

  	assign core_key 	= {	block_reg[0], block_reg[1], block_reg[2], block_reg[3],
                     		block_reg[4], block_reg[5], block_reg[6], block_reg[7]};


  	assign core_block  	= {	block_reg[8], block_reg[9],
                        	block_reg[10], block_reg[11]};

	localparam
			INIT		= 8,
			AES_READ1	= 1,
			AES_READ2	= 2,
			AES_READ3	= 3,
			START_AES	= 4,
			WAIT_AES	= 5,
			START_AES2  = 6,
			WAIT_AES2   = 7,
			LOOP_AES	= 8,
			HOLD		= 9;


	// AES State Machine
	reg	[3:0]	reg_num;
	reg	[3:0]	reg_num_nxt;
	reg	[7:0]	debug_1;

	always @( posedge aes_clk or negedge aes_rst_n) 
	begin : reg_reset
	integer i;
		if(aes_rst_n == 1'b0) begin
			block_reg[0]		<= 32'h0;
			block_reg[1] 		<= 32'h0;
			block_reg[2]		<= 32'h0;
			block_reg[3]		<= 32'h0;
			block_reg[4]		<= 32'h0;
			block_reg[5]		<= 32'h0;
			block_reg[6]		<= 32'h0;
			block_reg[7]		<= 32'h0;
			block_reg[8]		<= 32'h0;
			block_reg[9]		<= 32'h0;
			block_reg[10]		<= 32'h0;
			block_reg[11]		<= 32'h0;
			
			first_chunk			<= 1'b1;
			next_chunk			<= 1'b0;
			next_chunk_pending	<= 1'b0;
			aes_chunk_ctr		<= 32'h0;
			aes_bram_addr		<= 32'h0;
			aes_data_valid		<= 1'h0;
			reg_num_nxt			<= 4'h0;
			aes_start_read		<= 1'b0;
			aes_complete		<= 1'b0;			
			NXT_STATE			<= INIT;			

		end else if(aes_rst_n == 1'b1) begin
			if((STATE == INIT) && (axi_start_aes == 1'b1)) begin
				aes_bram_addr 	<= aes_bram_addr_start;
				aes_chunk_ctr 	<= aes_num_chunks;
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
				block_reg[reg_num] <= aes_bram_read_data;
				NXT_STATE <= AES_READ3;
			end else if((STATE == AES_READ3) && (reg_num[3:0] == 4'b1011)) begin
				reg_num_nxt	<= 4'b0000;
				NXT_STATE <= START_AES;
			end else if((STATE == AES_READ3) && (reg_num[3:0] != 4'b1011)) begin
				reg_num_nxt	<= reg_num + 4'b0001;
				aes_bram_addr	<= aes_bram_addr_nxt + 32'h4;
				aes_start_read	<= 1'b1;
				NXT_STATE	<= AES_READ1;
			end else if((STATE == START_AES) && (aes_idle == 1'b1)) begin
				if(next_chunk_pending == 1'b0) begin
					first_chunk <= 1'b1;
					//NXT_STATE   <= WAIT_AES;
				end else if(next_chunk_pending == 1'b1)
					first_chunk <= 1'b0;
					//next_chunk 	<= 1'b1;
					NXT_STATE	<= WAIT_AES;
			end else if((STATE == START_AES) && (aes_idle == 1'b0)) begin
				NXT_STATE <= START_AES;
			end else if((STATE == WAIT_AES) && (aes_idle == 1'b0)) begin
				NXT_STATE <= WAIT_AES;
			end else if((STATE == WAIT_AES) && (aes_idle == 1'b1)) begin
				first_chunk 	<= 1'b0;
				next_chunk		<= 1'b0;
				//aes_result_reg 	<= aes_result;
				NXT_STATE <= START_AES2;
			end else if((STATE == START_AES2) &&(aes_idle == 1'b1)) begin	
				next_chunk <= 1'b1;
				NXT_STATE <= WAIT_AES2;
		    end else if((STATE == START_AES2) && (aes_idle == 1'b0)) begin
		        NXT_STATE <= START_AES2;
		    end else if((STATE == WAIT_AES2) && (aes_idle == 1'b0)) begin
                NXT_STATE <= WAIT_AES2;
		    end else if((STATE == WAIT_AES2) && (aes_idle == 1'b1)) begin
		        next_chunk <= 1'b0;
                NXT_STATE <= LOOP_AES;
                aes_result_reg <= aes_result;
			end else if((STATE == LOOP_AES) && (aes_chunk_ctr > 32'b1)) begin
				first_chunk <= 1'b0;
				next_chunk 	<= 1'b1;
				aes_chunk_ctr	<= aes_chunk_ctr_nxt - 32'h1;
				aes_bram_addr	<= aes_bram_addr_nxt + 32'h4;
				next_chunk_pending <= 1'b1;
				NXT_STATE 	<= AES_READ1;
			end else if((STATE == LOOP_AES) && (aes_chunk_ctr == 32'b1)) begin
				first_chunk 	<= 1'b0;
				next_chunk		<= 1'b0;
				next_chunk_pending <= 1'b0;
				aes_complete <= 1'b1;
				NXT_STATE <= INIT;
			end
		end
	end

	always @(negedge aes_clk or negedge aes_rst_n) begin
		if(aes_rst_n == 1'b0) begin
			reg_num <= 4'b0;
			aes_bram_addr_nxt <= aes_bram_addr;
			aes_chunk_ctr_nxt <= aes_chunk_ctr;
			STATE <= INIT;
		end else if(aes_rst_n == 1'b1) begin
			reg_num 	<= reg_num_nxt;
			aes_chunk_ctr_nxt <= aes_chunk_ctr;
			aes_bram_addr_nxt <= aes_bram_addr;
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
		
		.key(core_key),
		.keylen(1'b1),	// 1 for 256 bit, 0 for 128
		
		.block(core_block),
		.result(aes_result),
		.result_valid(aes_digest_valid)
	);

 
endmodule
