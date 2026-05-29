#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "int_stack_ioctl.h"

static int open_dev(void)
{
    int fd = open(INT_STACK_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Cannot open " INT_STACK_DEVICE_PATH);
        exit(EXIT_FAILURE);
    }
    return fd;
}

static void cmd_set_size(const char *arg)
{
    long val;
    char *endp;
    int fd, size, ret;

    errno = 0;
    val = strtol(arg, &endp, 10);
    if (errno || *endp != '\0' || endp == arg || val <= 0) {
        fprintf(stderr, "ERROR: size should be > 0\n");
        exit(-EINVAL);
    }

    size = (int)val;
    fd = open_dev();
    ret = ioctl(fd, INT_STACK_IOC_SET_SIZE, &size);
    if (ret < 0) {
        if (errno == EINVAL)
            fprintf(stderr, "ERROR: size should be > 0\n");
        else if (errno == EBUSY)
            fprintf(stderr, "ERROR: stack has more elements than new size\n");
        else
            perror("ERROR: ioctl failed");
        close(fd);
        exit(-errno);
    }
    close(fd);
}

static void cmd_push(const char *arg)
{
    long val;
    char *endp;
    int fd, ival;
    ssize_t ret;

    errno = 0;
    val = strtol(arg, &endp, 10);
    if (errno || *endp != '\0' || endp == arg) {
        fprintf(stderr, "ERROR: invalid integer '%s'\n", arg);
        exit(EXIT_FAILURE);
    }

    ival = (int)val;
    fd = open_dev();
    ret = write(fd, &ival, sizeof(int));
    if (ret < 0) {
        if (errno == ERANGE)
            fprintf(stderr, "ERROR: stack is full\n");
        else
            perror("ERROR: push failed");
        close(fd);
        exit(-errno);
    }
    close(fd);
}

static void cmd_pop(void)
{
    int fd, val;
    ssize_t ret;

    fd = open_dev();
    ret = read(fd, &val, sizeof(int));
    if (ret == 0) {
        printf("NULL\n");
    } else if (ret < 0) {
        perror("ERROR: pop failed");
        close(fd);
        exit(-errno);
    } else {
        printf("%d\n", val);
    }
    close(fd);
}

static void cmd_unwind(void)
{
    int fd, val;
    ssize_t ret;

    fd = open_dev();
    while (1) {
        ret = read(fd, &val, sizeof(int));
        if (ret == 0)
            break;
        if (ret < 0) {
            perror("ERROR: unwind failed");
            close(fd);
            exit(-errno);
        }
        printf("%d\n", val);
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s {set-size|push|pop|unwind} [arg]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "set-size") == 0) {
        if (argc < 3) { fprintf(stderr, "ERROR: set-size needs argument\n"); return 1; }
        cmd_set_size(argv[2]);
    } else if (strcmp(argv[1], "push") == 0) {
        if (argc < 3) { fprintf(stderr, "ERROR: push needs argument\n"); return 1; }
        cmd_push(argv[2]);
    } else if (strcmp(argv[1], "pop") == 0) {
        cmd_pop();
    } else if (strcmp(argv[1], "unwind") == 0) {
        cmd_unwind();
    } else {
        fprintf(stderr, "ERROR: unknown command '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
