#ifndef GPIO_DRV_H
#define GPIO_DRV_H

#define GPIODRV_IOCTL_NOTHING 0
#define GPIODRV_IOCTL_WRITEPIN 1
#define GPIODRV_IOCTL_READPIN 2
#define GPIODRV_IOCTL_SELECTPIN 3

typedef struct gpio_pin {
	bool value;
	unsigned int pin_num;
	bool output;
} gpio_pin;

#endif // GPIO_DRV_H