#ifndef _INT_STACK_IOCTL_H
#define _INT_STACK_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define INT_STACK_IOC_MAGIC     'S'
#define INT_STACK_IOC_SET_SIZE  _IOW(INT_STACK_IOC_MAGIC, 1, int)

#define INT_STACK_DEVICE_PATH   "/dev/int_stack"

#endif
