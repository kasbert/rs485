#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <linux/serial.h>

/* Include definition for RS485 ioctls: TIOCGRS485 and TIOCSRS485 */
#include <sys/ioctl.h>

#define ARRAY_SIZE(x) sizeof(x)/sizeof(x[0])

struct cmd {
    const char *name;
    int (*fn)(int fd, int argc, const char **argv);
    int open_options;

};

static int send(int fd, int argc, const char **argv)
{
    if (argc < 0) {
        puts("send needs data to send\n");
        return -1;
    }
    write(fd, argv[0], strlen(argv[0]));
    write(fd, "\n", 1);
}

static int receive(int fd, int argc, const char **argv)
{
    int read_count = 0;
    if (argc > 0) {
        read_count = atol(argv[0]);
    }

    for (int i = 0; i < read_count || (read_count == 0); i ++) {
        char buf;
        int len;
        len = read(fd, &buf, 1);
        printf("%c", buf);
        fflush(stdout);
    }

    return 0;
}

struct cmd commands [] = {
    {"send", send, O_WRONLY | O_DSYNC | O_SYNC},
    {"receive", receive, O_RDONLY | O_SYNC}
};

static int config_rs485_mode(int fd)
{
    int ret;
    struct serial_rs485 rs485conf;

    ret = ioctl (fd, TIOCGRS485, &rs485conf);
    if (ret < 0) {
        printf("Can not get rs485 mode, ioctl failed %d (errno: %d)\n", ret, errno);
        return -1;
    }

    int rtsflag;

    /*
     * On Linux RTS is set when a tty device is open. This is not good
     * when RTS is used for RS485. In this case RTS will prevent the
     * transceiver to receive any data. So here RTS will be cleared.
     */
    rtsflag = TIOCM_RTS;
    ret = ioctl(fd, TIOCMBIC, &rtsflag);
    if (ret < 0) {
        printf("Can not set rts pint, octl failed %d, (errno: %d)\n", ret, errno);
        return -1;
    }

    /* Enable RS485 mode */
    rs485conf.flags |= SER_RS485_ENABLED;

    /* Set RTS when sending */
    rs485conf.flags |= SER_RS485_RTS_ON_SEND;

    /* Cleare RTS after sending */
    rs485conf.flags &= ~SER_RS485_RTS_AFTER_SEND;

    /* Set RTS delay before send */
    rs485conf.delay_rts_before_send = 0;

    /* Set RTS delay after send */
    rs485conf.delay_rts_after_send = 0;

    /* Don't receive data while sending */
    rs485conf.flags &= ~SER_RS485_RX_DURING_TX;

    ret = ioctl (fd, TIOCSRS485, &rs485conf);
    if (ret < 0) {
        printf("Can not send rs485 mode, ioctl failed %d (errno: %d)\n", ret, errno);
        return -1;
    }

    return 0;

}

static void usage(void)
{
    printf("rs485 {serial interface} {command}\n"
            "\tserial interface: e.g. /dev/ttyS0\n"
            "\tcommands:\n"
            "\t\tsend <string to send>\n"
            "\t\treceive <receive count, 0=forever>\n"
            "\tnote:\n"
            "\t\tuse stty to configure echoing and baudrate\n");
}

static int run_command(const char *tty, const struct cmd *cmd, int argc, const char **argv)
{
    /* Open your specific device (e.g., /dev/mydevice): */
    int fd = open (tty, cmd->open_options);
    if (fd < 0) {
        printf("Can not open %s\n", tty);
        return -1;
    }

    fcntl(fd, F_SETFL, 0);

    if(config_rs485_mode(fd)) {
        return -1;
    }

    cmd->fn(fd, argc, &argv[0]);

    close (fd);
    return 0;
}

int main(int argc, const char **argv)
{
    const char *tty;
    const char *cmd;
    int ret = -1;

    if (argc < 3) {
        usage();
        return -1;
    }

    tty = argv[1];
    cmd = argv[2];

    for (int i = 0; i < ARRAY_SIZE(commands); i++) {
        if (strcmp(commands[i].name, cmd) == 0) {
            ret = run_command(tty, &commands[i], argc - 3, &argv[3]);
            break;
        }
    }

    return ret;
}
