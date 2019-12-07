/* ===================================================================
 *  acc_interrupt.c
 *
 *  AUTHOR:     Matthew Davis (and Mark McDermott)

 *  CREATED:    May 19, 2019     Updated for ZED Board
 *
 *  DESCRIPTION: This kernel module registers interrupts from the SHA
 *               unit and measures the time between them. This is
 *               used to also measure the latency through the kernel
 *               to respond to interrupts. 
 *               
 *  DEPENDENCIES: Works on Xilinx ZED Board
 *
 */
 

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/io.h>
#include <linux/module.h>
#include <asm/gpio.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/gpio/driver.h>

#include <linux/pm_runtime.h>
#include <linux/of.h>

#define MODULE_VER "1.0"
#define INTERRUPT  165                  // Should use gic_interrupt number

#define DMA_MAJOR 244                  // Need to mknod /dev/acc_int c 244 0
#define MODULE_NM "accelerator_interrupt"

#undef DEBUG
//#define DEBUG
#undef DEBUG1
//#define DEBUG1


int             interruptcount  = 0;
int             temp            = 0;
int             len             = 0;
char            *msg            = NULL;
unsigned int    gic_interrupt;

static struct fasync_struct *fasync_acc_queue ;

/* ===================================================================
 * function: acc_int_handler
 *
 * This function is the acc_interrupt handler. It sets the tv2
 * structure using do_gettimeofday.
 */
 
//static irqreturn_t acc_int_handler(int irq, void *dev_id, struct pt_regs *regs)

irq_handler_t acc_int_handler(int irq, void *dev_id, struct pt_regs *regs)
{
  interruptcount++;
  
    #ifdef DEBUG1
    printk(KERN_INFO "acc_int: Interrupt detected in kernel \n");  // DEBUG
    #endif
  
    /* Signal the user application that an interupt occured */
  
    kill_fasync(&fasync_acc_queue, SIGIO, POLL_IN);

return  (irq_handler_t) IRQ_HANDLED;

}


static struct proc_dir_entry *proc_acc_int;

/* ===================================================================
*    function: read_proc -- Doesn't do anything
*/


int read_proc(struct file *filp,char *buf,size_t count,loff_t *offp ) 
    {
    printk("read_proc count value = %d\n", 44);    
    return 0;
}


/* ===================================================================
*    function: write_proc   --- Example code
*/

int write_proc(struct file *filp,const char *buf,size_t count,loff_t *offp)
    {
    printk("write_proc count value = %d\n", 67);

return count;
}


/* ===================================================================
 * function: acc_open
 *
 * This function is called when the acc_int device is opened
 *
 */
 
static int acc_open (struct inode *inode, struct file *file) {

    #ifdef DEBUG1
        printk(KERN_INFO "acc_int: Inside acc_open \n");  // DEBUG
    #endif
    return 0;
}

/* ===================================================================
 * function: acc_release
 *
 * This function is called when the acc_int device is
 * released
 *
 */
 
static int acc_release (struct inode *inode, struct file *file) {
    #ifdef DEBUG1
        printk(KERN_INFO "\nacc_int: Inside acc_release \n");  // DEBUG
    #endif
    return 0;
}

/* ===================================================================
 * function: acc_fasync
 *
 * This is invoked by the kernel when the user program opens this
 * input device and issues fcntl(F_SETFL) on the associated file
 * descriptor. fasync_helper() ensures that if the driver issues a
 * kill_fasync(), a SIGIO is dispatched to the owning application.
 */

static int acc_fasync (int fd, struct file *filp, int on)
{
    #ifdef DEBUG
    printk(KERN_INFO "\nacc_int: Inside acc_fasync \n");  // DEBUG
    #endif
    
    return fasync_helper(fd, filp, on, &fasync_acc_queue);
}; 

/* ===================================================================
*
*  Define which file operations are supported
*
*/

struct file_operations acc_fops = {
    .owner          =    THIS_MODULE,
    .llseek         =    NULL,
    .read           =    NULL,
    .write          =    NULL,
    .poll           =    NULL,
    .unlocked_ioctl =    NULL,
    .mmap           =    NULL,
    .open           =    acc_open,
    .flush          =    NULL,
    .release        =    acc_release,
    .fsync          =    NULL,
    .fasync         =    acc_fasync,
    .lock           =    NULL,
    .read           =    NULL,
    .write          =    NULL,
};

struct file_operations proc_fops = {
    read: read_proc,
    write: write_proc
};

/* ===================================================================
 
   This struct is critical. Make sure that the 'compatible' value
   matches what is in the the DTS file:
   
         dma@40000000 {
			#dma-cells = <0x1>;
			clock-names = "s_axi_lite_aclk", "m_axi_aclk";
			clocks = <0x1 0xf 0x1 0xf>;
			compatible = "xlnx,cdma_int";
			:
			:
			:
		};

	accelerator_lab_3@44000000 {
                        compatible = "xlnx,accelerator-lab-3-1.0";
                        interrupt-parent = <0x4>;
                        interrupts = <0x0 0x1f 0x4>;
                        reg = <0x44000000 0x1000>;
                        xlnx,s00-axi-addr-width = <0x6>;
                        xlnx,s00-axi-data-width = <0x20>;
                };
	
 */

    //{ .compatible = "xlnx,cdma_int" }, 
    
static const struct of_device_id zynq_accelerator_of_match[] = {
    { .compatible = "xlnx,accelerator-lab-3-1.0" }, 
    { /* end of table */ }
};    
    
MODULE_DEVICE_TABLE(of, zynq_accelerator_of_match);


/* ===================================================================
 *
 * zynq_acc_probe - Initialization method for a zynq_acc device
 *
 * Return: 0 on success, negative error otherwise.
 */

static int zynq_acc_probe(struct platform_device *pdev)
{
    struct resource *res;
        
    printk("In probe funtion\n");

    // This code gets the IRQ number by probing the system.

    res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
   
    if (!res) {
        printk("No IRQ found\n");
        return 0;
    } 
    
    // Get the IRQ number 
    gic_interrupt  = res->start;

    printk("Probe IRQ # = %d\n", res->start);

    return 0;

}

/* ===================================================================
 *
 * zynq_acc_remove - Driver removal function
 *
 * Return: 0 always
 */
 
static int zynq_acc_remove(struct platform_device *pdev)
{
    //struct zynq_acc *acc = platform_get_drvdata(pdev)

    return 0;
}


static struct platform_driver zynq_accelerator_driver = {
    .driver    = {
        .name = MODULE_NM,
        .of_match_table = zynq_accelerator_of_match,
    },
    .probe = zynq_acc_probe,
    .remove = zynq_acc_remove,
};


/* ===================================================================
 * function: init_acc_int
 *
 * This function creates the /proc directory entry acc_interrupt.
 */
 
static int __init init_acc_int(void)
{

    int rv = 0;
    int err = 0;
    
    platform_driver_unregister(&zynq_accelerator_driver);
    
   
    printk("ZED Interrupt Module\n");
    printk("ZED Interrupt Driver Loading.\n");
    printk("Using Major Number %d on %s\n", DMA_MAJOR, MODULE_NM); 

    err = platform_driver_register(&zynq_accelerator_driver);
      
    if(err !=0) printk("Driver register error with number %d\n",err);       
    else        printk("Driver registered with no error\n");
    
    if (register_chrdev(DMA_MAJOR, MODULE_NM, &acc_fops)) {
        printk("acc_int: unable to get major %d. ABORTING!\n", DMA_MAJOR);
    goto no_acc_interrupt;
    }

    proc_acc_int = proc_create("acc-interrupt", 0444, NULL, &proc_fops );
    msg=kmalloc(GFP_KERNEL,10*sizeof(char));
    
    if(proc_acc_int == NULL) {
          printk("acc_int: create /proc entry returned NULL. ABORTING!\n");
    goto no_acc_interrupt;
    }

    // Request interrupt
    
    rv = request_irq(gic_interrupt, 
                    (irq_handler_t) acc_int_handler, 
                     IRQF_TRIGGER_RISING,
                     //"cdma-controller", 
                     "acc-controller", 
                     NULL);
    
    
   /* 
    rv = request_irq(46, 
                    (irq_handler_t) acc_int_handler, 
                     0x84,
                    "xilinx-dma-controller", NULL);
  */
  
    if ( rv ) {
        printk("Can't get interrupt %d\n", gic_interrupt);
    goto no_acc_interrupt;
    }

    printk(KERN_INFO "%s %s Initialized\n",MODULE_NM, MODULE_VER);
    
    return 0;

    // remove the proc entry on error
    
no_acc_interrupt:
    unregister_chrdev(DMA_MAJOR, MODULE_NM);
    platform_driver_unregister(&zynq_accelerator_driver);
    remove_proc_entry("acc-interrupt", NULL);
    return -EBUSY;
};

/* ===================================================================
 * function: cleanup_acc_interrupt
 *
 * This function frees interrupt then removes the /proc directory entry 
 * acc_interrupt. 
 */
 
static void __exit cleanup_acc_interrupt(void)
{

    free_irq(gic_interrupt,NULL);                   // Release IRQ    
    unregister_chrdev(DMA_MAJOR, MODULE_NM);       // Release character device
    platform_driver_unregister(&zynq_accelerator_driver);  // Unregister the driver
    remove_proc_entry("acc-interrupt", NULL);      // Remove process entry
    kfree(msg);
    printk(KERN_INFO "%s %s removed\n", MODULE_NM, MODULE_VER);
     
}


/* ===================================================================
 *
 *
 *
 */


module_init(init_acc_int);
module_exit(cleanup_acc_interrupt);

MODULE_AUTHOR("Mark McDermott");
MODULE_DESCRIPTION("accelerator proc module");
MODULE_LICENSE("GPL");

