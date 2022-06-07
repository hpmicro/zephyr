/*
 * Copyright (c) 2022 hpmicro
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#define DT_DRV_COMPAT hpmicro_hpm_uart

#include <errno.h>
#include <drivers/pinctrl.h>
#include <drivers/uart.h>
#include <soc.h>
#include "hpm_common.h"
#include "hpm_uart_drv.h"
#include "hpm_clock_drv.h"

struct uart_hpm_cfg {
	UART_Type *base;
	uint32_t clock_name;
	uint32_t clock_src;
	uint32_t parity;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_config_func_t irq_config_func;
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
#ifdef CONFIG_PINCTRL
	const struct pinctrl_dev_config *pincfg;
#endif /* CONFIG_PINCTRL */
};

struct uart_hpm_data {
	uint32_t baud_rate;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_callback_user_data_t user_cb;
	void *user_data;
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
};

static int uart_hpm_init(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;
	struct uart_hpm_data *const data = dev->data;
	parity_setting_t parity;
	hpm_stat_t stat = status_success;
	uart_config_t config = {0};

#ifdef CONFIG_PINCTRL
	int ret;

	ret = pinctrl_apply_state(cfg->pincfg, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		return ret;
	}
#endif
	clock_set_source_divider(cfg->clock_name, cfg->clock_src, 1U);
	uart_default_config((UART_Type *)cfg->base, &config);
	config.src_freq_in_hz =	 clock_get_frequency(cfg->clock_name);
	config.baudrate = data->baud_rate;
	switch (cfg->parity) {
	case UART_CFG_PARITY_NONE:
		parity = parity_none;
		break;
	case UART_CFG_PARITY_ODD:
		parity = parity_odd;
		break;
	case UART_CFG_PARITY_EVEN:
		parity = parity_even;
		break;
	default:
		return -ENOTSUP;
	}
	config.parity = parity;
	stat = uart_init((UART_Type *)cfg->base, &config);

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	cfg->irq_config_func(dev);
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

	return 0;
}

static int uart_hpm_poll_in(const struct device *dev, unsigned char *c)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	if (status_success != uart_receive_byte((UART_Type *)cfg->base, c)) {
		return 1;
	} else {
		return 0;
	}
}

static void uart_hpm_poll_out(const struct device *dev, unsigned char c)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	uart_send_byte((UART_Type *)cfg->base, c);
}

static int uart_hpm_err_check(const struct device *dev)
{
	int errors = 0;

	return errors;
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

int uart_hpm_fifo_fill(const struct device *dev, const uint8_t *tx_data,
			 int len)
{
	const struct uart_hpm_cfg *const cfg = dev->config;
	uint8_t num_tx = 0U;

	if (uart_check_status((UART_Type *)cfg->base, uart_stat_transmitter_empty) != 0) {
		while ((len - num_tx > 0) && (num_tx < 16)) {
			uart_send_byte((UART_Type *)cfg->base, tx_data[num_tx++]);
		}
	}

	return num_tx;
}

int uart_hpm_fifo_read(const struct device *dev, uint8_t *rx_data,
			 const int size)
{
	const struct uart_hpm_cfg *const cfg = dev->config;
	uint8_t num_rx = 0U;

	while ((size - num_rx > 0) &&
		(uart_check_status((UART_Type *)cfg->base, uart_stat_data_ready) != 0)) {
		uart_receive_byte((UART_Type *)cfg->base, &rx_data[num_rx++]);
	}

	return num_rx;
}

void uart_hpm_irq_tx_enable(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	uart_enable_irq((UART_Type *)cfg->base, uart_intr_tx_slot_avail);
}

void uart_hpm_irq_tx_disable(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	uart_disable_irq((UART_Type *)cfg->base, uart_intr_tx_slot_avail);
}

int uart_hpm_irq_tx_ready(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	if (uart_check_status((UART_Type *)cfg->base, uart_stat_transmitter_empty) != 0) {
		return 1;
	} else {
		return 0;
	}
}

int uart_hpm_irq_tx_complete(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	if (uart_check_status((UART_Type *)cfg->base, uart_stat_transmitter_empty) != 0) {
		return 1;
	} else {
		return 0;
	}
}

void uart_hpm_irq_rx_enable(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	uart_enable_irq((UART_Type *)cfg->base, uart_intr_rx_data_avail_or_timeout);
}

void uart_hpm_irq_rx_disable(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	uart_disable_irq((UART_Type *)cfg->base, uart_intr_rx_data_avail_or_timeout);
}

int uart_hpm_irq_rx_ready(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	if (uart_check_status((UART_Type *)cfg->base, uart_stat_data_ready) != 0) {
		return 1;
	} else {
		return 0;
	}
}

void uart_hpm_irq_err_enable(const struct device *dev)
{

}

void uart_hpm_irq_err_disable(const struct device *dev)
{

}

int uart_hpm_irq_is_pending(const struct device *dev)
{
	const struct uart_hpm_cfg *const cfg = dev->config;

	if ((uart_get_irq_id((UART_Type *)cfg->base) & uart_intr_id_rx_data_avail) ||
	(uart_get_irq_id((UART_Type *)cfg->base) & uart_intr_id_tx_slot_avail)) {
		return 1;
	} else {
		return 0;
	}
}

void uart_hpm_irq_callback_set(const struct device *dev,
				 uart_irq_callback_user_data_t cb,
				 void *user_data)
{
	struct uart_hpm_data *const dev_data = dev->data;

	dev_data->user_cb = cb;
	dev_data->user_data = user_data;
}

static void uart_hpm_isr(const struct device *dev)
{
	struct uart_hpm_data * const dev_data = dev->data;

	if (dev_data->user_cb) {
		dev_data->user_cb(dev, dev_data->user_data);
	}
}

#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static const struct uart_driver_api uart_hpm_driver_api = {
	.poll_in = uart_hpm_poll_in,
	.poll_out = uart_hpm_poll_out,
	.err_check = uart_hpm_err_check,
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.fifo_fill		  = uart_hpm_fifo_fill,
	.fifo_read		  = uart_hpm_fifo_read,
	.irq_tx_enable	  = uart_hpm_irq_tx_enable,
	.irq_tx_disable	  = uart_hpm_irq_tx_disable,
	.irq_tx_ready	  = uart_hpm_irq_tx_ready,
	.irq_tx_complete  = uart_hpm_irq_tx_complete,
	.irq_rx_enable	  = uart_hpm_irq_rx_enable,
	.irq_rx_disable	  = uart_hpm_irq_rx_disable,
	.irq_rx_ready	  = uart_hpm_irq_rx_ready,
	.irq_err_enable	  = uart_hpm_irq_err_enable,
	.irq_err_disable  = uart_hpm_irq_err_disable,
	.irq_is_pending	  = uart_hpm_irq_is_pending,
	.irq_callback_set = uart_hpm_irq_callback_set,
#endif
};


#define UART_HPMICRO_IRQ_FLAGS_SENSE0(n) 0
#define UART_HPMICRO_IRQ_FLAGS_SENSE1(n) DT_INST_IRQ(n, sense)
#define UART_HPMICRO_IRQ_FLAGS(n) \
	_CONCAT(UART_HPMICRO_IRQ_FLAGS_SENSE, \
	DT_INST_IRQ_HAS_CELL(n, sense))(n)

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
#define DEV_CONFIG_IRQ_FUNC_INIT(n) \
	.irq_config_func = irq_config_func##n,
#define UART_HPMICRO_IRQ_FUNC_DECLARE(n) \
	static void irq_config_func##n(const struct device *dev);
#define UART_HPMICRO_IRQ_FUNC_DEFINE(n)	\
	static void irq_config_func##n(const struct device *dev)	\
	{	\
		ARG_UNUSED(dev);	\
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority),	\
		uart_hpm_isr, DEVICE_DT_INST_GET(n),	\
		UART_HPMICRO_IRQ_FLAGS(n));		\
		irq_enable(DT_INST_IRQN(n));	\
	}

#else
/* !CONFIG_UART_INTERRUPT_DRIVEN */
#define DEV_CONFIG_IRQ_FUNC_INIT(n)
#define UART_HPMICRO_IRQ_FUNC_DECLARE(n)
#define UART_HPMICRO_IRQ_FUNC_DEFINE(n)
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

#ifdef CONFIG_PINCTRL
#define PINCTRL_INIT(n) .pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),
#define PINCTRL_DEFINE(n) PINCTRL_DT_INST_DEFINE(n);
#else
#define PINCTRL_DEFINE(n)
#define PINCTRL_INIT(n)
#endif

#define HPM_UART_INIT(n)	\
		PINCTRL_DEFINE(n)	\
		UART_HPMICRO_IRQ_FUNC_DECLARE(n)	\
		static struct uart_hpm_data uart_hpm_data_##n = {	\
		.baud_rate = DT_INST_PROP(n, current_speed),	\
	};									\
	static const struct uart_hpm_cfg uart_hpm_config_##n = {	\
		.base = (UART_Type *)DT_INST_REG_ADDR(n),	\
		.clock_name = DT_INST_PROP(n, clock_name),	\
		.clock_src = DT_INST_PROP(n, clock_src),	\
		.parity = DT_INST_ENUM_IDX_OR(n, parity, UART_CFG_PARITY_NONE),	\
		DEV_CONFIG_IRQ_FUNC_INIT(n)		\
		PINCTRL_INIT(n)		\
	};	\
	DEVICE_DT_INST_DEFINE(n, &uart_hpm_init,	\
				  NULL,	\
				  &uart_hpm_data_##n,	\
				  &uart_hpm_config_##n, PRE_KERNEL_1,	\
				  CONFIG_SERIAL_INIT_PRIORITY,	\
				  &uart_hpm_driver_api);	\
	UART_HPMICRO_IRQ_FUNC_DEFINE(n)

DT_INST_FOREACH_STATUS_OKAY(HPM_UART_INIT)
