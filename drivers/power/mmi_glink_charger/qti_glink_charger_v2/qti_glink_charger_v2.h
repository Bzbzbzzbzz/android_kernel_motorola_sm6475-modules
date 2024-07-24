/*
 * Copyright (C) 2021 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __QTI_GLINK_CHARGER_H__
#define __QTI_GLINK_CHARGER_H__
#include <linux/ipc_logging.h>
/*OEM receivers maps*/
#define OEM_NOTIFY_RECEIVER_PEN_CHG	0x0
#define OEM_NOTIFY_RECEIVER_WLS_CHG	0x1
#define OEM_NOTIFY_RECEIVER_EXT_CHG	0x2

#define MAX_OEM_NOTIFY_DATA_LEN		8

#define mmi_err(chg, fmt, ...)			\
	do {						\
		pr_err("%s: %s: " fmt, chg->name,	\
		       __func__, ##__VA_ARGS__);	\
		ipc_log_string(chg->ipc_log,		\
		"E %s: %s: " fmt, chg->name, __func__, ##__VA_ARGS__); \
	} while (0)

#define mmi_warn(chg, fmt, ...)			\
	do {						\
		pr_warn("%s: %s: " fmt, chg->name,	\
		       __func__, ##__VA_ARGS__);	\
		ipc_log_string(chg->ipc_log,		\
		"W %s: %s: " fmt, chg->name, __func__, ##__VA_ARGS__); \
	} while (0)

#define mmi_info(chg, fmt, ...)			\
	do {						\
		pr_info("%s: %s: " fmt, chg->name,	\
		       __func__, ##__VA_ARGS__);	\
		ipc_log_string(chg->ipc_log,		\
		"I %s: %s: " fmt, chg->name, __func__, ##__VA_ARGS__); \
	} while (0)

#define mmi_dbg(chg, fmt, ...)			\
	do {							\
		if (*chg->debug_enabled)		\
			pr_info("%s: %s: " fmt, chg->name,	\
				__func__, ##__VA_ARGS__);	\
		else						\
			pr_debug("%s: %s: " fmt, chg->name,	\
				__func__, ##__VA_ARGS__);	\
		ipc_log_string(chg->ipc_log,		\
			"D %s: %s: " fmt, chg->name, __func__, ##__VA_ARGS__); \
	} while (0)


enum oem_property_type {
	OEM_PROP_BATT_INFO,
	OEM_PROP_CHG_INFO,
	OEM_PROP_CHG_PROFILE_INFO,
	OEM_PROP_CHG_PROFILE_DATA,
	OEM_PROP_CHG_FV,
	OEM_PROP_CHG_FCC,
	OEM_PROP_CHG_ITERM,
	OEM_PROP_CHG_FG_ITERM,
	OEM_PROP_CHG_BC_PMAX,
	OEM_PROP_CHG_QC_PMAX,
	OEM_PROP_CHG_PD_PMAX,
	OEM_PROP_CHG_WLS_PMAX,
	OEM_PROP_CHG_SUSPEND,
	OEM_PROP_CHG_DISABLE,
	OEM_PROP_DEMO_MODE,
	OEM_PROP_FACTORY_MODE,
	OEM_PROP_FACTORY_VERSION,
	OEM_PROP_TCMD,
	OEM_PROP_PMIC_ICL,
	OEM_PROP_REG_ADDRESS,
	OEM_PROP_REG_DATA,
	OEM_PROP_LPD_INFO,
	OEM_PROP_USB_SUSPEND,
	OEM_PROP_WLS_EN,
	OEM_PROP_WLS_VOLT_MAX,
	OEM_PROP_WLS_CURR_MAX,
	OEM_PROP_WLS_CHIP_ID,
	OEM_PROP_PEN_CTRL,
	OEM_PROP_PEN_ID,
	OEM_PROP_PEN_SOC,
	OEM_PROP_PEN_MAC,
	OEM_PROP_PEN_STATUS,
	OEM_PROP_WLS_RX_FOD_CURR,
	OEM_PROP_WLS_RX_FOD_GAIN,
	OEM_PROP_WLS_TX_MODE,
	OEM_PROP_WLS_FOLIO_MODE,
	OEM_PROP_WLS_DUMP_INFO,
	OEM_PROP_WLS_WLC_LIGHT_CTL,
	OEM_PROP_WLS_WLC_FAN_SPEED,
	OEM_PROP_WLS_WLC_TX_TYPE,
	OEM_PROP_WLS_WLC_TX_POWER,
	OEM_PROP_SKU_TYPE,
	OEM_PROP_HW_REVISION,
	OEM_PROP_WLS_WLC_TX_CAPABILITY,
	OEM_PROP_WLS_WLC_TX_ID,
	OEM_PROP_WLS_WLC_TX_SN,
	OEM_PROP_LPD_MITIGATE_MODE,
	OEM_PROP_CHG_PARTNER_ICL,
	OEM_PROP_MSB_DEV_INFO,
	OEM_PROP_MASTER_SWITCHEDCAP_INFO,
	OEM_PROP_SLAVE_SWITCHEDCAP_INFO,
	OEM_PROP_MASTER_SWITCHEDCAP_RESET,
	OEM_PROP_WLS_WEAK_CHARGE_CTRL,
	OEM_PROP_THERM_PRIMARY_CHG_CONTROL,
	OEM_PROP_THERM_SECONDARY_CHG_CONTROL,
	OEM_PROP_FG_DUMP_INFO,
	OEM_PROP_FG_OPERATION,
	OEM_PROP_TYPEC_RESET,
	OEM_PROP_CHG_PARTNER_SOC,
	OEM_PROP_ENCRYT_DATA,
	OEM_PROP_WLS_RX_DEV_MFG,
	OEM_PROP_WLS_RX_DEV_TYPE,
	OEM_PROP_WLS_RX_DEV_ID,
	OEM_PROP_MAIN_BATT_INFO,
	OEM_PROP_FLIP_BATT_INFO,
	OEM_PROP_MAIN_BATT_ID,
	OEM_PROP_FLIP_BATT_ID,
	OEM_PROP_MBC_MASTER_DEV_INFO,
	OEM_PROP_MBC_MASTER_CHIP_EN,
	OEM_PROP_MBC_MASTER_CHRG_DIS,
	OEM_PROP_MBC_MASTER_EXTMOS_EN,
	OEM_PROP_MBC_MASTER_CURR_MAX,
	OEM_PROP_MBC_SLAVE_DEV_INFO,
	OEM_PROP_MBC_SLAVE_CHIP_EN,
	OEM_PROP_MBC_SLAVE_CHRG_DIS,
	OEM_PROP_MBC_SLAVE_EXTMOS_EN,
	OEM_PROP_MBC_SLAVE_CURR_MAX,
	OEM_PROP_MAX,
};

enum mmi_charger_sku_type
{
	MMI_CHARGER_SKU_PRC = 0x01,
	MMI_CHARGER_SKU_ROW,
	MMI_CHARGER_SKU_NA,
	MMI_CHARGER_SKU_VZW,
	MMI_CHARGER_SKU_JPN,
	MMI_CHARGER_SKU_ITA,
	MMI_CHARGER_SKU_NAE,
	MMI_CHARGER_SKU_SUPERSET,
};

struct qti_charger_notify_data {
	u32 receiver;
	u32 data[MAX_OEM_NOTIFY_DATA_LEN];
};

extern int qti_charger_set_property(u32 property, const void *val, size_t val_len);
extern int qti_charger_get_property(u32 property, void *val, size_t val_len);
extern int qti_charger_register_notifier(struct notifier_block *nb);
extern int qti_charger_unregister_notifier(struct notifier_block *nb);

#endif
