#ifndef GPIO_DRV_H
#define GPIO_DRV_H

#include <asm-generic/int-ll64.h>

#define GPIODRV_IOCTL_NOTHING 50
#define GPIODRV_IOCTL_WRITEPIN 51
#define GPIODRV_IOCTL_READPIN 52
#define GPIODRV_IOCTL_OPENPIN 53
#define GPIODRV_IOCTL_CLOSEPIN 54

typedef struct gpio_pin {
	/// 1 for high, 0 for low (secretly a bool)
	u8 value;
	u32 pin_num;
	/// 1 for output, 0 for input (secretly a bool)
	u8 output;
} gpio_pin;

#endif // GPIO_DRV_H