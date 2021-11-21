#include <linux/module.h>   //module架构及宏定义
#include <linux/fs.h>       //register_chrdev(); unregister_chrdev();
#include <linux/uaccess.h>  //copy_to_user(); copy_from_user();
#include <linux/wait.h>     //wait_queue_head_t
#include <linux/kfifo.h>    //DEFINE_KFIFO()

/* 指定一个0-4095范围(usigned int的高12位)内的设备号，并给设备命名 */
#define    BlockFIFODev_MAJOR         369
#define    DEVICE_NAME      "BlockFIFODev"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yige LIU");
MODULE_DESCRIPTION("MY FIFO Overflow Module");

#define BUFFER_SIZE 64
DEFINE_KFIFO(FIFO_BUFFER, char, BUFFER_SIZE);  //申请缓冲区
static wait_queue_head_t ReadQueue;
static wait_queue_head_t WriteQueue;

static int BlockFIFODev_open(struct inode *inode, struct file *file) {
    printk(KERN_EMERG "Device Open.\n");
    return 0;
}

static int BlockFIFODev_release(struct inode *inode, struct file *filp) { 
    printk(KERN_EMERG "Device Release.\n");
    return 0; 
} 

static ssize_t BlockFIFODev_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {  //读取最大值
    int readLen;
    if (kfifo_is_empty(&FIFO_BUFFER)) {  //当缓冲区无数据
        if (file->f_flags & O_NONBLOCK) {  //判断设备是阻塞还是非阻塞模式
            return -EAGAIN;
        }
        //使读进程等待并设置唤醒条件
        wait_event_interruptible(ReadQueue, !kfifo_is_empty(&FIFO_BUFFER));		
    }
    //当缓冲区有数据
    if (kfifo_to_user(&FIFO_BUFFER, buf, count, &readLen) == 0) {
        printk(KERN_EMERG "Device Read %d bytes. Used buffer size: %d.\n", readLen, kfifo_len(&FIFO_BUFFER));  //调试输出信息
    }
    if (!kfifo_is_full(&FIFO_BUFFER)) {  //缓冲区有空间，唤醒写队列中进程
        wake_up_interruptible(&WriteQueue);
    }
    
    return readLen;
}

static ssize_t BlockFIFODev_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {  //写两个数
    int writeLen;
    if (kfifo_is_full(&FIFO_BUFFER)) {
        if (file->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        }
        //使写进程等待并设置唤醒条件
        wait_event_interruptible(WriteQueue, !kfifo_is_full(&FIFO_BUFFER));
    }
    //当缓冲区有剩余空间，向FIFO写数据
    if (kfifo_from_user(&FIFO_BUFFER, buf, count, &writeLen) == 0) {
        printk(KERN_EMERG "Device Write %d bytes. Used buffer size: %d.\n", writeLen, kfifo_len(&FIFO_BUFFER));
    }
    if (!kfifo_is_empty(&FIFO_BUFFER)) {  //缓冲区有内容，唤醒读队列中进程
        wake_up_interruptible(&ReadQueue);
    }

    return writeLen;
}

static struct file_operations BlockFIFODev_fops = {
    .owner      =   THIS_MODULE,
    .open       =   BlockFIFODev_open,
    .release    =   BlockFIFODev_release, 
    .read       =   BlockFIFODev_read,    
    .write      =   BlockFIFODev_write,
};

static int __init BlockFIFODev_init(void) {  //装载驱动
    int ret = register_chrdev(BlockFIFODev_MAJOR, DEVICE_NAME, &BlockFIFODev_fops);
    if (ret < 0) {
        printk(KERN_EMERG DEVICE_NAME " can't register major number.\n");
        return ret;
    }
    init_waitqueue_head(&ReadQueue);  //初始化等待队列
    init_waitqueue_head(&WriteQueue);
    printk(KERN_EMERG DEVICE_NAME " Initialized.\n");
    return 0;
}

static void __exit BlockFIFODev_exit(void) {  //卸载驱动
    unregister_chrdev(BlockFIFODev_MAJOR, DEVICE_NAME);
    printk(KERN_EMERG DEVICE_NAME " Removed.\n");
}

module_init(BlockFIFODev_init);
module_exit(BlockFIFODev_exit);