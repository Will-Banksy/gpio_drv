#ifndef GPIO_DRV_H
#define GPIO_DRV_H

#include <asm-generic/int-ll64.h>

#define GPIODRV_IOCTL_NOTHING 0
#define GPIODRV_IOCTL_WRITEPIN 1
#define GPIODRV_IOCTL_READPIN 2
#define GPIODRV_IOCTL_SELECTPIN 3

typedef struct gpio_pin {
	/// 1 for high, 0 for low (secretly a bool)
	u8 value;
	u32 pin_num;
	/// 1 for output, 0 for input (secretly a bool)
	u8 output;
} gpio_pin;

#endif // GPIO_DRV_H