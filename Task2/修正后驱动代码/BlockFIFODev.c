#include <linux/module.h>   //module架构及宏定义
#include <linux/fs.h>       //register_chrdev(); unregister_chrdev();
#include <linux/uaccess.h>  //copy_to_user(); copy_from_user();
#include <linux/wait.h>     //wait_queue_head_t
#include <linux/kfifo.h>    //DEFINE_KFIFO()
#include <linux/mutex.h>    //struct mutex

typedef unsigned long ul;

/* 指定一个0-4095范围(usigned int的高12位)内的设备号，并给设备命名 */
#define    FIFODev_MAJOR         369
#define    DEVICE_NAME      "FIFO-Device"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yige LIU");
MODULE_DESCRIPTION("MY Block and Non-Block Driver Module");

#define BUFFER_SIZE 64
DEFINE_KFIFO(FIFO_BUFFER, char, BUFFER_SIZE);  //申请缓冲区
struct mutex mutex_r, mutex_w, can_read, can_write;
static wait_queue_head_t ReadQueue;
static wait_queue_head_t WriteQueue;

static int FIFODev_open(struct inode *inode, struct file *file) {
    printk(KERN_EMERG "Device Open.\n");
    return 0;
}

static int FIFODev_release(struct inode *inode, struct file *filp) { 
    printk(KERN_EMERG "Device Release.\n");
    return 0; 
} 

static ssize_t FIFODev_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {  //读取最大值
    int readLen;
    //保证读进程只有一个可以判断kfifo缓冲区状态，防止多个进程同时判断非空而导致不被挂起以致kfifo溢出
    if (file->f_flags & O_NONBLOCK) {  //阻塞方式与非阻塞方式的上锁方式不同
        if (mutex_trylock(&mutex_r) != 1) {  //非阻塞方式上锁失败，直接返回
            printk(KERN_EMERG "[INFO]: Nonblock try lock failed.\n");
            return -EAGAIN;
        }
    } else mutex_lock(&mutex_r);  //阻塞方式以阻塞方式上锁
    //单一的读进程判断是否要挂起
    if (kfifo_is_empty(&FIFO_BUFFER)) {  //当缓冲区无数据
        mutex_unlock(&mutex_r);  //释放判断读条件的锁
        if (file->f_flags & O_NONBLOCK) {  //非阻塞访问模式由于无数据不能读而直接返回
            printk(KERN_EMERG "[INFO]: Read, but buffer is empty.\n");
            return -EAGAIN;
        }
        //阻塞模式挂起，唤醒条件为kfifo有数据且获取到访问该缓冲区的锁，这样避免了仅判断kfifo状态而导致的过多进程被唤醒一致读溢出的发生
        wait_event_interruptible(ReadQueue, ((!kfifo_is_empty(&FIFO_BUFFER)) && (mutex_trylock(&can_read) == 1)));		
    } else mutex_unlock(&mutex_r);  //释放判断读条件的锁
    //当缓冲区有数据时进行读操作
    if (!mutex_is_locked(&can_read)) mutex_lock(&can_read);  //当不是从队列唤醒的时，首先申请缓冲区访问锁
    if (kfifo_to_user(&FIFO_BUFFER, buf, count, &readLen) == 0) {
        printk(KERN_EMERG "[INFO]: Device Read %d bytes. Used buffer size: %d.\n", readLen, kfifo_len(&FIFO_BUFFER));  //调试输出信息
    }
    mutex_unlock(&can_read);  //释放占用的缓冲区
    if (!kfifo_is_full(&FIFO_BUFFER)) {  //缓冲区有空间，唤醒写队列中进程
        wake_up_interruptible(&WriteQueue);
    }
    
    return readLen;
}

static ssize_t FIFODev_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {  //写两个数
    int writeLen;
    //保证写进程只有一个可以判断kfifo缓冲区状态，防止多个进程同时判断非满而导致不被挂起以致kfifo溢出
    if (file->f_flags & O_NONBLOCK) {  //阻塞方式与非阻塞方式的上锁方式不同
        if (mutex_trylock(&mutex_w) != 1) {  //非阻塞方式上锁失败，直接返回
            printk(KERN_EMERG "[INFO]: Nonblock try lock failed.\n");
            return -EAGAIN;
        }
    } else mutex_lock(&mutex_w);  //阻塞方式以阻塞方式上锁
    //单一的读进程判断是否要挂起
    if (kfifo_is_full(&FIFO_BUFFER)) {
        mutex_unlock(&mutex_w);  //释放判断写条件的锁
        if (file->f_flags & O_NONBLOCK) {
            printk(KERN_EMERG "[INFO]: Write, but buffer is full.\n");
            return -EAGAIN;
        }
        //阻塞模式挂起，唤醒条件为kfifo不满且获取到访问该缓冲区的锁，这样避免了仅判断kfifo状态而导致的过多进程被唤醒一致读溢出的发生
        wait_event_interruptible(WriteQueue, ((!kfifo_is_full(&FIFO_BUFFER)) && (mutex_trylock(&can_write) == 1)));
    } else mutex_unlock(&mutex_w);  //释放判断写条件的锁
    //当缓冲区有剩余空间，向FIFO写数据
    if (!mutex_is_locked(&can_write)) mutex_lock(&can_write);  //当不是从队列唤醒的时，首先申请缓冲区访问锁
    if (kfifo_from_user(&FIFO_BUFFER, buf, count, &writeLen) == 0) {
        printk(KERN_EMERG "[INFO]: Device Write %d bytes. Used buffer size: %d.\n", writeLen, kfifo_len(&FIFO_BUFFER));
    }
    mutex_unlock(&can_write);  //释放占用的缓冲区
    if (!kfifo_is_empty(&FIFO_BUFFER)) {  //缓冲区有内容，唤醒读队列中进程
        wake_up_interruptible(&ReadQueue);
    }

    return writeLen;
}

static struct file_operations FIFODev_fops = {
    .owner      =   THIS_MODULE,
    .open       =   FIFODev_open,
    .release    =   FIFODev_release, 
    .read       =   FIFODev_read,    
    .write      =   FIFODev_write,
};

static int __init FIFODev_init(void) {  //装载驱动
    int ret = register_chrdev(FIFODev_MAJOR, DEVICE_NAME, &FIFODev_fops);
    if (ret < 0) {
        printk(KERN_EMERG DEVICE_NAME " can't register major number.\n");
        return ret;
    }
    mutex_init(&mutex_r);  //初始化互斥锁
    mutex_init(&mutex_w);
    mutex_init(&can_read);
    mutex_init(&can_write);
    init_waitqueue_head(&ReadQueue);  //初始化等待队列
    init_waitqueue_head(&WriteQueue);
    printk(KERN_EMERG DEVICE_NAME " Initialized.\n");
    return 0;
}

static void __exit FIFODev_exit(void) {  //卸载驱动
    unregister_chrdev(FIFODev_MAJOR, DEVICE_NAME);
    printk(KERN_EMERG DEVICE_NAME " Removed.\n");
}

module_init(FIFODev_init);
module_exit(FIFODev_exit);