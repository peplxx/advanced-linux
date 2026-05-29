#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/rwlock.h>
#include <linux/errno.h>

#include "int_stack_ioctl.h"

#define DEFAULT_MAX_SIZE 16
#define DEVICE_NAME      "int_stack"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Melnikov Sergei");
MODULE_DESCRIPTION("int_stack - integer stack chardev");

struct int_stack {
    int          *data;
    int           top;        /* -1 = empty */
    int           max_size;
    rwlock_t      lock;
    struct mutex  resize_mutex;
};

static struct int_stack stack;
static dev_t dev_num;
static struct cdev stack_cdev;
static struct class *stack_class;

/* ── open ── */
static int stack_open(struct inode *inode, struct file *filp)
{
    return 0;
}

/* ── release ── */
static int stack_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* ── read → pop ──
 * Returns sizeof(int) on success, 0 if empty (NULL), negative on error */
static ssize_t stack_read(struct file *filp, char __user *buf,
                          size_t count, loff_t *f_pos)
{
    int val;
    unsigned long flags;

    if (count < sizeof(int))
        return -EINVAL;

    write_lock_irqsave(&stack.lock, flags);

    if (stack.top < 0) {
        write_unlock_irqrestore(&stack.lock, flags);
        return 0; /* empty → NULL */
    }

    val = stack.data[stack.top];
    stack.top--;

    write_unlock_irqrestore(&stack.lock, flags);

    if (copy_to_user(buf, &val, sizeof(int)))
        return -EFAULT;

    return sizeof(int);
}

/* ── write → push ──
 * Returns sizeof(int) on success, -ERANGE if full */
static ssize_t stack_write(struct file *filp, const char __user *buf,
                           size_t count, loff_t *f_pos)
{
    int val;
    unsigned long flags;

    if (count < sizeof(int))
        return -EINVAL;

    if (copy_from_user(&val, buf, sizeof(int)))
        return -EFAULT;

    write_lock_irqsave(&stack.lock, flags);

    if (stack.top + 1 >= stack.max_size) {
        write_unlock_irqrestore(&stack.lock, flags);
        return -ERANGE;
    }

    stack.top++;
    stack.data[stack.top] = val;

    write_unlock_irqrestore(&stack.lock, flags);

    return sizeof(int);
}

/* ── ioctl → set-size ── */
static long stack_ioctl(struct file *filp, unsigned int cmd,
                        unsigned long arg)
{
    int new_size;
    int *new_data;
    unsigned long flags;

    if (cmd != INT_STACK_IOC_SET_SIZE)
        return -ENOTTY;

    if (copy_from_user(&new_size, (int __user *)arg, sizeof(int)))
        return -EFAULT;

    if (new_size <= 0)
        return -EINVAL;

    mutex_lock(&stack.resize_mutex);

    new_data = kmalloc_array(new_size, sizeof(int), GFP_KERNEL);
    if (!new_data) {
        mutex_unlock(&stack.resize_mutex);
        return -ENOMEM;
    }

    write_lock_irqsave(&stack.lock, flags);

    if (stack.top + 1 > new_size) {
        write_unlock_irqrestore(&stack.lock, flags);
        kfree(new_data);
        mutex_unlock(&stack.resize_mutex);
        return -EBUSY;
    }

    if (stack.top >= 0)
        memcpy(new_data, stack.data, (stack.top + 1) * sizeof(int));

    kfree(stack.data);
    stack.data = new_data;
    stack.max_size = new_size;

    write_unlock_irqrestore(&stack.lock, flags);
    mutex_unlock(&stack.resize_mutex);

    pr_info("int_stack: size set to %d\n", new_size);
    return 0;
}

static const struct file_operations stack_fops = {
    .owner          = THIS_MODULE,
    .open           = stack_open,
    .release        = stack_release,
    .read           = stack_read,
    .write          = stack_write,
    .unlocked_ioctl = stack_ioctl,
};

/* ── init ── */
static int __init stack_init(void)
{
    int ret;

    stack.max_size = DEFAULT_MAX_SIZE;
    stack.top = -1;
    rwlock_init(&stack.lock);
    mutex_init(&stack.resize_mutex);

    stack.data = kmalloc_array(stack.max_size, sizeof(int), GFP_KERNEL);
    if (!stack.data)
        return -ENOMEM;

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
        goto err_free;

    cdev_init(&stack_cdev, &stack_fops);
    stack_cdev.owner = THIS_MODULE;
    ret = cdev_add(&stack_cdev, dev_num, 1);
    if (ret < 0)
        goto err_unreg;

    stack_class = class_create(DEVICE_NAME);
    if (IS_ERR(stack_class)) {
        ret = PTR_ERR(stack_class);
        goto err_cdev;
    }

    if (IS_ERR(device_create(stack_class, NULL, dev_num, NULL, DEVICE_NAME))) {
        ret = -ENOMEM;
        goto err_class;
    }

    pr_info("int_stack: loaded (major=%d, default_size=%d)\n",
            MAJOR(dev_num), DEFAULT_MAX_SIZE);
    return 0;

err_class:
    class_destroy(stack_class);
err_cdev:
    cdev_del(&stack_cdev);
err_unreg:
    unregister_chrdev_region(dev_num, 1);
err_free:
    kfree(stack.data);
    return ret;
}

/* ── exit ── */
static void __exit stack_exit(void)
{
    device_destroy(stack_class, dev_num);
    class_destroy(stack_class);
    cdev_del(&stack_cdev);
    unregister_chrdev_region(dev_num, 1);
    kfree(stack.data);
    pr_info("int_stack: unloaded\n");
}

module_init(stack_init);
module_exit(stack_exit);
