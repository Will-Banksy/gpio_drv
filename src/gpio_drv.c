#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>

#include "gpio_drv.h"

// Module metadata - visible with modinfo
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Will B");
MODULE_DESCRIPTION("A driver for GPIO devices");
MODULE_VERSION("0.1");

// Constants
#define GPIODRV_DEVICE_NAME "gpio_drv"
#define GPIODRV_CLASS_NAME "class_gpio_drv"
#define GPIODRV_DEFAULT_PIN 25
#define GPIODRV_DEFAULT_OUTPUT true
#define GPIODRV_NUM_OPEN_PINS 20

// Static variables
static int s_DeviceMajor;
static struct class* s_ChrdevClass = NULL;
static struct device* s_ChrdevDevice = NULL;
/// The currently selected GPIO pin - most recently opened
static int s_GpioPin;
static bool s_GpioOutput;
static gpio_pin s_OpenPins[]; // TODO: Reformat whole LKM to work on multiple open pins simultaneously. Maybe though, have it work like it is, but track pins not explicitly closed and then add an IOCTL to close them

// Macros - Allows easier configuring of how the driver works overall, as well as making my code more DRY
#define gpiodrv_request() if(gpio_is_valid(s_GpioPin) < 0) { return -ENODEV; } else if(gpio_request(s_GpioPin, "gpio_drv_pin") < 0) { return -EAGAIN; }
#define gpiodrv_request_unsafe() gpio_request(s_GpioPin, "gpio_drv_pin");
#define gpiodrv_configure() if(s_GpioOutput) { gpio_direction_output(s_GpioPin, 0); } else { gpio_direction_input(s_GpioPin); }
#define gpiodrv_setup() gpiodrv_request(); gpiodrv_configure();
#define gpiodrv_setup_unsafe() gpiodrv_request_unsafe(); gpiodrv_configure();
#define gpiodrv_set(value) gpio_set_value(s_GpioPin, value);
#define gpiodrv_get(value) gpio_get_value(s_GpioPin);
#define gpiodrv_free() gpiodrv_set(0); gpio_free(s_GpioPin);

// File operations

static int gpio_drv_fopen(struct inode* inode, struct file* file) {
	pr_info("gpio_drv: %s\n", __func__);
	return 0;
}

static int gpio_drv_frelease(struct inode* inode, struct file* file) {
	pr_info("gpio_drv: %s\n", __func__);
	return 0;
}

static ssize_t gpio_drv_fread(struct file* file, char* buffer, size_t length, loff_t* offset) {
	int gpio_val;

	// Get the GPIO value of the pin, then set the output buffer to that value in ASCII
	gpio_val = gpiodrv_get();
	gpio_val += '0';
	memset(buffer, gpio_val, length);

	pr_info("gpio_drv: %s (read %u bytes)\n", __func__, length);

	return length;
}

static ssize_t gpio_drv_fwrite(struct file* file, const char* buffer, size_t length, loff_t* offset) {
	char last_written;
	int i;

	// Loop backward from end of input buffer until found first instance of either '1' or '0'
	i = length - 1;
	while(i >= 0) {
		last_written = buffer[i];

		// Then set the GPIO pin to low for '0' or high for '1'
		if(last_written == '0') {
			gpiodrv_set(0);
			pr_info("gpio_drv: Set GPIO pin %u to low", s_GpioPin);
			break;
		} else if(last_written == '1') {
			gpiodrv_set(1);
			pr_info("gpio_drv: Set GPIO pin %u to high", s_GpioPin);
			break;
		} else {
			i--;
		}
	}

	pr_info("gpio_drv: %s (wrote %u bytes)\n", __func__, length);

	return length;
}

static long gpio_drv_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {
	gpio_pin pin;

	pr_info("gpio_drv: ioctl cmd %u\n", cmd);

	switch(cmd) {
		case GPIODRV_IOCTL_NOTHING:
			break;

		case GPIODRV_IOCTL_WRITEPIN: {
			if((void*)arg == NULL) {
				return -EBADMSG;
			}

			pin = *(gpio_pin*)arg;

			gpiodrv_set(pin.value);

			break;
		}

		case GPIODRV_IOCTL_READPIN: {
			if((void*)arg == NULL) {
				return -EBADMSG;
			}

			pin = *(gpio_pin*)arg;

			pin.value = gpiodrv_get();

			break;
		}

		case GPIODRV_IOCTL_SELECTPIN: {
			if((void*)arg == NULL) {
				return -EBADMSG;
			}

			pin = *(gpio_pin*)arg;

			gpiodrv_free();

			s_GpioPin = pin.pin_num;
			s_GpioOutput = pin.output;

			gpiodrv_setup();

			break;
		}

		default:
			return -ENOIOCTLCMD;
	}

	return 0;
}

struct file_operations gpio_drv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gpio_drv_ioctl,
	.open = gpio_drv_fopen,
	.release = gpio_drv_frelease,
	.read = gpio_drv_fread,
	.write = gpio_drv_fwrite,
};

// ---

// LKM init/exit

static int __init gpio_drv_init(void) {
	pr_info("gpio_drv: Initialising...\n");

	s_GpioPin = GPIODRV_DEFAULT_PIN;
	s_GpioOutput = GPIODRV_DEFAULT_OUTPUT;

	// Register new character device with a dynamically allocated major number
	s_DeviceMajor = register_chrdev(0, GPIODRV_DEVICE_NAME, &gpio_drv_fops);
	if(s_DeviceMajor < 0) {
		pr_err("gpio_drv: Character device failed to register\n");
		return s_DeviceMajor;
	}
	pr_info("gpio_drv: Character device registered with major number %d\n", s_DeviceMajor);

	// Register the device class for the character device
	s_ChrdevClass = class_create(THIS_MODULE, GPIODRV_CLASS_NAME);
	if(IS_ERR(s_ChrdevClass)) {
		unregister_chrdev(s_DeviceMajor, GPIODRV_DEVICE_NAME);
		pr_info("gpio_drv: Failed to register device class\n");
		return PTR_ERR(s_ChrdevClass); // Way of returning an error from a pointer? Just casts it to a long?
	}
	pr_info("gpio_drv: Device class registered\n");

	// Register the device
	s_ChrdevDevice = device_create(s_ChrdevClass, NULL, MKDEV(s_DeviceMajor, 0), NULL, GPIODRV_DEVICE_NAME);
	if(IS_ERR(s_ChrdevDevice)) {
		class_destroy(s_ChrdevClass);
		unregister_chrdev(s_DeviceMajor, GPIODRV_DEVICE_NAME);
		pr_info("gpio_drv: Failed to create the device\n");
		return PTR_ERR(s_ChrdevDevice);
	}
	pr_info("gpio_drv: Device created\n");

	// Using the legacy linux GPIO library, request access to the GPIO pin (idk what label does),
	// set it as output and set it's value to 1
	// TODO: Upgrade to new descriptor library
	gpiodrv_setup();
	gpio_set_value(s_GpioPin, 1);

	pr_info("gpio_drv: Initialised\n");

	return 0;
}

static void __exit gpio_drv_exit(void) {
	// Unregister and destroy the character device and related things
	device_destroy(s_ChrdevClass, MKDEV(s_DeviceMajor, 0));
	class_unregister(s_ChrdevClass);
	class_destroy(s_ChrdevClass);
	unregister_chrdev(s_DeviceMajor, GPIODRV_DEVICE_NAME);

	// Set the GPIO pin value to 0 and release it
	gpiodrv_setup_unsafe();
	gpio_set_value(s_GpioPin, 0);
	gpio_free(s_GpioPin);

	pr_info("gpio_drv exited\n");
}

module_init(gpio_drv_init);
module_exit(gpio_drv_exit);

// ---