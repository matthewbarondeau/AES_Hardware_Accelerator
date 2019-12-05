`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 10/14/2019 11:16:36 AM
// Design Name: 
// Module Name: Timer
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


module Timer(
    input wire S_AXI_ACLK,
    input wire AXI_RESET,
    input wire capture_gate,
    input wire timer_enable,
    input wire overflow_enable,
    output wire capture_complete,
    output reg overflow_flag,
    output reg[31:0] cap_timer_out
    );
    
    //Used for Timer State Machine
    reg[1:0] current_state;
    reg[1:0] next_state;
    
        //Counting Mechanism
    reg[32:0] slv_reg3_negedge;
    
    assign capture_complete = current_state[1] && ~current_state[0];
    
      //Update Registers for counter in way that prevents race condition
      always @(negedge S_AXI_ACLK)
      begin
          slv_reg3_negedge = cap_timer_out + 1'b1;
      end
  
      always @(posedge S_AXI_ACLK)
      begin
          case(current_state)
          
          //RESET STATE
             2'b00   : 
             begin
                 if(timer_enable)
                 begin
                     next_state = 2'b01;
                 end 
                 else begin
                     next_state = 2'b00;
                 end
             end
             
          //COUNT STATE
             2'b01   : 
              begin
                  if(timer_enable == 1'b1 && capture_gate == 1'b0)
                  begin
                     next_state = 2'b01;
                  end 
                  else begin
                     next_state = 2'b10;
                  end
              end
  
          //WAIT STATE
             2'b10   : 
             begin
                 if(timer_enable == 1'b1)
                 begin
                     next_state = 2'b10;
                 end 
                 else begin
                     next_state = 2'b11;
                 end
             end
  
          //IDLE STATE
             2'b11   : 
             begin
                 if(timer_enable == 1'b1)
                 begin
                     next_state = 2'b01;
                 end 
                 else begin
                     next_state = 2'b11;
                 end
             end
  
             default : 
             begin
                 next_state = 2'b00;
             end
         endcase
           
         
         //Overflow logic
         if(overflow_enable == 1'b1)
         begin
             overflow_flag = slv_reg3_negedge[32];
         end 
         else begin
                overflow_flag = 1'b0;
         end
         
         //Update status registers based on current state
          case(current_state)
              //Reset
              2'b00:
              begin
                  cap_timer_out <= 0;
              end
              
              //Count
              2'b01:
              begin
                  cap_timer_out <= slv_reg3_negedge;
              end
              
              //Wait
              2'b10:
              begin
                  cap_timer_out <= cap_timer_out;
              end
              
              //Idle
              2'b11:
              begin
                cap_timer_out <= 0;
              end
         
          endcase
          
          current_state <= next_state; 
     end
endmodule
