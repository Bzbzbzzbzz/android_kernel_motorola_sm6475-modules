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
#include "asm-generic/errno-base.h"
#include "linux/err.h"
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/bitfield.h>
#ifdef CONFIG_EVENT_TRACING
#include <linux/trace_events.h> /* for trace_set_clr_event() */
#endif

#include "qm35_bypass.h"
#include "qm35_core.h"
#include "qm35_notifier.h"
#include "qm35_uci_dev.h"
#include "qm35_uci_dev_trc.h"
#include "qm35_uci_dev_ioctl.h"

static inline unsigned int qm35_uci_dev_get_state(struct qm35_uci_dev *uci_dev)
{
	return uci_dev->state;
}

static inline void qm35_uci_dev_set_state(struct qm35_uci_dev *uci_dev,
					  int state)
{
	uci_dev->state = state;
}

static inline unsigned
qm35_uci_dev_get_hsspi_msg_type(struct qm35_uci_dev *uci_dev)
{
	return uci_dev->hsspi_msg_type;
}

static inline void qm35_uci_dev_set_hsspi_msg_type(struct qm35_uci_dev *uci_dev,
						   unsigned hsspi_msg_type)
{
	long param = (long)hsspi_msg_type;
	/* Keep in sync type of message sent and message received */
	if (qm35_bypass_control(uci_dev->channel, QM35_BYPASS_ACTION_MSG_TYPE,
				&param) >= 0)
		uci_dev->hsspi_msg_type = hsspi_msg_type;
}

/**
 * qm35_uci_dev_listener() - Callback function for qm35_bypass_open().
 * @data: Private data of the callback, set on qm35_bypass_open() call.
 * @event: The event to forward to the callback owner.
 *
 * Listener callback function, called by qm35_bypass_event_cb() if the
 * misc device is opened and something was received. This function wake-up the
 * user-space process if it is currently blocked in the read() syscall.
 *
 * Return: Zero on success, else a negative error code.
 */
static int qm35_uci_dev_listener(void *data, enum qm35_bypass_events event)
{
	struct qm35_uci_dev *uci_dev = data;

	qm35_uci_dev_set_state(uci_dev, QM35_UCI_DEV_CTRL_STATE_READY);
	uci_dev->data_available = true;
	uci_dev->bypass_events = event;
	/* Wake up the file reader or ioctl processes. */
	wake_up_interruptible_all(&uci_dev->wait_queue);
	return 0;
}

/**
 * qm35_uci_dev_open() - Miscdevice open callback function.
 * @inode: The inode of the miscdevice.
 * @file: The file of the miscdevice to open.
 *
 * Calls qm35_bypass_open() to open the low-level hardware device and reroute
 * traffic to this miscdevice. Also install the incoming packet listener
 * qm35_uci_dev_listener() to get notified when new packet is avail.
 *
 * This function also reset current message type to %QM35_TRANSPORT_MSG_UCI.
 *
 * The access to the miscdevice is exclusive.
 *
 * Context: User context.
 * Return: 0 on success, else the error code of qm35_bypass_open().
 */
static int qm35_uci_dev_open(struct inode *inode, struct file *file)
{
	struct qm35_uci_dev *uci_dev = file_to_qm35_uci_dev(file);
	struct qm35 *qm35 = uci_dev->qm35;
	qm35_bypass_handle hnd;
	int rc = 0;

	trace_qm35_uci_dev_open(qm35);
	hnd = qm35_bypass_open(qm35, qm35_uci_dev_listener, uci_dev);
	if (IS_ERR(hnd)) {
		rc = PTR_ERR(hnd);
		goto error;
	}
	uci_dev->channel = hnd;

	/* Set default state and message type when opened. */
	qm35_uci_dev_set_state(uci_dev, QM35_UCI_DEV_CTRL_STATE_READY);
	qm35_uci_dev_set_hsspi_msg_type(uci_dev, QM35_TRANSPORT_MSG_UCI);
error:
	trace_qm35_uci_dev_open_return(qm35, rc);
	return rc;
}

/**
 * qm35_uci_dev_release() - Miscdevice release callback function.
 * @inode: The inode of the miscdevice.
 * @file: The file of the opened miscdevice to close.
 *
 * Calls qm35_bypass_close() to release the low-level hardware device and
 * allow other traffic.
 *
 * Context: User context.
 * Return: 0 on success, else the error code of qm35_bypass_close().
 */
static int qm35_uci_dev_release(struct inode *inode, struct file *file)
{
	struct qm35_uci_dev *uci_dev = file_to_qm35_uci_dev(file);
	struct qm35 *qm35 = uci_dev->qm35;
	int rc;

	trace_qm35_uci_dev_close(qm35);
	rc = qm35_bypass_close(uci_dev->channel);
	if (rc < 0)
		goto error;
	uci_dev->data_available = false;

	/* Previous call stopped the device. */
	qm35_uci_dev_set_state(uci_dev, QM35_UCI_DEV_CTRL_STATE_OFF);
error:
	trace_qm35_uci_dev_close_return(qm35, rc);
	return rc;
}

/**
 * qm35_uci_dev_read() - Miscdevice read callback function.
 * @file: The file of the opened miscdevice.
 * @buf: The buffer where the data will be copied.
 * @count: The requested data size to read.
 * @ppos: The current reading position.
 *
 * Wait for a frame (or return -EAGAIN if non blocking mode and no data waiting).
 *
 * Calls qm35_bypass_recv() to read the awaiting data and copy it to caller.
 *
 * Context: User context.
 * Return: The size of the read data on success, else a negative error code.
 */
static ssize_t qm35_uci_dev_read(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	struct qm35_uci_dev *uci_dev = file_to_qm35_uci_dev(file);
	struct qm35 *qm35 = uci_dev->qm35;
	enum qm35_transport_msg_type type;
	int flags, rc;

	trace_qm35_uci_dev_read(qm35);
	if (!(file->f_flags & O_NONBLOCK)) {
		/* Blocking read, go to sleep. */
		rc = wait_event_interruptible(uci_dev->wait_queue,
					      uci_dev->data_available);
		if (rc) {
			/* A signal has arrived. Return -ERESTARTSYS lets the VFS restart the
			 * system call or return -EINTR */
			goto error;
		}
	}
	/* Here, wake up or non blocking read, try to read data. */
	rc = qm35_bypass_recv(uci_dev->channel, buf, count, &type, &flags);
	if (rc == -EAGAIN)
		uci_dev->data_available = false;
	if (rc < 0)
		goto error;

error:
	trace_qm35_uci_dev_read_return(qm35, rc);
	return rc;
}

/**
 * qm35_uci_dev_write() - Miscdevice write callback function.
 * @file: The file of the opened miscdevice.
 * @buf: The buffer where the data will be read.
 * @count: The requested data size to write.
 * @ppos: The current writing position.
 *
 * Calls qm35_bypass_send() to send the provided data to the QM35 HW using
 * the transport API.
 *
 * Context: User context.
 * Return: The size of the written data on success, else a negative error code.
 */
static ssize_t qm35_uci_dev_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *ppos)
{
	struct qm35_uci_dev *uci_dev = file_to_qm35_uci_dev(file);
	struct qm35 *qm35 = uci_dev->qm35;
	int rc;

	trace_qm35_uci_dev_write(qm35);
	/* Check size first. */
	if (count > sizeof(uci_dev->write_buffer)) {
		rc = -ENOBUFS;
		goto error;
	}

	/* Get the data, only the UCI message, from the user mode. */
	if (copy_from_user(uci_dev->write_buffer, buf, count)) {
		rc = -EFAULT;
		goto error;
	}

	/* Send the UCI message to the device. */
	rc = qm35_bypass_send(uci_dev->channel, uci_dev->write_buffer, count);
	if (!rc)
		rc = count;
error:
	trace_qm35_uci_dev_write_return(qm35, rc);
	return rc; /* If no error, return the written byte count. */
}

/**
 * qm35_uci_dev_ioctl() - Miscdevice ioctl callback function.
 * @file: The file of the opened miscdevice.
 * @cmd: The ioctl command.
 * @args: The parameter of the command.
 *
 * Handle IOCTL from the user-space application and calls qm35_bypass_control()
 * according the IOCTL.
 *
 * Context: User context.
 * Return: 0 on success, else a negative error code.
 */
static long qm35_uci_dev_ioctl(struct file *file, unsigned int cmd,
			       unsigned long args)
{
	struct qm35_uci_dev *uci_dev = file_to_qm35_uci_dev(file);
	void __user *argp = (void __user *)args;
	int rc;
	unsigned int param;
	struct qm35_fwupload_params ext_params = {};
	long bypass_param;

	switch (cmd) {
	case QM35_CTRL_RESET:
		bypass_param = 0;
		rc = qm35_bypass_control(uci_dev->channel,
					 QM35_BYPASS_ACTION_RESET,
					 &bypass_param);
		if (rc)
			return rc;
		param = QM35_UCI_DEV_CTRL_STATE_RESET;
		qm35_uci_dev_set_state(uci_dev, param);
		return copy_to_user(argp, &param, sizeof(param)) ? -EFAULT : 0;

	case QM35_CTRL_RESET_EXT:
		if (copy_from_user(&param, argp, sizeof(param)))
			return -EFAULT;
		bypass_param = param;
		rc = qm35_bypass_control(uci_dev->channel,
					 QM35_BYPASS_ACTION_RESET,
					 &bypass_param);
		qm35_uci_dev_set_state(uci_dev, QM35_UCI_DEV_CTRL_STATE_RESET);
		return rc;

	case QM35_CTRL_GET_STATE:
		param = qm35_uci_dev_get_state(uci_dev);
		return copy_to_user(argp, &param, sizeof(param)) ? -EFAULT : 0;

	case QM35_CTRL_FW_UPLOAD:
		qm35_uci_dev_set_state(uci_dev,
				       QM35_UCI_DEV_CTRL_STATE_FW_DOWNLOADING);
		rc = qm35_bypass_control(uci_dev->channel,
					 QM35_BYPASS_ACTION_FWUPD, NULL);
		param = QM35_UCI_DEV_CTRL_STATE_RESET;
		qm35_uci_dev_set_state(uci_dev, param);
		return copy_to_user(argp, &param, sizeof(param)) ? -EFAULT : 0;

	case QM35_CTRL_FW_UPLOAD_EXT:
		if (copy_from_user(&ext_params, argp, sizeof(ext_params)))
			return -EFAULT;
		qm35_uci_dev_set_state(uci_dev,
				       QM35_UCI_DEV_CTRL_STATE_FW_DOWNLOADING);
		rc = qm35_bypass_control(uci_dev->channel,
					 QM35_BYPASS_ACTION_FWUPD,
					 (long *)ext_params.fw_name);
		qm35_uci_dev_set_state(uci_dev, QM35_UCI_DEV_CTRL_STATE_RESET);
		return rc;

	case QM35_CTRL_POWER:
		if (copy_from_user(&param, argp, sizeof(param)))
			return -EFAULT;
		if (param > 1)
			return -EINVAL;
		bypass_param = param;
		rc = qm35_bypass_control(uci_dev->channel,
					 QM35_BYPASS_ACTION_POWER,
					 &bypass_param);
		qm35_uci_dev_set_state(uci_dev,
				       param ? QM35_UCI_DEV_CTRL_STATE_RESET :
					       QM35_UCI_DEV_CTRL_STATE_OFF);
		return rc;

	case QM35_CTRL_SET_STATE:
		if (copy_from_user(&param, argp, sizeof(param)))
			return -EFAULT;
		qm35_uci_dev_set_state(uci_dev, param);
		return 0;

	case QM35_CTRL_GET_TYPE:
		param = qm35_uci_dev_get_hsspi_msg_type(uci_dev);
		return copy_to_user(argp, &param, sizeof(param)) ? -EFAULT : 0;

	case QM35_CTRL_SET_TYPE:
		if (copy_from_user(&param, argp, sizeof(param)))
			return -EFAULT;
		qm35_uci_dev_set_hsspi_msg_type(uci_dev, param);
		return 0;

	case QM35_CTRL_WAIT_EVENT:
		/* Blocking read, go to sleep. */
		rc = wait_event_interruptible(uci_dev->wait_queue,
					      uci_dev->data_available);
		if (rc)
			/* A signal has arrived. Return -ERESTARTSYS lets the VFS restart the
			 * system call or return -EINTR */
			return rc;
		return copy_to_user(argp, &uci_dev->bypass_events,
				    sizeof(uci_dev->bypass_events)) ?
			       -EFAULT :
			       0;

	default:
		return -EINVAL;
	}
}

/**
 * qm35_uci_dev_poll() - Miscdevice poll callback function.
 * @file: The file of the opened miscdevice.
 * @wait: The poll_table_struct struct.
 *
 * Allow ``poll()`` / ``select()`` syscalls.
 *
 * Context: User context.
 * Return: A poll event.
 */
static __poll_t qm35_uci_dev_poll(struct file *file,
				  struct poll_table_struct *wait)
{
	struct qm35_uci_dev *uci_dev = file_to_qm35_uci_dev(file);
	struct qm35 *qm35 = uci_dev->qm35;
	__poll_t mask = EPOLLOUT | EPOLLWRNORM; // Can always write.

	trace_qm35_uci_dev_poll(qm35);
	poll_wait(file, &uci_dev->wait_queue, wait);

	if (qm35_bypass_queue_check(uci_dev->channel) > 0)
		mask |= EPOLLIN;
	trace_qm35_uci_dev_poll_return(qm35, mask);

	return mask;
}

static const struct file_operations qm35_uci_dev_fops = {
	.owner = THIS_MODULE,
	.open = qm35_uci_dev_open,
	.release = qm35_uci_dev_release,
	.read = qm35_uci_dev_read,
	.write = qm35_uci_dev_write,
	.unlocked_ioctl = qm35_uci_dev_ioctl,
	.poll = qm35_uci_dev_poll,
};

static struct list_head uci_devs_list = LIST_HEAD_INIT(uci_devs_list);

/**
 * qm35_uci_dev_misc_register() - Register a miscdevice.
 * @qm35: The associated qm35 device to this miscdevice.
 *
 * It calls misc_register() to create the ``/dev/uciX`` misc device associated
 * to the newly created QM35 device.
 *
 * Context: Kernel thread context.
 * Return: 0 on success, else misc_register() error code.
 */
static int qm35_uci_dev_misc_register(struct qm35 *qm35)
{
	struct device *dev = qm35_get_device(qm35);
	struct qm35_uci_dev *uci_dev;
	int rc;

	uci_dev = kzalloc(sizeof(struct qm35_uci_dev), GFP_KERNEL);
	if (!uci_dev) {
		dev_err(dev, "Cannot allocate memory for UCI dev\n");
		return -ENOMEM;
	}

	/* Get the dev_id for this instance. */
	rc = qm35_get_dev_id(qm35);
	if (rc < 0) {
		dev_err(dev, "Cannot get the device id\n");
		goto error_free;
	}
	snprintf(uci_dev->name, QM35_UCI_DEV_DEVICE_NAME_SIZE, "%s%u",
		 QM35_UCI_DEV_DEVICE_NAME, rc);
	uci_dev->miscdev.name = uci_dev->name;
	uci_dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	uci_dev->miscdev.fops = &qm35_uci_dev_fops;
	uci_dev->miscdev.parent = dev;
	uci_dev->qm35 = qm35;
	rc = misc_register(&uci_dev->miscdev);
	if (rc) {
		dev_err(dev, "Unable to register misc device %s\n",
			uci_dev->miscdev.name);
		goto error_free;
	}
	init_waitqueue_head(&uci_dev->wait_queue);
	list_add_tail(&uci_dev->dev_list, &uci_devs_list);
	return 0;

error_free:
	kfree(uci_dev);
	return rc;
}

/**
 * qm35_uci_dev_misc_deregister() - Deregister a miscdevice.
 * @qm35: The associated qm35 device to this miscdevice.
 *
 * It calls ``misc_deregister()`` to destroy the misc device associated to the
 * QM35 device.
 *
 * Context: Kernel thread context.
 * Return: void.
 */
static void qm35_uci_dev_misc_deregister(struct qm35 *qm35)
{
	struct qm35_uci_dev *cur, *n;

	list_for_each_entry_safe (cur, n, &uci_devs_list, dev_list) {
		if (cur->qm35 == qm35) {
			misc_deregister(&cur->miscdev);
			list_del(&cur->dev_list);
			kfree(cur);
			break;
		}
	}
}

/**
 * qm35_uci_dev_notifier() - notifier_block callback function.
 * @nb: The notifier_block.
 * @action: The notifier event.
 * @data: The data provided by the notifier.
 *
 * This notifier callback function handles the create/destroy of the misc device
 * according the new/online/delete event from QM35 core.
 *
 * It calls qm35_uci_dev_misc_register() to create the misc device when ONLINE
 * event is received.
 *
 * It calls qm35_uci_dev_misc_deregister() to destroy the misc device when DELETE
 * event is received.
 *
 * Context: Kernel thread context.
 * Return: 0 on success, else -EINVAL if @data is NULL or if the @action is
 * unknown, -EBUSY if the bypass channel is opened on delete event , else the
 * qm35_uci_dev_misc_register() error code.
 */
static int qm35_uci_dev_notifier(struct notifier_block *nb,
				 unsigned long action, void *data)
{
	enum qm35_notifier_events event = action;
	struct qm35 *qm35 = data;
	int rc = 0;

	switch (event) {
	case QM35_NOTIFIER_EVENT_NEW:
		/* Ignore this message as all is done in ONLINE event case. */
		break;
	case QM35_NOTIFIER_EVENT_ONLINE:
		/* Ignore the return value. We don't want to stop notifier chain. */
		qm35_uci_dev_misc_register(qm35);
		break;
	case QM35_NOTIFIER_EVENT_DELETE:
		/* An instance of QM35 cannot be removed if this char device is
		 * opened because opening it increase its module usage counter.
		 * So just de-register normally here. */
		qm35_uci_dev_misc_deregister(qm35);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return notifier_from_errno(rc);
}

static struct notifier_block nb = {
	.notifier_call = qm35_uci_dev_notifier,
	.next = NULL,
	.priority = 0,
};

/**
 * qm35_uci_dev_init() - Module init function.
 *
 * Calls ``qm35_register_notifier(struct notifier_block *nb)`` to install a
 * ``notifier_block`` with a callback to handle QM35 new and delete events.
 *
 * Context: User context.
 * Return: 0 on success, else qm35_register_notifier() error code.
 */
static int qm35_uci_dev_init(void)
{
	return qm35_register_notifier(&nb);
}

/**
 * qm35_uci_dev_exit() - Module exit function.
 *
 * Remove all created misc devices and unregister the ``notifier_block``.
 *
 * Context: User context.
 * Return: 0 on success, else qm35_unregister_notifier() error code.
 */
static int qm35_uci_dev_exit(void)
{
	struct qm35_uci_dev *cur, *n;

	list_for_each_entry_safe (cur, n, &uci_devs_list, dev_list) {
		misc_deregister(&cur->miscdev);
		list_del(&cur->dev_list);
		kfree(cur);
	}
	return qm35_unregister_notifier(&nb);
}

#ifndef QM35_UCI_DEV_TESTS

static u32 debug_flags = 0;
module_param(debug_flags, uint, 0660);

static int __init qm35_uci_dev_module_init(void)
{
#ifdef CONFIG_EVENT_TRACING
	if (debug_flags & 1)
		trace_set_clr_event(THIS_MODULE->name, NULL, 1);
#endif
	return qm35_uci_dev_init();
}

static void __exit qm35_uci_dev_module_exit(void)
{
#ifdef CONFIG_EVENT_TRACING
	if (debug_flags & 1)
		trace_set_clr_event(THIS_MODULE->name, NULL, 0);
#endif
	qm35_uci_dev_exit();
}

module_init(qm35_uci_dev_module_init);
module_exit(qm35_uci_dev_module_exit);

#ifdef GITVERSION
MODULE_VERSION(GITVERSION);
#endif
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Yvan Roch <yvan.roch@qorvo.com>");
MODULE_DESCRIPTION("Qorvo QM35 UCI pass thru driver");

#endif /* !QM35_UCI_DEV_TESTS */
