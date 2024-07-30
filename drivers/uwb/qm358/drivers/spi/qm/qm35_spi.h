/*
 * This file is part of the UWB stack for linux.
 *
 * Copyright (c) 2020-2021 Qorvo US, Inc.
 *
 * This software is provided under the GNU General Public License, version 2
 * (GPLv2), as well as under a Qorvo commercial license.
 *
 * You may choose to use this software under the terms of the GPLv2 License,
 * version 2 ("GPLv2"), as published by the Free Software Foundation.
 * You should have received a copy of the GPLv2 along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * This program is distributed under the GPLv2 in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GPLv2 for more
 * details.
 *
 * If you cannot meet the requirements of the GPLv2, you may not use this
 * software for any purpose without first obtaining a commercial license from
 * Qorvo. Please contact Qorvo to inquire about licensing terms.
 */
#ifndef __QM35_SPI_H
#define __QM35_SPI_H

#include <linux/spi/spi.h>
#include <linux/atomic.h>

#include "qm35.h"
#include "qm35_spi_setup.h"
#include "qm35_spi_thread.h"
#include "qm35_spi_fw.h"
#include "qm35_uci_probe.h"

#define QM35_RESET_DURATION_US 2000
#define QM35_RESET_BACKOFF_DURATION_US 25000

/**
 * enum qm35_spi_debug_flags - QM35 SPI debug_flags bit-field definition.
 * @QMSPI_NO_RESET: Disable reset GPIO toggle before probing.
 * @QMSPI_AUTO_TRACE: Activate traces automatically for QM35 (core & SPI).
 * @QMSPI_FORCE_FW_UPDATE: Always flash QM35 firmware after probing.
 * @QMSPI_NO_FW_UPDATE: Disable firmware flashing completely.
 * @QMSPI_NO_RESET_ON_SUSPEND: Disable holding reset in PM suspend callback.
 * @QMSPI_COMBINED_WRITE: Allow combined read/write while receiving a frame.
 * @QMSPI_AUTO_LOAD_OPTIONAL: Automatically load optional modules.
 * @QMSPI_NO_PROBING: Disable probing. Assume device always present.
 * @QMSPI_LOW_GPIO: Bypass ready, wakeup and exton GPIOs setup.
 */
enum qm35_spi_debug_flags {
	QMSPI_NO_RESET = BIT(0),
	QMSPI_AUTO_TRACE = BIT(1),
	QMSPI_FORCE_FW_UPDATE = BIT(2),
	QMSPI_NO_FW_UPDATE = BIT(3),
	QMSPI_NO_RESET_ON_SUSPEND = BIT(4),
	QMSPI_COMBINED_WRITE = BIT(5),
	QMSPI_AUTO_LOAD_OPTIONAL = BIT(6),
	QMSPI_NO_PROBING = BIT(7),
	QMSPI_LOW_GPIO = BIT(8),
};

/**
 * struct qm35_spi_work_params - Parameters for HSSPI work.
 * @data_out: Buffer for payload data to send.
 * @data_in: Buffer for data to recv.
 * @size: Payload data size.
 * @type: Payload type.
 * @flags: Received flags.
 * @wakeup: Flag to force wakeup.
 */
struct qm35_spi_work_params {
	const void *data_out;
	void *data_in;
	size_t size;
	enum qm35_transport_msg_type type;
	int flags;
	bool wakeup;
};

/**
 * struct qm35_spi - QM35 SPI device structure.
 * @base: Core QM35 device structure.
 * @spi: Back-pointer to struct spi_device for this device.
 * @of_max_speed_hz: Saved SPI max speed from device tree.
 * @regulators: Configured power regulators.
 * @reset_gpio: The GPIO to reset the chip.
 * @irq_gpio: The GPIO to use for IRQ if any.
 * @ready_gpio: The GPIO to use for READY status if any.
 * @exton_gpio: The GPIO to use for EXTON status if any.
 * @wakeup_gpio: The GPIO to use for WAKEUP if any.
 * @worker: The worker thread.
 * @fw: The firmware upgrade data.
 * @work_send: Work structure for the send.
 * @work_recv: Work structure for the recv.
 * @send_params: Parameters used by the send work.
 * @probing_data: Probing related data.
 * @suspend_reset: Force chip reset at end of PM suspend.
 * @started: The boolean that notifies if the chip is started or not.
 * @prd_done: Bool showing state of the pre read call.
 * @should_read: Bool attesting if we need to read or not.
 * @should_write: Bool attesting if we need to write or not.
 * @prd_length: Integer to store the length of the pre read.
 */
struct qm35_spi {
	struct qm35 base;
	struct spi_device *spi;
	u32 of_max_speed_hz;
	struct qm35_regulators regulators;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *irq_gpio;
	struct gpio_desc *ready_gpio;
	struct gpio_desc *wakeup_gpio;
	struct gpio_desc *exton_gpio;
	struct qm35_worker worker;
	struct qm35_firmware fw;
	struct qm35_work work_recv;
	struct qm35_work work_send;
	struct qm35_spi_work_params send_params;
	struct qm35_uci_probing probing_data;
	bool suspend_reset;
	bool started;
	atomic_t prd_done;
	atomic_t should_read;
	atomic_t should_write;
	int prd_length;
};

static inline struct qm35_spi *qm35_to_qm35_spi(const struct qm35 *qm35)
{
	return container_of(qm35, struct qm35_spi, base);
}

/* Global variables. */
extern u32 qm35_hsspi_delay_us;
extern u32 qm35_regulator_delay_us;
extern u32 qm35_debug_flags;
extern const char *qm35_fw_name;

/* Global APIs. */
int qm35_spi_isr(struct qm35_spi *qmspi);
int qm35_spi_reset_wait_ready(struct qm35_spi *qmspi, bool bootrom, bool wait);

#endif /* __QM35_SPI_H */
