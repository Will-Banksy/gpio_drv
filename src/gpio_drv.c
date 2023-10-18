#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>

// Module metadata - visible with modinfo
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Will B");
MODULE_DESCRIPTION("A driver for GPIO devices");
MODULE_VERSION("0.1");

// Constants
#define GPIODRV_DEVICE_NAME "gpio_drv"
#define GPIODRV_CLASS_NAME "class_gpio_drv"
#define GPIODRV_PIN 25

// Static variables
static int s_DeviceMajor;
static struct class* s_ChrdevClass = NULL;
static struct device* s_ChrdevDevice = NULL;

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
	gpio_val = gpio_get_value(GPIODRV_PIN);
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
			gpio_set_value(GPIODRV_PIN, 0);
			pr_info("gpio_drv: Set GPIO pin %u to low", GPIODRV_PIN);
			break;
		} else if(last_written == '1') {
			gpio_set_value(GPIODRV_PIN, 1);
			pr_info("gpio_drv: Set GPIO pin %u to high", GPIODRV_PIN);
			break;
		} else {
			i--;
		}
	}

	pr_info("gpio_drv: %s (wrote %u bytes)\n", __func__, length);

	return length;
}

struct file_operations gpio_drv_fops = {
	.owner = THIS_MODULE,
	.open = gpio_drv_fopen,
	.release = gpio_drv_frelease,
	.read = gpio_drv_fread,
	.write = gpio_drv_fwrite,
};

// ---

// LKM init/exit

static int __init gpio_drv_init(void) {
	pr_info("Initialising gpio_drv...\n");

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

	// Using the linux GPIO library, request access to the GPIO pin (idk what label does),
	// set it as output and set it's value to 1
	gpio_request(GPIODRV_PIN, "A1");
	gpio_direction_output(GPIODRV_PIN, 0);
	gpio_set_value(GPIODRV_PIN, 1);

	pr_info("gpio_drv initialised\n");

	return 0;
}

static void __exit gpio_drv_exit(void) {
	// Unregister and destroy the character device and related things
	device_destroy(s_ChrdevClass, MKDEV(s_DeviceMajor, 0));
	class_unregister(s_ChrdevClass);
	class_destroy(s_ChrdevClass);
	unregister_chrdev(s_DeviceMajor, GPIODRV_DEVICE_NAME);

	// Set the GPIO pin value to 0 and release it
	gpio_set_value(GPIODRV_PIN, 0);
	gpio_free(GPIODRV_PIN);

	pr_info("gpio_drv exited\n");
}

module_init(gpio_drv_init);
module_exit(gpio_drv_exit);

// ---