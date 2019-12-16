`timescale 1ns / 1ps

//////////////////////////////////////////////////////////////////////////////////
//
// Create Date:     06/21/2019 
// Design Name: 
// Module Name:     BRAM_IF
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: This module is the interface state machine to the BRAM block.
// 
// Dependencies: Zedboard only
// 
// Revision:
// Revision 0.01 - File Created
// 
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module BRAM_IF(

 // AXI I/F
    input   wire            axi_start_read,         // Start AXI read tansaction
    input   wire            axi_start_write,        // Start AXI write transaction
    input   wire            axi_clk,
    input   wire            axi_rst,
    
    input   wire [31:0]     axi_bram_addr,          // BRAM Address from the AXI unit
    input   wire [31:0]     axi_bram_write_data,    // AXI write data to the BRAM
    output  reg  [31:0]     axi_bram_read_data,     // BRAM read data to the AXI unit
 
 // SHA I/F
    input   wire [31:0]     sha_bram_addr,          // BRAM Address from the SHA unit
    output  reg  [31:0]     sha_bram_read_data,     // BRAM read data to the SHA unit   
    input   wire            sha_start_read,         // Start SHA read transaction
    input   wire [31:0]     sha_bram_write_data,
    input   wire            sha_start_write,
    
    output  reg             bram_complete,          // BRAM transaction completed
    
 // BRAM I/F 
    output  reg [31:0]      addr_BRAM,              // Address to the BRAM
    output  wire            clk_BRAM,               // CLOCK to the BRAM
    output  reg [31:0]      dout_BRAM,              // NOTE: This connects to DIN on the BRAM
    input   wire [31:0]     din_BRAM,               // NOTE: This connects to DOUT on the BRAM
    output  reg             en_BRAM,                // Enable BRAM
    output  wire            rst_BRAM,               // Reset to the BRAM
    output  reg [3:0]       we_BRAM                 // Write Enable to BRAM
    
    );
    
    // BIU State machine STATE mapping:
    localparam  
            IDLE        = 0,
            READ1       = 1,
            READ2       = 2,
            READ3       = 3,        // Not used yet
            WRITE1      = 4,
            WRITE2      = 5,
            WRITE3      = 6,       
            HOLD        = 7,
            SHA_READ1   = 8,
            SHA_READ2   = 9,
            SHA_READ3   = 10,       // Not used yet
            SHA_WRITE1  = 11,
            SHA_WRITE2  = 12,
            SHA_WRITE3  = 13;
    
    // ---------- ASSIGNMENTS ------------------------------------
    
    assign rst_BRAM     =   ~axi_rst;           // Complement AXI RESET to BRAM
    assign clk_BRAM     =   axi_clk;            // Forward AXI_CLK
    
    
    // ---------------------------------------------------------------------------
    // --------------------------- BIU STATE MACHINE -----------------------------
    // ---------------------------------------------------------------------------       
  
    reg [3:0]       STATE;
    reg [3:0]       NXT_STATE;
    
 
   
    always @(negedge axi_clk) begin                 // Use Neg edge of clock to generate control signals to the BRAM
        
        if ( rst_BRAM  == 1'b1 ) begin              // In reset
        
	            addr_BRAM       <= 32'b0;
                dout_BRAM       <= 32'b0;                
                we_BRAM         <=  4'b0;
                en_BRAM         <=  1'b0;
                bram_complete   <=  1'b0;
                NXT_STATE       <=  IDLE;           // Start in the IDLE STATE waiting for Start 
         end 
	
	
	        
	    else if ( rst_BRAM == 1'b0 ) begin         // Not in reset

    // ------------IDLE STATE  ----------------------------------------------------	    

            if ((STATE == IDLE) && 
                (sha_start_read== 1'b1)) 
            begin
                
                we_BRAM             <= 4'b0;
                en_BRAM             <= 1'b0;
                addr_BRAM           <= sha_bram_addr[31:0];     // Assert address
                dout_BRAM           <= 32'b0;           
                bram_complete       <=  1'b0;
                NXT_STATE           <= SHA_READ1;               // Next State = SHA READ
            end
                
            else if ((STATE == IDLE) && 
                (axi_start_read== 1'b1)) 
            begin
                 
                we_BRAM             <= 4'b0;
                en_BRAM             <= 1'b0;
                addr_BRAM           <= axi_bram_addr[31:0];     // Assert address
                dout_BRAM           <= 32'b0;           
                bram_complete       <=  1'b0;
                NXT_STATE           <= READ1;                   // Next State = READ
            end
                             
            else if ((STATE == IDLE) && 
                     (axi_start_write== 1'b1)) 
            begin                   
                                        
                we_BRAM             <= 4'b0000;
                en_BRAM             <= 1'b0;
                addr_BRAM           <= axi_bram_addr[31:0];      // Assert address
                dout_BRAM           <= axi_bram_write_data;      // Assert write Data          
                NXT_STATE           <= WRITE1;                   // Next State = Write    
            end  
            
            else if ((STATE == IDLE) &&
                     (sha_start_write == 1'b1))
            begin
                we_BRAM             <= 4'b0000;
                en_BRAM             <= 1'b0;
                addr_BRAM           <= sha_bram_addr[31:0];      // Assert address
                dout_BRAM           <= sha_bram_write_data;      // Assert write Data          
                NXT_STATE           <= SHA_WRITE1;               // Next State = Write    
            end

            else if ((STATE == IDLE) && 
                     (~axi_start_write== 1'b1) && 
                     (~axi_start_read == 1'b1) &&
                     (~sha_start_read == 1'b1) &&
                     (~sha_start_write== 1'b1))
            begin        
                NXT_STATE           <= IDLE;                    // Stay in IDLE state
            end
            
    // ------------  READ1 STATE  -------------------------------------------------
    //            
            else if(STATE == READ1) 
            begin
                en_BRAM             <= 1'b1;                   // Assert Enable
                we_BRAM             <= 4'b0000;                // Negate write         
                addr_BRAM           <= axi_bram_addr[31:0];    // Assert address;   
                NXT_STATE           <= READ2;
            end
                              
    // ------------   READ2 STATE  ------------------------------------------------- 
    //                 
            else if (STATE == READ2) 
            begin
                en_BRAM             <= 1'b1;                    // Assert enable
                we_BRAM             <= 4'b0000;                 // Negate write                                        
                axi_bram_read_data  <= din_BRAM;                // Read data from BRAM 
                bram_complete       <= 1'b1;
                NXT_STATE           <= HOLD;                    // Go to HOLD and then back to IDLE
            end       
                            
    // ------------   WRITE1 STATE  ---------------------------------------------- 
    //                   
            else if (STATE == WRITE1) 
            begin 
                en_BRAM             <= 1'b1;                    // Assert enable
                we_BRAM             <= 4'b0000;                 // Assert Write Enable           
                dout_BRAM           <= axi_bram_write_data;     // Assert Write Data
                addr_BRAM           <= axi_bram_addr[31:0];     // Assert address;                
                NXT_STATE           <= WRITE2;               
            end

    // ------------   WRITE2 STATE  -----------------------------------------------  
    //         
            else if(STATE == WRITE2)   
            begin      
                en_BRAM             <= 1'b1;
                we_BRAM             <= 4'b1111;
                dout_BRAM           <= axi_bram_write_data;     // Assert Write Data
                addr_BRAM           <= axi_bram_addr[31:0];     // Assert address; 
                NXT_STATE           <= WRITE3;                  
            end
                    
    // ------------   WRITE3 STATE  -----------------------------------------------  
    //         
            else if(STATE == WRITE3)   
            begin      
                en_BRAM             <= 1'b0;
                we_BRAM             <= 4'b1111;
                dout_BRAM           <= axi_bram_write_data;     // Assert Write Data
                addr_BRAM           <= axi_bram_addr[31:0];     // Assert address; 
                NXT_STATE           <= READ1;                   // Go to READ1 state to read written
            end


    // ------------  SHA READ1 STATE  -------------------------------------------------
    //            
            else if(STATE == SHA_READ1) 
            begin
                en_BRAM             <= 1'b1;                    // Assert Enable
                we_BRAM             <= 4'b0000;                 // Negate write         
                addr_BRAM           <= sha_bram_addr[31:0];     // Assert address;   
                NXT_STATE           <= SHA_READ2;
            end
                              
    // ------------   SHA_READ2 STATE  ------------------------------------------------- 
    //                 
            else if (STATE == SHA_READ2) 
            begin
                en_BRAM             <= 1'b1;                    // Assert enable
                we_BRAM             <= 4'b0000;                 // Negate write                                        
                sha_bram_read_data  <= din_BRAM;                // Read data from BRAM 
                //sha_bram_read_data  <= 32'hDEADDEAD;            // Read data from BRAM
                bram_complete       <= 1'b0;
                NXT_STATE           <= SHA_READ3;                    // Go to HOLD and then back to IDLE
            end       
         
                             
    // ------------   SHA_READ3 STATE  ------------------------------------------------- 
    //                 
            else if (STATE == SHA_READ3) 
            begin
                en_BRAM             <= 1'b1;                    // Assert enable
                we_BRAM             <= 4'b0000;                 // Negate write                                        
                sha_bram_read_data  <= din_BRAM;                // Read data from BRAM 
                //sha_bram_read_data  <= 32'hDEADDEAD;            // Read data from BRAM
                bram_complete       <= 1'b1;
                NXT_STATE           <= HOLD;                    // Go to HOLD and then back to IDLE
            end       

    // ------------   SHA_WRITE1 STATE  ---------------------------------------------- 
    //                   
            else if (STATE == SHA_WRITE1) 
            begin 
                en_BRAM             <= 1'b1;                    // Assert enable
                we_BRAM             <= 4'b0000;                 // Assert Write Enable           
                dout_BRAM           <= sha_bram_write_data;     // Assert Write Data
                addr_BRAM           <= sha_bram_addr[31:0];     // Assert address;                
                NXT_STATE           <= SHA_WRITE2;               
            end

    // ------------   SHA_WRITE2 STATE  -----------------------------------------------  
    //         
            else if(STATE == SHA_WRITE2)   
            begin      
                en_BRAM             <= 1'b1;
                we_BRAM             <= 4'b1111;
                dout_BRAM           <= sha_bram_write_data;     // Assert Write Data
                addr_BRAM           <= sha_bram_addr[31:0];     // Assert address; 
                NXT_STATE           <= SHA_WRITE3;                  
            end
                    
    // ------------   SHA_WRITE3 STATE  -----------------------------------------------  
    //         
            else if(STATE == SHA_WRITE3)   
            begin      
                en_BRAM             <= 1'b0;
                we_BRAM             <= 4'b1111;
                dout_BRAM           <= sha_bram_write_data;     // Assert Write Data
                addr_BRAM           <= sha_bram_addr[31:0];     // Assert address; 
                NXT_STATE           <= SHA_READ1;                   // Go to READ1 state to read written
            end

                              
    //------------    HOLD STATE   -------------------------------------------------
    //
            else if((STATE == HOLD) &&              
                   ((axi_start_read  == 1'b1) ||
                    (axi_start_write == 1'b1) ||
                    (sha_start_read  == 1'b1) ||
                    (sha_start_write == 1'b1)))            
            begin
                we_BRAM             <=  4'b0;
                en_BRAM             <=  1'b0;
                addr_BRAM           <= 32'b0;
                bram_complete       <=  1'b1;                   // Don't negate until R/W is completed
                NXT_STATE           <= HOLD;
            end
            
            else if((STATE == HOLD) &&              
                   ((axi_start_read  == 1'b0) &&
                    (axi_start_write == 1'b0) &&
                    (sha_start_write == 1'b0) &&
                    (sha_start_read  == 1'b0)))            
            begin
                we_BRAM             <=  4'b0;
                en_BRAM             <=  1'b0;
                addr_BRAM           <= 32'b0;
                bram_complete       <=  1'b0;
                NXT_STATE           <= IDLE;
            end
        end   
     end        


    //------------    UPDATE NEXT STATE-----------------------------------------------
    //       
    always @(posedge axi_clk) begin             // Use POS edge of clock
                 
        if ( rst_BRAM  == 1'b1 ) begin          // In reset
            STATE           <=  IDLE;           // Start in the IDLE STATE waiting for Start 
        end 
         
        else if ( rst_BRAM == 1'b0 ) begin      // Not in reset
            STATE           <= NXT_STATE;
        end             
    end
    
endmodule
