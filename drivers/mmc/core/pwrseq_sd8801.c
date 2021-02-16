// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * pwrseq_sd8801.c - power sequence support for Marvell SD8801 Wifi chip
 *
 * Copyright (C) 2016 Matt Ranostay <matt@ranostay.consulting>
 *
 * Based on the original work pwrseq_simple.c
 *  Copyright (C) 2014 Linaro Ltd
 *  Author: Ulf Hansson <ulf.hansson@linaro.org>
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/property.h>
#include <linux/mmc/host.h>

#include "pwrseq.h"

struct mmc_pwrseq_sd8801 {
	struct mmc_pwrseq pwrseq;
	u32 power_on_signal_wait_ms;
	u32 power_off_signal_wait_ms;
	u32 wake_up_signal_wait_ms;
	struct gpio_desc *wakeup_gpio;
	struct gpio_desc *pwrdn_gpio;
};

#define to_pwrseq_sd8801(p) container_of(p, struct mmc_pwrseq_sd8801, pwrseq)

static void mmc_pwrseq_sd8801_reset(struct mmc_host *host)
{
	struct mmc_pwrseq_sd8801 *pwrseq = to_pwrseq_sd8801(host->pwrseq);

	/* release the wake-up signal */
	gpiod_set_value_cansleep(pwrseq->wakeup_gpio, 0);

	/* assert the power down signal */
	gpiod_set_value_cansleep(pwrseq->pwrdn_gpio, 1);
	if (pwrseq->power_off_signal_wait_ms)
		msleep(pwrseq->power_off_signal_wait_ms);

	/* release the power down signal */
	gpiod_set_value_cansleep(pwrseq->pwrdn_gpio, 0);
	if (pwrseq->power_on_signal_wait_ms)
		msleep(pwrseq->power_on_signal_wait_ms);

	/* assert the wake-up signal */
	gpiod_set_value_cansleep(pwrseq->wakeup_gpio, 1);
	if (pwrseq->wake_up_signal_wait_ms)
		msleep(pwrseq->wake_up_signal_wait_ms);

	/* release the wake-up signal */
	gpiod_set_value_cansleep(pwrseq->wakeup_gpio, 0);
}

static const struct mmc_pwrseq_ops mmc_pwrseq_sd8801_ops = {
	.reset = mmc_pwrseq_sd8801_reset,
};

static const struct of_device_id mmc_pwrseq_sd8801_of_match[] = {
	{ .compatible = "mmc-pwrseq-sd8801",},
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, mmc_pwrseq_sd8801_of_match);

static int mmc_pwrseq_sd8801_probe(struct platform_device *pdev)
{
	struct mmc_pwrseq_sd8801 *pwrseq;
	struct device *dev = &pdev->dev;

	pwrseq = devm_kzalloc(dev, sizeof(*pwrseq), GFP_KERNEL);
	if (!pwrseq)
		return -ENOMEM;

	pwrseq->pwrdn_gpio = devm_gpiod_get(dev, "powerdown", GPIOD_OUT_LOW);
	if (IS_ERR(pwrseq->pwrdn_gpio))
		return PTR_ERR(pwrseq->pwrdn_gpio);

	pwrseq->wakeup_gpio = devm_gpiod_get(dev, "wakeup", GPIOD_OUT_LOW);
	if (IS_ERR(pwrseq->wakeup_gpio))
		return PTR_ERR(pwrseq->wakeup_gpio);

	device_property_read_u32(dev, "power-on-signal-wait-ms",
				 &pwrseq->power_on_signal_wait_ms);
	device_property_read_u32(dev, "power-off-signal-wait-ms",
				 &pwrseq->power_off_signal_wait_ms);
	device_property_read_u32(dev, "wake-up-signal-wait-ms",
				 &pwrseq->wake_up_signal_wait_ms);

	pwrseq->pwrseq.dev = dev;
	pwrseq->pwrseq.ops = &mmc_pwrseq_sd8801_ops;
	pwrseq->pwrseq.owner = THIS_MODULE;
	platform_set_drvdata(pdev, pwrseq);

	return mmc_pwrseq_register(&pwrseq->pwrseq);
}

static int mmc_pwrseq_sd8801_remove(struct platform_device *pdev)
{
	struct mmc_pwrseq_sd8801 *pwrseq = platform_get_drvdata(pdev);

	mmc_pwrseq_unregister(&pwrseq->pwrseq);

	return 0;
}

static struct platform_driver mmc_pwrseq_sd8801_driver = {
	.probe = mmc_pwrseq_sd8801_probe,
	.remove = mmc_pwrseq_sd8801_remove,
	.driver = {
		.name = "pwrseq_sd8801",
		.of_match_table = mmc_pwrseq_sd8801_of_match,
	},
};

module_platform_driver(mmc_pwrseq_sd8801_driver);
MODULE_LICENSE("GPL v2");
