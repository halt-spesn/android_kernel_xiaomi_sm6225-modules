#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/delay.h> // for msleep

static struct gpio_desc *gpiod_a; // For brightness < 50
static struct gpio_desc *gpiod_b; // For brightness >= 50

static void flashlight_brightness_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
    if (brightness > 0 && brightness < 50) {
        gpiod_set_value(gpiod_a, 1);
        gpiod_set_value(gpiod_b, 0);
    } else if (brightness >= 50) {
        gpiod_set_value(gpiod_a, 0);
        gpiod_set_value(gpiod_b, 1);
    } else {
        gpiod_set_value(gpiod_a, 0);
        gpiod_set_value(gpiod_b, 0);
    }
}

static struct led_classdev flashlight_led = {
    .name = "led:torch",
    .brightness_set = flashlight_brightness_set,
};

static int flashlight_probe(struct platform_device *pdev)
{
    int ret = -EINVAL;
    int gpio_a = -1, gpio_b = -1;
    int i;

    struct device_node *np = pdev->dev.of_node;

    for (i = 0; i < 5; i++) {
        gpio_a = of_get_named_gpio(np, "qcom,flash-gpios", 0);
        gpio_b = of_get_named_gpio(np, "qcom,flash-gpios", 1);

        if (gpio_is_valid(gpio_a) && gpio_is_valid(gpio_b))
            break;

        dev_warn(&pdev->dev, "Retry %d: waiting for GPIOs...\n", i + 1);
        msleep(1000);
    }

    if (!gpio_is_valid(gpio_a)) {
        dev_err(&pdev->dev, "Invalid flash GPIO A\n");
        return -EINVAL;
    }

    if (!gpio_is_valid(gpio_b)) {
        dev_err(&pdev->dev, "Invalid flash GPIO B\n");
        return -EINVAL;
    }

    gpiod_a = gpio_to_desc(gpio_a);
    gpiod_b = gpio_to_desc(gpio_b);

    if (!gpiod_a || !gpiod_b) {
        dev_err(&pdev->dev, "Failed to convert GPIOs to descriptors\n");
        return -ENODEV;
    }

    // Initialize both GPIOs as output-low
    gpiod_direction_output(gpiod_a, 0);
    gpiod_direction_output(gpiod_b, 0);

    ret = devm_led_classdev_register(&pdev->dev, &flashlight_led);
    if (ret) {
        dev_err(&pdev->dev, "Failed to register LED device: %d\n", ret);
        return ret;
    }

    dev_info(&pdev->dev, "Flashlight driver probed successfully\n");
    return 0;
}

static int flashlight_remove(struct platform_device *pdev)
{
    return 0;
}

static const struct of_device_id flashlight_dt_ids[] = {
    { .compatible = "qcom,camera-flash" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, flashlight_dt_ids);

static struct platform_driver flashlight_driver = {
    .probe  = flashlight_probe,
    .remove = flashlight_remove,
    .driver = {
        .name           = "flashlight_driver",
        .of_match_table = flashlight_dt_ids,
    },
};

module_platform_driver(flashlight_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HALt The Dragon");
MODULE_DESCRIPTION("GPIO Flashlight Driver");
