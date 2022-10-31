#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/init.h>      /* Needed for the macros */
#include <linux/kern_levels.h>
#include <linux/printk.h>
#include <linux/sched.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>

#define MY_MAX_MINORS 5

struct my_device_data {
    struct cdev cdev;
    /* my data starts here */
    //...
};

struct my_device_data devs[MY_MAX_MINORS];

static int my_open(struct inode *inode, struct file *file);
static ssize_t my_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset);
static ssize_t my_write(struct file *file, const char __user *user_buffer,size_t size, loff_t * offset);

const struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .read = my_read,
    .write = my_write,
    //.release = my_release,
    //.unlocked_ioctl = my_ioctl
};

dev_t dev_num;
struct class *my_class;
const char* test = "hello world.\n";
#define MAX_BUFF_SIZE 100
char buffer[MAX_BUFF_SIZE+1];

wait_queue_head_t wq;
//DECLARE_WAIT_QUEUE_HEAD(wq);
int flag = 0;

int my_open(struct inode *inode, struct file *file)
{
    struct my_device_data *my_data;

    printk(KERN_INFO "my open called\n");
    my_data = container_of(inode->i_cdev, struct my_device_data, cdev);

    file->private_data = my_data;
    //...

    return 0;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer, size_t size, loff_t * offset)
{
    size_t csize = 0;
    size_t my = MAX_BUFF_SIZE;

    printk("my write called before wake up. flag: %p %d\n", &flag, flag);
    flag = 1 ;
    wake_up(&wq);

    csize = min(my, size);
    if (copy_from_user(buffer, user_buffer, csize)) {
        return -EFAULT;
    }
    buffer[csize] = 0;
    printk("my write called: %s %ld %lld\n", buffer, size, *offset);
    return csize;
}

ssize_t my_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset)
{
    struct my_device_data *my_data;
    size_t csize;

    flag = 0;
    printk(KERN_INFO "my read called: before wait event interruptible: %ld %lld flag: %p %d\n", size, *offset, &flag, flag);
    //wait_event_interruptible(wq, flag != 0);
    wait_event(wq, flag != 0);

    printk(KERN_INFO "my read called: wakeuped: %ld %lld flag: %p %d\n", size, *offset, &flag, flag);
    my_data = (struct my_device_data *) file->private_data;
    csize = min(strlen(test), size);

    if (copy_to_user(user_buffer, test, csize)) {
        return -EFAULT;
    }
    
    if ((size_t)(*offset) >= strlen(test))
        return 0;

    *offset += csize;
    //...
    return csize;
}
 
static int __init ktest_module_init(void)
{
    dev_t curr_dev;
    printk(KERN_INFO "hello world.\n");

    init_waitqueue_head(&wq);
    alloc_chrdev_region(&dev_num, 0, MY_MAX_MINORS, "ktest_hello");
    /* Create a class : appears at /sys/class */
    my_class = class_create(THIS_MODULE, "ktest_hello");

    for(int i = 0; i < MY_MAX_MINORS; i++) {
        /* initialize devs[i] fields */
        cdev_init(&devs[i].cdev, &my_fops);
        curr_dev = MKDEV(MAJOR(dev_num), MINOR(dev_num) + i);
        /* Create a device node for this device. Look, the class is
         * being used here. The same class is associated with N_MINOR
         * devices. Once the function returns, device nodes will be
         * created as /dev/my_dev0, /dev/my_dev1,... You can also view
         * the devices under /sys/class/my_driver_class.
         */
        device_create(my_class, NULL, curr_dev, NULL, "ktest_hello%d", i);

        cdev_add(&devs[i].cdev, curr_dev, 1);
    }
    return 0;
}
 
static void __exit ktest_module_exit(void)
{
    dev_t curr_dev;
    printk(KERN_INFO "Goodbye hello world!\n");
    for(int i = 0; i < MY_MAX_MINORS; i++) {
        /* release devs[i] fields */
        cdev_del(&devs[i].cdev);
        curr_dev = MKDEV(MAJOR(dev_num), MINOR(dev_num) + i);
        device_destroy(my_class, curr_dev);
    }
    unregister_chrdev_region(dev_num, MY_MAX_MINORS);
    class_destroy(my_class);
}
 
module_init(ktest_module_init);
module_exit(ktest_module_exit);

MODULE_LICENSE("GPL");
