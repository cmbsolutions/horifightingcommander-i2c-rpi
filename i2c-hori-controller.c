#include <linux/i2c-dev.h>
#ifdef HAVE_SMBUS_H
#include <i2c/smbus.h>
#endif
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <linux/input-event-codes.h>

#define UINPUT_DEV_NAME "Hori I2C Fighting Commander"
#define I2C_ADDR 0x52
#define I2C_DELAY 10000
#define I2C_DELAY_US 10

#define RECONNECT_DELAY_US 500000
#define POLL_DELAY_US 50000
#define RETRY_DELAY_US 1000
#define RETRIES_MAX 5

#define BUTTONS_NUMBER 12
#define BUTTON_BYTES_UP      0x0001
#define BUTTON_BYTES_RIGHT   0x8000
#define BUTTON_BYTES_DOWN    0x4000
#define BUTTON_BYTES_LEFT    0x0002
#define BUTTON_BYTES_A       0x0010
#define BUTTON_BYTES_B       0x0040
#define BUTTON_BYTES_X       0x0008
#define BUTTON_BYTES_Y       0x0020
#define BUTTON_BYTES_L       0x2000
#define BUTTON_BYTES_R       0x0200
#define BUTTON_BYTES_START   0x0400
#define BUTTON_BYTES_SELECT  0x1000

struct button {
	int bytes;
    int linux_code;
	bool pressed;
    bool prev_pressed;
    char key;
};

// Define button mappings
struct button buttons[BUTTONS_NUMBER] = {
    {BUTTON_BYTES_UP, BTN_DPAD_UP, false, false, '^'}, // BUTTON_BYTES_UP BTN_DPAD_UP
    {BUTTON_BYTES_RIGHT, BTN_DPAD_RIGHT, false, false, '>'}, // BUTTON_BYTES_RIGHT BTN_DPAD_RIGHT
    {BUTTON_BYTES_DOWN, BTN_DPAD_DOWN, false, false, 'v'}, // BUTTON_BYTES_DOWN BTN_DPAD_DOWN
    {BUTTON_BYTES_LEFT, BTN_DPAD_LEFT, false, false, '<'}, // BUTTON_BYTES_LEFT BTN_DPAD_LEFT
    {BUTTON_BYTES_A, BTN_A, false, false, 'A'}, // BUTTON_BYTES_A
    {BUTTON_BYTES_B, BTN_B, false, false, 'B'}, // BUTTON_BYTES_B
    {BUTTON_BYTES_X, BTN_X, false, false, 'X'}, // BUTTON_BYTES_X
    {BUTTON_BYTES_Y, BTN_Y, false, false, 'Y'}, // BUTTON_BYTES_Y
    {BUTTON_BYTES_L, BTN_TL, false, false, 'L'}, // BUTTON_BYTES_L
    {BUTTON_BYTES_R, BTN_TR, false, false, 'R'}, // BUTTON_BYTES_R
    {BUTTON_BYTES_START, BTN_START, false, false, '+'}, // BUTTON_BYTES_START
    {BUTTON_BYTES_SELECT, BTN_SELECT, false, false, '-'}  // BUTTON_BYTES_SELECT
};

int debug = 0;

int setup_uinput_device() {
    struct uinput_setup uidev;

    // Open the uinput device file
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("Failed to open /dev/uinput");
        return -1;
    }

    // Enable button events
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) {
        perror("Error: ioctl EV_KEY");
        return -1;
    }

    for (int i = 0; i < BUTTONS_NUMBER; i++) {
        ioctl(fd, UI_SET_KEYBIT, buttons[i].linux_code);
    }

    // Configure the uinput device
    memset(&uidev, 0, sizeof(uidev));
    uidev.id.bustype = BUS_USB;    // Pretend to be a USB device
    uidev.id.vendor = 0x24c6;    // Example vendor ID
    uidev.id.product = 0x5510;    // Example product ID
    strcpy(uidev.name, UINPUT_DEV_NAME);

    if (ioctl(fd, UI_DEV_SETUP, &uidev) < 0) {
        perror("Error: ioctl UI_DEV_SETUP");
        return -1;
    }

    // Create the uinput device
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("Error: ioctl UI_DEV_CREATE");
        return -1;
    }

    return fd;
}

void uinput_emit(int fd, int type, int code, int value) {
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    ev.time.tv_sec = 0;
	ev.time.tv_usec = 0;

    if (write(fd, &ev, sizeof(ev)) < 0) {
        perror("Failed to emit uinput event");
    }
    else {
        if (debug) printf("Event emitted: type=%d, code=%d, value=%d\n", type, code, value);
    }
}

int init_device(int fd) {
    // Send 0x55 to 0xF0
    int res = i2c_smbus_write_byte_data(fd, 0xF0, 0x55);
    if (res < 0) {
        perror("Error: i2c bus write 1");
        return -1;
    }
    usleep(I2C_DELAY); // 10ms delay

    // Send 0x00 to 0xFB
    res = i2c_smbus_write_byte_data(fd, 0xFB, 0x00);
    if (res < 0) {
        perror("Error: i2c bus write 2");
        return -1;
    }
    usleep(I2C_DELAY); // 10ms delay

    // Send 0x01
    res = i2c_smbus_write_byte_data(fd, 0xFE, 0x01);
    if (res < 0) {
        perror("Error: i2c bus write 3");
        return -1;
    }
    usleep(I2C_DELAY); // 10ms delay
    return 0;
}

int read_bytes(int file, int offset, int c, char* to) {
    int res = i2c_smbus_write_byte(file, offset);
    if (res < 0) return res;
    usleep(I2C_DELAY_US);
    for (int i = 0; i < c; i++) {
        res = i2c_smbus_read_byte(file);
        if (res < 0) return res;
        to[i] = res;
        usleep(I2C_DELAY_US);
    }
    return c;
}
int update_device(int fd) {
    // Request button data
    int button_bytes = 0;
    uint8_t data[6] = { 0 };
    int res = -1;

    for (int retries = 0; res < 0 && retries < RETRIES_MAX; retries++) {
	    // Get the data from the device
        if (i2c_smbus_write_byte(fd, 0x00) < 0) {
            if (debug) perror("Error: need to reinit");
            // Reinitialize if not connected
            init_device(fd);
            return -1;
        }
        usleep(I2C_DELAY); // 10ms delay
    
        res = read(fd, data, 6);

	    if (res != 6) {
		    if (debug) perror("Error: i2c bus read");
		    return -1;
	    }
        if (data[0] != 0x5F) {
            if (debug) {
                perror("Error: invalid data: retry");
                for (int i = 0; i < 6; i++) {
                    printf(" %02X", data[i]);
                }
                printf("\r");
                fflush(stdout);
            }
            res = -1;
            usleep(RETRY_DELAY_US);
        }
        else {
            button_bytes |= (255 - data[4]) << 8;
            button_bytes |= (255 - data[5]);

            for (int i = 0; i < BUTTONS_NUMBER; i++) {
                buttons[i].pressed = (button_bytes & buttons[i].bytes) == buttons[i].bytes;
            }

            // Decode button bytes
            return button_bytes;
        }
    }

    return -1;
}

#define IS_PRESSED(button_index) (button_index >= 0 && button_index < BUTTONS_NUMBER && buttons[button_index].pressed)
void process_buttons(int fd, int button_bytes) {
    int syn = 0;

    for (int i = 0; i < BUTTONS_NUMBER; i++) {
        // Check if the button state has changed
        if (buttons[i].pressed != buttons[i].prev_pressed) {
			buttons[i].prev_pressed = buttons[i].pressed;
            // Emit the Linux input event for the button
            uinput_emit(fd, EV_KEY, buttons[i].linux_code, buttons[i].pressed ? 1 : 0);
            syn = 1;
        }
    }
    if (syn) {
        if (debug) {
            printf("buttons: ");
            for (int j = BUTTONS_NUMBER - 1; j >= 0; j--) {
                putchar(IS_PRESSED(j) ? buttons[j].key : ' ');
            }
            putchar('\r');
            fflush(stdout);
        }
        // Synchronize events
        uinput_emit(fd, EV_SYN, SYN_REPORT, 0);
    }
}

void cleanup_uinput_device(int fd) {
    if (ioctl(fd, UI_DEV_DESTROY) < 0) {
        if (debug) perror("Error: ioctl UI_DEV_DESTROY");
    }
    close(fd);
}

int main(int argc, char** argv) {
    int adapter_number = 1;
    char filename[20];
    int opt;
    
    while ((opt = getopt(argc, argv, "y:f:d")) != -1) {
        switch (opt) {
        case 'y':
			adapter_number = atoi(optarg);
            break;
        case 'd':
            debug = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-d] [-y <bus index=1>] \n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

	snprintf(filename, sizeof(filename), "/dev/i2c-%d", adapter_number);
    int file = open(filename, O_RDWR);
    int uinput_fd = setup_uinput_device();
    if (uinput_fd < 0) {
        perror("Failed to setup uinput");
        exit(EXIT_FAILURE);
    }

    if (file < 0) {
        perror("Failed to open the i2c device");
        exit(EXIT_FAILURE);
    }

    if (ioctl(file, I2C_SLAVE, I2C_ADDR) < 0) {
        perror("Failed to acquire bus access or talk to the device");
        exit(EXIT_FAILURE);
    }

    int button_bytes = 0;
    int connected = 0;

    while (1) {
        if (connected) {
            // Update button states
            button_bytes = update_device(file);

			if (button_bytes < 0) {
				connected = 0;
				continue;
			}

            // Process buttons
            process_buttons(uinput_fd, button_bytes);

            usleep(POLL_DELAY_US); // poll @ 50ms
        }
        else {
            uint8_t id[6];
            // Initialize the device
            if (init_device(file) < 0 || read_bytes(file, 0xFA, 6, id) < 0) {
                perror("init");
                usleep(RECONNECT_DELAY_US);
                continue;
            }
            printf("Detected device:");
            for (int i = 0; i < 6; i++) {
                printf(" %02X", id[i]);
            }
            printf("\n");
            if (id[2] != 0xa4 || id[3] != 0x20) {
                fprintf(stderr, "unknown device id %02x%02x\n", (int)id[2], (int)id[3]);
                usleep(RECONNECT_DELAY_US);
                continue;
            }
            if (id[5] != 0x01) {
                fprintf(stderr, "wrong device type %02x\n", (int)id[5]);
                usleep(RECONNECT_DELAY_US);
                continue;
            }
            connected = 1;
        }
    }

    cleanup_uinput_device(file);
    return EXIT_SUCCESS;
}