#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

/*
 * Set the method for calculation fibonacci sequence
 */
#define ORIGINAL 0
#define FAST_DOUBLING 1

#define FIB_METHOD FAST_DOUBLING

#define unlikely(x) __builtin_expect(!!(x), 0)

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

#if FIB_METHOD == ORIGINAL
static long long fib_original(long long k)
{
    /* original method */
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}
#endif

#if FIB_METHOD == FAST_DOUBLING
// ref:
// https://chunminchang.github.io/blog/post/calculating-fibonacci-numbers-by-fast-doubling
static long long fib_fast_doubling(long long k)
{
    /* use fast doubling method */
    // F(0) = 0, F(1) = 1, F(2) = 2
    if (unlikely(k < 2))
        return k;

    unsigned long long a = 0, b = 1;
    unsigned int i =
        1u << (31 - __builtin_clz(k));  // find the highest set digit of k
    for (; i; i >>= 1) {
        unsigned long long c =
            a * (2 * b - a);  // F(2n) = F(n) * [ 2 * F(n+1) – F(n) ]
        unsigned long long d = a * a + b * b;  // F(2n+1) = F(n)^2 + F(n+1)^2

        if (i & k) {    // n_j is odd: n = (k_j-1)/2 => k_j = 2n + 1
            a = d;      //   F(k_j) = F(2n+1)
            b = c + d;  //   F(k_j + 1) = F(2n + 2) = F(2n) + F(2n+1)
        } else {        // k_j is even: n = k_j/2 => k_j = 2n
            a = c;      //   F(k_j) = F(2n)
            b = d;      //   F(k_j + 1) = F(2n + 1)
        }
    }
    return a;
}
#endif

static long long fib_sequence(long long k)
{
#if FIB_METHOD == ORIGINAL
    return fib_original(k);
#elif FIB_METHOD == FAST_DOUBLING
    return fib_fast_doubling(k);
#else
    return -1;
#endif
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    ktime_t kt_start = ktime_get();
    ssize_t result = fib_sequence(*offset);
    ktime_t duration = ktime_get() - kt_start;
    printk(KERN_DEBUG "%lld %lld", *offset, ktime_to_ns(duration));
    return result;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
