/*
 * FBUI input event snagging
 * Code for relaying input events to FBUI.
 *
 * Modification for FBUI by Zachary Smith, copyright (C) 2004.
 * Based on evbug, which is copyright (c) 1999-2001 Vojtech Pavlik
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <fbui@comcast.net>.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/spinlock.h>



MODULE_AUTHOR("Zachary Smith <fbui@comcast.net>");
MODULE_DESCRIPTION("Input-driver-event besnagger module");
MODULE_LICENSE("GPL");


#ifdef CONFIG_INPUT_FBUI

static char fbui_input_name[] = "fbui_input";

static spinlock_t mylock = SPIN_LOCK_UNLOCKED;

static FBUIInputEventHandler *handler = NULL;
static u32 handlerparam = 0;


void fbui_input_register_handler (FBUIInputEventHandler *h, u32 param)
{
	if (h) {
		handler = h;
		handlerparam = param;
	}
}

static void fbui_input_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	unsigned long flags;

	// printk(KERN_INFO "fbui_input.c: Event. Dev: %s, Type: %d, Code: %d, Value: %d\n", handle->dev->phys, type, code, value);

	if (!handler)
		return;

	spin_lock_irqsave(&mylock, flags);

	struct input_event e;
	e.type = type;
	e.code = code;
	e.value= value;
	do_gettimeofday(&e.time);
	(handler) (handlerparam, &e);

	spin_unlock_irqrestore(&mylock, flags);
}

static struct input_handle *fbui_input_connect(struct input_handler *handler, struct input_dev *dev, struct input_device_id *id)
{
	struct input_handle *handle;

	if (!(handle = kmalloc(sizeof(struct input_handle), GFP_KERNEL)))
		return NULL;
	memset(handle, 0, sizeof(struct input_handle));

	handle->dev = dev;
	handle->handler = handler;
	handle->name = fbui_input_name;

	input_open_device(handle);

	return handle;
}

static void fbui_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	kfree(handle);
}

static struct input_device_id fbui_input_ids[] = {
	{ .driver_info = 1 },	/* Matches all devices */
	{ },			/* Terminating zero entry */
};

MODULE_DEVICE_TABLE(input, fbui_input_ids);

static struct input_handler fbui_input_handler = {
	.event =	fbui_input_event,
	.connect =	fbui_input_connect,
	.disconnect =	fbui_input_disconnect,
	.name =		"fbui_input",
	.id_table =	fbui_input_ids,
};

int __init fbui_input_init(void)
{
	handler = NULL;

	printk(KERN_INFO "FBUI input module: initializing\n");

	input_register_handler(&fbui_input_handler);
	return 0;
}

void __exit fbui_input_exit(void)
{
	handler = NULL;

	input_unregister_handler(&fbui_input_handler);
}

EXPORT_SYMBOL(fbui_input_register_handler);

module_init(fbui_input_init);
module_exit(fbui_input_exit);

#endif
