#include <linux/module.h>    /* Needed by all modules */
#include <linux/kernel.h>    /* Needed for KERN_INFO */
#include <linux/init.h>      /* Needed for the macros */
#include <linux/kern_levels.h>
#include <linux/printk.h>
#include <linux/sched.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/mm.h>

#define MY_MAX_MINORS 5

static void *kmalloc_ptr;
static char *kmalloc_area;
const size_t NPAGES = 3;

struct my_device_data {
	struct cdev cdev;
	wait_queue_head_t inq;
	wait_queue_head_t outq;
	int has_write;
	struct semaphore sem;

	/* my data starts here */
	//...
};

static int can_interruptible = 1;
module_param(can_interruptible, int, 0);
MODULE_PARM_DESC(can_interruptible, "block read and write can be interruptible by signal.");

struct my_device_data devs[MY_MAX_MINORS];

static int my_open(struct inode *inode, struct file *file);
static ssize_t my_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset);
static ssize_t my_write(struct file *file, const char __user *user_buffer,size_t size, loff_t * offset);
static long my_ioctl (struct file *file, unsigned int cmd, unsigned long arg);
static int my_mmap(struct file *filp, struct vm_area_struct *vma);

const struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.open = my_open,
	.read = my_read,
	.write = my_write,
	.mmap = my_mmap,
	//.release = my_release,
	.unlocked_ioctl = my_ioctl
};

dev_t dev_num;
struct class *my_class;
const char* test = "hello world.\n";
#define MAX_BUFF_SIZE 100
char buffer[MAX_BUFF_SIZE+1];

//DECLARE_WAIT_QUEUE_HEAD(wq);

static __always_inline int __must_check lock(struct my_device_data *data)
{
	if (can_interruptible) {
		return down_interruptible(&data->sem);
	}
	down(&data->sem);
	return 0;
}

static __always_inline void unlock(struct my_device_data *data)
{
	up(&data->sem);
}

static __always_inline int __must_check wait_read(struct my_device_data *data)
{
	if (can_interruptible > 0) {
		return wait_event_interruptible(data->inq, data->has_write != 0);
	}
	wait_event(data->inq, data->has_write != 0);
	return 0;
}

static __always_inline int __must_check wait_write(struct my_device_data *data)
{
	if (can_interruptible > 0) {
		return wait_event_interruptible(data->outq, data->has_write == 0);
	}
	wait_event(data->outq, data->has_write == 0);
	return 0;
}

int my_mmap(struct file *filp, struct vm_area_struct *vma)
{
	//struct my_device_data *my_data = (struct myk_device_data*) filp->private_data;
	size_t len = vma->vm_end - vma->vm_start;
	char *start = NULL;
	int pfn, ret;
	pr_info("my_mmap: start: %lx end: %lx, offset: %lx", vma->vm_start, vma->vm_end, vma->vm_pgoff);
	if ((vma->vm_pgoff << PAGE_SHIFT) + len > (NPAGES << PAGE_SHIFT)) {
		return -EOVERFLOW;
	}
	start = kmalloc_area + (vma->vm_pgoff << PAGE_SHIFT);
	pfn = virt_to_phys((void*)start);
	ret = remap_pfn_range(vma, vma->vm_start, pfn, len, vma->vm_page_prot);
	if (ret < 0) {
		return -EIO;
	}
	return 0;
}

#define MY_IOCTL_IN 1024

long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct my_device_data *my_data =
         (struct my_device_data*) file->private_data;
	printk(KERN_INFO "my_ioctl called. my_data.has_write: %d cmd: %d\n", my_data->has_write, cmd);
    switch(cmd) {
    case MY_IOCTL_IN:
		//my_data->interruptible = !my_data->interruptible;
		//if (copy_to_user((void*)arg, &my_data->interruptible, sizeof(my_data->interruptible))) {
		//	return -EFAULT;
		//}
        /* process data and execute command */
        break;
    default:
        return -ENOTTY;
    }
    return 0;
}

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
	struct my_device_data *my_data;

	my_data = (struct my_device_data *) file->private_data;
	printk(KERN_INFO "pid: %d my write called before wake up. flag: %p %d\n", current->pid, &my_data->has_write, my_data->has_write);

	if (lock(my_data)) {
		return -ERESTARTSYS;
	}
	while (my_data->has_write)
	{
		unlock(my_data);
		if (file->f_flags & O_NONBLOCK) {
			return -EAGAIN;
		}
		if (wait_write(my_data)) {
			pr_info("wake up by signal, return erestart sys.\n");
			return -ERESTARTSYS;
		}
		if (lock(my_data)) {
			return -ERESTARTSYS;
		}
	}
	
	my_data->has_write = 1 ;
	csize = min(my, size);
	if (copy_from_user(buffer, user_buffer, csize)) {
		return -EFAULT;
	}
	buffer[csize] = 0;

	unlock(my_data);
	wake_up(&my_data->inq);
	printk(KERN_INFO "my write called: %s %ld %lld\n", buffer, size, *offset);
	return csize;
}

ssize_t my_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset)
{
	struct my_device_data *my_data;
	size_t csize;

	if ((size_t)(*offset) >= strlen(test)) {
		return 0;
	}
	my_data = (struct my_device_data *) file->private_data;
	printk(KERN_INFO "pid: %d, my read called: before wait event interruptible: %ld %lld flag: %p %d\n", current->pid, size, *offset, &my_data->has_write, my_data->has_write);
	if (lock(my_data)) {
		return -ERESTARTSYS;
	};
	while (!my_data->has_write) {
		unlock(my_data);
		if (file->f_flags & O_NONBLOCK) {
            return -EAGAIN;
		}
		if (wait_read(my_data)) {
			pr_warn("wake up by signal, return -ERESTARTSYS.\n");
			return -ERESTARTSYS;
		}
		if (lock(my_data)) {
			return -ERESTARTSYS;
		};
	}
	my_data->has_write = 0;
	printk(KERN_INFO "my read called: wakeuped: %ld %lld flag: %p %d\n", size, *offset, &my_data->has_write, my_data->has_write);

	csize = min(strlen(test), size);
	if (copy_to_user(user_buffer, test, csize)) {
		unlock(my_data);
		wake_up(&my_data->outq);
		return -EFAULT;
	}
	*offset += csize;
	unlock(my_data);
	wake_up(&my_data->outq);
	return csize;
}

static __always_inline void init_my_device(struct my_device_data *data)
{
	cdev_init(&data->cdev, &my_fops);
	init_waitqueue_head(&data->inq);
	init_waitqueue_head(&data->outq);
	sema_init(&data->sem, 1);
	data->has_write = 0;
}

static int my_dev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int __init ktest_module_init(void)
{
	dev_t curr_dev;
	printk(KERN_INFO "hello world.\n");

	alloc_chrdev_region(&dev_num, 0, MY_MAX_MINORS, "ktest_hello");
	/* Create a class : appears at /sys/class */
	my_class = class_create(THIS_MODULE, "ktest_hello");
	my_class->dev_uevent = my_dev_uevent;

	for(int i = 0; i < MY_MAX_MINORS; i++) {
		/* initialize devs[i] fields */
		init_my_device(&devs[i]);
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

	kmalloc_ptr = kmalloc((NPAGES+2) << PAGE_SHIFT, GFP_KERNEL);
	kmalloc_area = (char*)PAGE_ALIGN((unsigned long)kmalloc_ptr);

	for  (size_t i = 0; i < NPAGES; ++i) {
		char *a = (kmalloc_area) + (i << PAGE_SHIFT);
		struct page* page = virt_to_page(a);
		SetPageReserved(page);
	}
	kmalloc_area[1] = 0xaa;
	kmalloc_area[2] = 0xbb;
	kmalloc_area[3] = 0xcc;
	kmalloc_area[4] = 0xdd;
	return 0;
}

static void __exit ktest_module_exit(void)
{
	dev_t curr_dev;
	printk(KERN_INFO "Goodbye hello world!\n");
	for(int i = 0; i < MY_MAX_MINORS; i++) {
		/* release devs[i] fields */
		curr_dev = MKDEV(MAJOR(dev_num), MINOR(dev_num) + i);
		cdev_del(&devs[i].cdev);
		device_destroy(my_class, curr_dev);
	}
	unregister_chrdev_region(dev_num, MY_MAX_MINORS);
	class_destroy(my_class);

	for  (size_t i = 0; i < NPAGES; ++i) {
		char *a = (kmalloc_area) + (i << PAGE_SHIFT);
		struct page* page = virt_to_page(a);
		ClearPageReserved(page);
	}
	kfree(kmalloc_ptr);
}

module_init(ktest_module_init);
module_exit(ktest_module_exit);

MODULE_LICENSE("GPL");
