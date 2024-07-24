/*
 * Copyright (C) 2024 Motorola Mobility LLC
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
#include "mmi_glink_core.h"
#include "battery_host.h"
#include <linux/power_supply.h>
#include <linux/of.h>
#include "qti_glink_charger_v2.h"

static struct mmi_glink_chip *this_root_chip =  NULL;
static struct battery_host *this_batt_host = NULL;

static char *charge_rate[] = {
	"None", "Normal", "Weak", "Turbo", "Turbo_30W", "Hyper"
};
static ssize_t charge_rate_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip is invalid\n");
		return -ENODEV;
	}

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%s\n",
			 charge_rate[this_root_chip->max_charger_rate]);
}
static DEVICE_ATTR(charge_rate, S_IRUGO, charge_rate_show, NULL);

static ssize_t age_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct battery_host *batt_host = this_batt_host;

	if (!batt_host) {
		pr_err("batt_host: chip is invalid\n");
		return -ENODEV;
	}

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", batt_host->age);
}
static DEVICE_ATTR(age, S_IRUGO, age_show, NULL);

static ssize_t state_of_health_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct battery_host *batt_host = this_batt_host;

	if (!batt_host) {
		pr_err("batt_host: chip is invalid\n");
		return -ENODEV;
	}

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", batt_host->state_of_health);
}

static DEVICE_ATTR(state_of_health, S_IRUGO, state_of_health_show, NULL);

static ssize_t first_usage_date_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct battery_host *batt_host = this_batt_host;

	if (!batt_host) {
		pr_err("batt_host: chip is invalid\n");
		return -ENODEV;
	}

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", batt_host->first_usage_date);
}

static ssize_t first_usage_date_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long r;
	unsigned long first_usage_date;

	struct battery_host *batt_host = this_batt_host;

	if (!batt_host) {
		pr_err("batt_host: chip is invalid\n");
		return -ENODEV;
	}

	r = kstrtoul(buf, 0, &first_usage_date);
	if (r) {
		mmi_err(this_root_chip, "Invalid first_usage_date value = %lu\n", first_usage_date);
		return -EINVAL;
	}

	batt_host->first_usage_date = first_usage_date;

	return r ? r : count;
}

static DEVICE_ATTR(first_usage_date, 0644, first_usage_date_show, first_usage_date_store);

static ssize_t manufacturing_date_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct battery_host *batt_host = this_batt_host;

	if (!batt_host) {
		pr_err("batt_host: chip is invalid\n");
		return -ENODEV;
	}

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", batt_host->manufacturing_date);
}

static ssize_t manufacturing_date_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long r;
	unsigned long manufacturing_date;
	struct battery_host *batt_host = this_batt_host;

	if (!batt_host) {
		pr_err("batt_host: chip is invalid\n");
		return -ENODEV;
	}

	r = kstrtoul(buf, 0, &manufacturing_date);
	if (r) {
		mmi_err(this_root_chip, "Invalid manufacturing_date value = %lu\n", manufacturing_date);
		return -EINVAL;
	}

	batt_host->manufacturing_date = manufacturing_date;

	return r ? r : count;
}

static DEVICE_ATTR(manufacturing_date, 0644, manufacturing_date_show, manufacturing_date_store);

static struct attribute * mmi_g[] = {
	&dev_attr_charge_rate.attr,
	&dev_attr_age.attr,
	&dev_attr_state_of_health.attr,
	&dev_attr_manufacturing_date.attr,
	&dev_attr_first_usage_date.attr,
	NULL,
};

static const struct attribute_group power_supply_mmi_attr_group = {
	.attrs = mmi_g,
};

static ssize_t factory_image_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip not valid\n");
		return -ENODEV;
	}

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		mmi_err(this_root_chip, "Invalid factory image mode value = %lu\n", mode);
		return -EINVAL;
	}

	this_root_chip->factory_version = (mode) ? true : false;

	return r ? r : count;
}

static ssize_t factory_image_mode_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int state;

	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip not valid\n");
		return -ENODEV;
	}

	state = (this_root_chip->factory_version) ? 1 : 0;

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(factory_image_mode, 0644,
		factory_image_mode_show,
		factory_image_mode_store);

static ssize_t factory_charge_upper_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int state;

	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip not valid\n");
		return -ENODEV;
	}

	state = this_root_chip->upper_limit_capacity;

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(factory_charge_upper, 0444,
		factory_charge_upper_show,
		NULL);

#define MMI_CHIP_MODE_LOWER_LIMIT 35
#define MMI_CHIP_MODE_UPPER_LIMIT 80
static ssize_t force_demo_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip not valid\n");
		return -ENODEV;
	}

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		mmi_err(this_root_chip, "Invalid demo mode value = %lu\n", mode);
		return -EINVAL;
	}

	if ((mode >= MMI_CHIP_MODE_LOWER_LIMIT) &&
	    (mode <= MMI_CHIP_MODE_UPPER_LIMIT))
		this_root_chip->demo_mode = mode;
	else
		this_root_chip->demo_mode = MMI_CHIP_MODE_LOWER_LIMIT;

	return r ? r : count;
}

static ssize_t force_demo_mode_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int state;

	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip not valid\n");
		return -ENODEV;
	}

	state = this_root_chip->demo_mode;

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_demo_mode, 0644,
		force_demo_mode_show,
		force_demo_mode_store);

#define MIN_TEMP_C -20
#define MAX_TEMP_C 60
#define MIN_MAX_TEMP_C 47
#define HYSTERESIS_DEGC 2
static ssize_t force_max_chrg_temp_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip not valid\n");
		return -ENODEV;
	}

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		mmi_err(this_root_chip, "Invalid max temp value = %lu\n", mode);
		return -EINVAL;
	}

	if ((mode >= MIN_MAX_TEMP_C) && (mode <= MAX_TEMP_C))
		this_root_chip->max_chrg_temp = mode;
	else
		this_root_chip->max_chrg_temp = MAX_TEMP_C;

	return r ? r : count;
}

static ssize_t force_max_chrg_temp_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int state;

	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip not valid\n");
		return -ENODEV;
	}

	state = this_root_chip->max_chrg_temp;

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_max_chrg_temp, 0644,
		force_max_chrg_temp_show,
		force_max_chrg_temp_store);

static ssize_t force_charger_suspend_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int state;

	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip not valid\n");
		return -ENODEV;
	}

	state = this_root_chip->force_charger_disabled;

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static ssize_t force_charger_suspend_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip not valid\n");
		return -ENODEV;
	}

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("mmi_charger: Invalid charger suspend value = %lu\n", mode);
		return -EINVAL;
	}

	this_root_chip->force_charger_disabled = (mode) ? true : false;
	cancel_delayed_work(&this_root_chip->heartbeat_work);
	schedule_delayed_work(&this_root_chip->heartbeat_work, msecs_to_jiffies(0));
	mmi_info(this_root_chip, "%s force_charger_disabled\n", (mode)? "set" : "clear");

	return count;
}

static DEVICE_ATTR(force_charger_suspend, 0644,
		force_charger_suspend_show,
		force_charger_suspend_store);

static ssize_t force_charging_enable_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int state;

	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip not valid\n");
		return -ENODEV;
	}

	state = this_root_chip->force_charging_enabled;

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static ssize_t force_charging_enable_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip not valid\n");
		return -ENODEV;
	}

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("mmi_charger: Invalid charger enable value = %lu\n", mode);
		return -EINVAL;
	}

	this_root_chip->force_charging_enabled = (mode) ? true : false;
	cancel_delayed_work(&this_root_chip->heartbeat_work);
	schedule_delayed_work(&this_root_chip->heartbeat_work, msecs_to_jiffies(0));
	mmi_info(this_root_chip, "%s force_charging_enabled\n", (mode)? "set" : "clear");

	return count;
}

static DEVICE_ATTR(force_charging_enable, 0644,
		force_charging_enable_show,
		force_charging_enable_store);

static ssize_t force_charging_disable_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	if (!this_root_chip) {
		pr_err("mmi_glink_charger: chip not valid\n");
		return -ENODEV;
	}

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("mmi_charger: Invalid charger disable value = %lu\n", mode);
		return -EINVAL;
	}

	mmi_vote_charging_disable("MMI_USER", !!mode);
	cancel_delayed_work(&this_root_chip->heartbeat_work);
	schedule_delayed_work(&this_root_chip->heartbeat_work, msecs_to_jiffies(0));
	mmi_info(this_root_chip, "%s force_charging_disable\n", (mode)? "set" : "clear");

	return count;
}

static DEVICE_ATTR(force_charging_disable, 0200,
		NULL,
		force_charging_disable_store);

static ssize_t thermal_primary_charge_control_limit_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long r;
	unsigned long charge_primary_limit_level;
	struct battery_host *batt_host = this_batt_host;

	if (!batt_host) {
		pr_err("batt_host not vaild\n");
		return -ENODEV;
	}

	if (!batt_host->num_thermal_primary_levels)
		return 0;

	if (batt_host->num_thermal_primary_levels < 0) {
		pr_err("Incorrect num_thermal_primary_levels\n");
		return -EINVAL;
	}

	r = kstrtoul(buf, 0, &charge_primary_limit_level);
	if (r) {
		pr_err("Invalid charge_primary_limit_level = %lu\n", charge_primary_limit_level);
		return -EINVAL;
	}

	if (charge_primary_limit_level < 0 || charge_primary_limit_level > batt_host->num_thermal_primary_levels) {
		pr_err("Invalid charge_primary_limit_level: %lu\n", charge_primary_limit_level);
		return -EINVAL;
	}

	batt_host->thermal_primary_fcc_ua = batt_host->thermal_primary_levels[charge_primary_limit_level];
	batt_host->curr_thermal_primary_level = charge_primary_limit_level;
	pr_info("charge_primary_limit_level = %lu, thermal_primary_fcc_ma = %d",
			charge_primary_limit_level, batt_host->thermal_primary_fcc_ua);

	r = qti_charger_set_property(OEM_PROP_THERM_PRIMARY_CHG_CONTROL,
				&batt_host->thermal_primary_fcc_ua,
				sizeof(batt_host->thermal_primary_fcc_ua));

	return r ? r : count;
}

static ssize_t thermal_primary_charge_control_limit_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct battery_host *batt_host = this_batt_host;

	if (!batt_host) {
		pr_err("batt_host not vaild\n");
		return -ENODEV;
	}


	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", batt_host->curr_thermal_primary_level );
}
static DEVICE_ATTR(thermal_primary_charge_control_limit, S_IRUGO|S_IWUSR, thermal_primary_charge_control_limit_show, thermal_primary_charge_control_limit_store);

static ssize_t thermal_primary_charge_control_limit_max_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct battery_host *batt_host = this_batt_host;

	if (!batt_host) {
		pr_err("batt_host not vaild\n");
		return -ENODEV;
	}

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", batt_host->num_thermal_primary_levels);
}
static DEVICE_ATTR(thermal_primary_charge_control_limit_max, S_IRUGO, thermal_primary_charge_control_limit_max_show, NULL);


static ssize_t thermal_secondary_charge_control_limit_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long r;
	unsigned long charge_secondary_limit_level;
	struct battery_host *batt_host = this_batt_host;

	if (!batt_host) {
		pr_err("batt_host not vaild\n");
		return -ENODEV;
	}

	if (!batt_host->num_thermal_secondary_levels)
		return 0;

	if (batt_host->num_thermal_secondary_levels < 0) {
		pr_err("Incorrect num_thermal_psecondary_levels\n");
		return -EINVAL;
	}

	r = kstrtoul(buf, 0, &charge_secondary_limit_level);
	if (r) {
		pr_err("Invalid charge_secondary_limit_level = %lu\n", charge_secondary_limit_level);
		return -EINVAL;
	}

	if (charge_secondary_limit_level < 0 || charge_secondary_limit_level > batt_host->num_thermal_secondary_levels) {
		pr_err("Invalid charge_secondary_limit_level: %lu\n", charge_secondary_limit_level);
		return -EINVAL;
	}

	batt_host->thermal_secondary_fcc_ua = batt_host->thermal_secondary_levels[charge_secondary_limit_level];
	batt_host->curr_thermal_secondary_level = charge_secondary_limit_level;
	pr_info("charge_secondary_limit_level = %lu, thermal_secondary_fcc_ma = %d",
			charge_secondary_limit_level, batt_host->thermal_secondary_fcc_ua);

	r = qti_charger_set_property(OEM_PROP_THERM_SECONDARY_CHG_CONTROL,
				&batt_host->thermal_secondary_fcc_ua,
				sizeof(batt_host->thermal_secondary_fcc_ua));

	return r ? r : count;
}

static ssize_t thermal_secondary_charge_control_limit_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct battery_host *batt_host = this_batt_host;

	if (!batt_host) {
		pr_err("batt_host not vaild\n");
		return -ENODEV;
	}


	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", batt_host->curr_thermal_secondary_level );
}
static DEVICE_ATTR(thermal_secondary_charge_control_limit, S_IRUGO|S_IWUSR, thermal_secondary_charge_control_limit_show, thermal_secondary_charge_control_limit_store);

static ssize_t thermal_secondary_charge_control_limit_max_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct battery_host *batt_host = this_batt_host;

	if (!batt_host) {
		pr_err("batt_host not vaild\n");
		return -ENODEV;
	}

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", batt_host->num_thermal_secondary_levels);
}
static DEVICE_ATTR(thermal_secondary_charge_control_limit_max, S_IRUGO, thermal_secondary_charge_control_limit_max_show, NULL);

static inline int primary_get_max_charge_cntl_limit(struct thermal_cooling_device *tcd,
                    unsigned long *state)
{
    struct battery_host *batt_host = tcd->devdata;

    *state = batt_host->num_thermal_primary_levels;

    return 0;
}

static inline int primary_get_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
                    unsigned long *state)
{
    struct battery_host *batt_host = tcd->devdata;

    *state = batt_host->curr_thermal_primary_level;

    return 0;
}

static int primary_set_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
                    unsigned long state)
{
    char buf[32] = {0};

    sprintf(buf, "%ld", state);

    return thermal_primary_charge_control_limit_store(NULL, NULL, buf, strlen(buf));
}

static const struct thermal_cooling_device_ops primary_charge_ops = {
    .get_max_state = primary_get_max_charge_cntl_limit,
    .get_cur_state = primary_get_cur_charge_cntl_limit,
    .set_cur_state = primary_set_cur_charge_cntl_limit,
};

static inline int secondary_get_max_charge_cntl_limit(struct thermal_cooling_device *tcd,
                    unsigned long *state)
{
    struct battery_host *batt_host = tcd->devdata;

    *state = batt_host->num_thermal_secondary_levels;

    return 0;
}

static inline int secondary_get_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
                    unsigned long *state)
{
    struct battery_host *batt_host = tcd->devdata;

    *state = batt_host->curr_thermal_secondary_level;

    return 0;
}

static int secondary_set_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
                    unsigned long state)
{
    char buf[32] = {0};

    sprintf(buf, "%ld", state);

    return thermal_secondary_charge_control_limit_store(NULL, NULL, buf, strlen(buf));
}

static const struct thermal_cooling_device_ops secondary_charge_ops = {
    .get_max_state = secondary_get_max_charge_cntl_limit,
    .get_cur_state = secondary_get_cur_charge_cntl_limit,
    .set_cur_state = secondary_set_cur_charge_cntl_limit,
};

static void thermal_charge_control_init(struct battery_host *batt_host)
{
	struct power_supply		*battery_psy;
	int rc;

	if (!batt_host) {
		pr_err("QTI: chip not valid\n");
		return;
	}

	battery_psy = power_supply_get_by_name("battery");
	if (!battery_psy) {
		pr_err("No battery power supply found\n");
		return;
	}

	rc = device_create_file(&battery_psy->dev,
				&dev_attr_thermal_primary_charge_control_limit);
	if (rc) {
		pr_err("couldn't create thermal_primary_charge_control_limit\n");
	}

	rc = device_create_file(&battery_psy->dev,
				&dev_attr_thermal_primary_charge_control_limit_max);
	if (rc) {
		pr_err("couldn't create thermal_primary_charge_control_limit_max\n");
	}

	rc = device_create_file(&battery_psy->dev,
				&dev_attr_thermal_secondary_charge_control_limit);
	if (rc) {
		pr_err("couldn't create thermal_secondary_charge_control_limit\n");
	}

	rc = device_create_file(&battery_psy->dev,
				&dev_attr_thermal_secondary_charge_control_limit_max);
	if (rc) {
		pr_err("couldn't create thermal_secondary_charge_control_limit_max\n");
	}

	batt_host->primary_tcd = thermal_cooling_device_register("primary_charge", batt_host, &primary_charge_ops);
	if (IS_ERR_OR_NULL(batt_host->primary_tcd)) {
		rc = PTR_ERR_OR_ZERO(batt_host->primary_tcd);
		dev_err(batt_host->dev, "Failed to register thermal cooling device rc=%d\n", rc);
	}

	batt_host->secondary_tcd = thermal_cooling_device_register("secondary_charge", batt_host, &secondary_charge_ops);
	if (IS_ERR_OR_NULL(batt_host->secondary_tcd)) {
		rc = PTR_ERR_OR_ZERO(batt_host->secondary_tcd);
		dev_err(batt_host->dev, "Failed to register thermal cooling device rc=%d\n", rc);
	}

	mmi_info(this_root_chip, "thermal_charge_control_init is initialized\n");
}

static void thermal_charge_control_deinit(struct battery_host *batt_host)
{
	struct power_supply		*battery_psy;

	if (!batt_host) {
		pr_err("QTI: chip not valid\n");
		return;
	}

	battery_psy = power_supply_get_by_name("battery");
	if (!battery_psy) {
		pr_err("No battery power supply found\n");
		return;
	}

	device_remove_file(&battery_psy->dev,
				&dev_attr_thermal_primary_charge_control_limit);

	device_remove_file(&battery_psy->dev,
				&dev_attr_thermal_primary_charge_control_limit_max);

	device_remove_file(&battery_psy->dev,
				&dev_attr_thermal_secondary_charge_control_limit);

	device_remove_file(&battery_psy->dev,
				&dev_attr_thermal_secondary_charge_control_limit_max);

	thermal_cooling_device_unregister(batt_host->primary_tcd);
	thermal_cooling_device_unregister(batt_host->secondary_tcd);
	mmi_info(this_root_chip, "thermal_charge_control_init is initialized\n");
}

void battery_supply_init(struct battery_host *batt_host)
{
	int rc;
	struct power_supply	*batt_psy;
	if (!batt_host)
		return;

	if (batt_host->batt_psy)
		return;

	batt_host->batt_psy = power_supply_get_by_name("battery");
	if (!batt_host->batt_psy) {
		mmi_err(this_root_chip, "No battery supply found\n");
		return;
	}

	batt_psy = batt_host->batt_psy;
	rc = sysfs_create_group(&batt_psy->dev.kobj,
				&power_supply_mmi_attr_group);
	if (rc)
		mmi_err(this_root_chip, "failed: attr create\n");

	rc = device_create_file(batt_psy->dev.parent,
				&dev_attr_force_demo_mode);
	if (rc) {
		mmi_err(this_root_chip, "couldn't create force_demo_mode\n");
	}

	rc = device_create_file(batt_psy->dev.parent,
				&dev_attr_force_max_chrg_temp);
	if (rc) {
		mmi_err(this_root_chip, "couldn't create force_max_chrg_temp\n");
	}

	rc = device_create_file(batt_psy->dev.parent,
				&dev_attr_factory_image_mode);
	if (rc) {
		mmi_err(this_root_chip, "couldn't create factory_image_mode\n");
	}

	rc = device_create_file(batt_psy->dev.parent,
				&dev_attr_factory_charge_upper);
	if (rc)
		mmi_err(this_root_chip, "couldn't create factory_charge_upper\n");

	rc = device_create_file(batt_psy->dev.parent,
				&dev_attr_force_charger_suspend);
	if (rc)
		mmi_err(this_root_chip, "couldn't create force_charger_suspend\n");

	rc = device_create_file(batt_psy->dev.parent,
				&dev_attr_force_charging_enable);
	if (rc)
		mmi_err(this_root_chip, "couldn't create force_charging_enable\n");

	rc = device_create_file(batt_psy->dev.parent,
				&dev_attr_force_charging_disable);
	if (rc)
		mmi_err(this_root_chip, "couldn't create force_charging_disable\n");

	mmi_info(this_root_chip, "battery supply is initialized\n");

	thermal_charge_control_init(batt_host);
	return;
}

void battery_supply_deinit(struct battery_host *batt_host)
{
	struct power_supply	*batt_psy;
	if (!batt_host)
		return;

	batt_psy = batt_host->batt_psy;

	if (batt_psy) {

		device_remove_file(batt_psy->dev.parent,
					&dev_attr_force_demo_mode);
		device_remove_file(batt_psy->dev.parent,
					&dev_attr_force_max_chrg_temp);
		device_remove_file(batt_psy->dev.parent,
					&dev_attr_factory_image_mode);
		device_remove_file(batt_psy->dev.parent,
					&dev_attr_factory_charge_upper);
		device_remove_file(batt_psy->dev.parent,
					&dev_attr_force_charger_suspend);
		device_remove_file(batt_psy->dev.parent,
					&dev_attr_force_charging_enable);
		device_remove_file(batt_psy->dev.parent,
					&dev_attr_force_charging_disable);
		sysfs_remove_group(&batt_psy->dev.kobj,
					&power_supply_mmi_attr_group);
		power_supply_put(batt_psy);
	}

	thermal_charge_control_deinit(batt_host);
	return;
}

static int battery_host_parse_dt(struct mmi_glink_chip *chip, struct battery_host *batt_host)
{
	int rc = 0, len = 0;
	int i;
	u32 prev, val;
	struct device_node *node = chip->dev->of_node;

	rc = of_property_read_u32(node, "mmi,max-fcc-ma",
				  &batt_host->max_fcc_ua);
	if (rc)
		batt_host->max_fcc_ua = 4000;
	batt_host->max_fcc_ua *= 1000;

	rc = of_property_read_u32(node, "mmi,demo-fv-mv",
				  &batt_host->demo_fv_mv);
	if (rc)
		batt_host->demo_fv_mv = 4000;

	rc = of_property_count_elems_of_size(node, "mmi,thermal-primary-mitigation",
							sizeof(u32));
	if (rc <= 0) {
		return 0;
	}

	len = rc;
	prev = batt_host->max_fcc_ua;

	for (i = 0; i < len; i++) {
		rc = of_property_read_u32_index(node,
					"mmi,thermal-primary-mitigation",
					i, &val);
		if (rc < 0) {
			pr_err("failed to get thermal-primary-mitigation[%d], ret=%d\n", i, rc);
			return rc;
		}
		pr_info("thermal-primary-mitigation[%d], val=%d, prev=%d\n", i, val, prev);
		if (val > prev) {
			pr_err("Thermal primary levels should be in descending order\n");
			batt_host->num_thermal_primary_levels = -EINVAL;
			return 0;
		}
		prev = val;
	}

	batt_host->thermal_primary_levels = devm_kcalloc(batt_host->dev, len + 1,
					sizeof(*batt_host->thermal_primary_levels),
					GFP_KERNEL);
	if (!batt_host->thermal_primary_levels)
		return -ENOMEM;

	rc = of_property_read_u32_array(node, "mmi,thermal-primary-mitigation",
						&batt_host->thermal_primary_levels[1], len);
	if (rc < 0) {
		pr_err("Error in reading mmi,thermal-primary-mitigation, rc=%d\n", rc);
		return rc;
	}
	batt_host->num_thermal_primary_levels = len;
	batt_host->thermal_primary_fcc_ua = batt_host->max_fcc_ua;
	batt_host->thermal_primary_levels[0] = batt_host->thermal_primary_levels[1];

	pr_info("Parse mmi,thermal-primary-mitigation successfully, num_primary_levels %d\n", batt_host->num_thermal_primary_levels);

	rc = of_property_count_elems_of_size(node, "mmi,thermal-secondary-mitigation",
							sizeof(u32));
	if (rc <= 0) {
		return 0;
	}

	len = rc;
	prev = batt_host->max_fcc_ua;

	for (i = 0; i < len; i++) {
		rc = of_property_read_u32_index(node,
					"mmi,thermal-secondary-mitigation",
					i, &val);
		if (rc < 0) {
			pr_err("failed to get thermal-secondary-mitigation[%d], ret=%d\n", i, rc);
			return rc;
		}
		pr_info("thermal-secondary-mitigation[%d], val=%d, prev=%d\n", i, val, prev);
		if (val > prev) {
			pr_err("Thermal secondary levels should be in descending order\n");
			batt_host->num_thermal_secondary_levels = -EINVAL;
			return 0;
		}
		prev = val;
	}

	batt_host->thermal_secondary_levels = devm_kcalloc(batt_host->dev, len + 1,
					sizeof(*batt_host->thermal_secondary_levels),
					GFP_KERNEL);
	if (!batt_host->thermal_secondary_levels)
		return -ENOMEM;

	rc = of_property_read_u32_array(node, "mmi,thermal-secondary-mitigation",
						&batt_host->thermal_secondary_levels[1], len);
	if (rc < 0) {
		pr_err("Error in reading mmi,thermal-secondary-mitigation, rc=%d\n", rc);
		return rc;
	}
	batt_host->num_thermal_secondary_levels = len;
	batt_host->thermal_secondary_fcc_ua = batt_host->max_fcc_ua;
	batt_host->thermal_secondary_levels[0] = batt_host->thermal_secondary_levels[1];

	pr_info("Parse mmi,thermal-secondary-mitigation successfully, num_secondary_levels %d\n", batt_host->num_thermal_secondary_levels);
	return 0;
}

struct battery_host *battery_glink_host_init(struct mmi_glink_chip *chip)
{
	struct battery_host *batt_host;
	int rc = 0;

	if (!chip)
		goto exit;

	batt_host = kzalloc(sizeof(struct battery_host),GFP_KERNEL);
	if (!batt_host)
		goto exit;

	batt_host->dev = chip->dev;
	chip->batt_host = batt_host;
	rc = battery_host_parse_dt(chip, batt_host);
	if (rc)
		goto exit;

	this_root_chip =  chip;
	this_batt_host = batt_host;
	return batt_host;
exit:
	return (struct battery_host *)NULL;
}

void battery_glink_host_deinit(void)
{
	if(!this_batt_host)
		return;
	mmi_err(this_root_chip, "battery_glink_host_deinit");
	kfree(this_batt_host);
	this_batt_host = NULL;
	this_root_chip = NULL;
}
