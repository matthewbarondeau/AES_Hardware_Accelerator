module BRAM_Selector(
    
    // DMA Signals
    input   wire [31:0] dma_addr_BRAM,
    input   wire        dma_clk_BRAM,
    input   wire [31:0] dma_dout_BRAM,
    input   wire        dma_en_BRAM,
    input   wire        dma_rst_BRAM,
    input   wire [3:0]  dma_we_BRAM,
    output  wire [31:0] dma_din_BRAM,
    
    // AES Signals
    input   wire [31:0] aes_addr_BRAM,
    input   wire        aes_clk_BRAM,
    input   wire [31:0] aes_dout_BRAM,
    input   wire        aes_en_BRAM,
    input   wire        aes_rst_BRAM,
    input   wire [3:0]  aes_we_BRAM,
    output  wire [31:0] aes_din_BRAM,
    
    // Select Signal
    input wire select_signal,

    // Outputs to BRAM
    output  wire [31:0] addr_BRAM,
    output  wire        clk_BRAM,
    output  wire [31:0] dout_BRAM,
    output  wire        en_BRAM,
    output  wire        rst_BRAM,
    output  wire [3:0]  we_BRAM,
    input   wire [31:0] din_BRAM
    
    );

    assign addr_BRAM    = (select_signal) ? aes_addr_BRAM : dma_addr_BRAM;
    assign clk_BRAM     = (select_signal) ? aes_clk_BRAM : dma_clk_BRAM;
    assign dout_BRAM    = (select_signal) ? aes_dout_BRAM : dma_dout_BRAM;
    assign en_BRAM      = (select_signal) ? aes_en_BRAM : dma_en_BRAM;
    assign rst_BRAM     = (select_signal) ? aes_rst_BRAM : dma_rst_BRAM;
    assign we_BRAM      = (select_signal) ? aes_we_BRAM : dma_we_BRAM;
    
    assign aes_din_BRAM = (select_signal) ? din_BRAM : 32'hz;
    assign dma_din_BRAM = (select_signal) ? 32'hz : din_BRAM;


endmodule