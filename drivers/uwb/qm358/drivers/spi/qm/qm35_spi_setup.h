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
#ifndef __QM35_SPI_SETUP_H
#define __QM35_SPI_SETUP_H

#include <linux/regulator/consumer.h>
#include <linux/atomic.h>

/**
 * struct qm35_regulators - QM35 power regulators.
 * @vdd: Generic power supply.
 * @v1p8: 1.8V power supply.
 * @v2p5: 2.5V power supply.
 * @enabled: Current power regulator status.
 */
struct qm35_regulators {
	struct regulator *vdd;
	struct regulator *v1p8;
	struct regulator *v2p5;
	atomic_t enabled;
};

struct qm35_spi;
int qm35_setup_regulators(struct qm35_spi *qmspi);
int qm35_setup_gpios(struct qm35_spi *qmspi);
int qm35_setup_irq(struct qm35_spi *qmspi);

#endif /* __QM35_SPI_SETUP_H */
