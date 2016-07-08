/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/kfifo.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/ioctl.h>
#include <linux/errno.h>
#include <linux/poll.h>

#define DEV_CLASS_NAME "rpmsg_piksi"
#define DEV_CLASS_MAX_MINORS 10

#define CHANNEL_NAME "piksi"
#define NUM_ENDPOINTS 3

#define RPMSG_BUFF_SIZE_MAX 512
#define RX_FIFO_SIZE (32 * RPMSG_BUFF_SIZE_MAX)
#define TX_BUFF_SIZE (RPMSG_BUFF_SIZE_MAX)

#define IOCTL_CMD_GET_KFIFO_SIZE      1
#define IOCTL_CMD_GET_AVAIL_DATA_SIZE 2
#define IOCTL_CMD_GET_FREE_BUFF_SIZE  3

static const u32 endpoint_addr_config[NUM_ENDPOINTS] = {
  100,
  101,
  102
};

struct ept_params {
  int dev_minor;
  u32 addr;
  struct cdev cdev;
  struct device *rpmsg_dev;
  struct rpmsg_channel *rpmsg_chnl;
  struct rpmsg_endpoint *rpmsg_ept;
  wait_queue_head_t usr_wait_q;
  struct mutex rx_lock;
  STRUCT_KFIFO_REC_2(RX_FIFO_SIZE) rx_fifo;
  char tx_buff[TX_BUFF_SIZE];
};

struct dev_params {
  struct ept_params epts[NUM_ENDPOINTS];
};

static struct class *dev_class;
static int dev_major;
static int dev_minor_next = 0;

static int ept_cdev_open(struct inode *inode, struct file *p_file)
{
  /* Initialize file descriptor with pointer to associated endpoint params */
  struct ept_params *ept_params = container_of(inode->i_cdev,
                                               struct ept_params, cdev);
  p_file->private_data = ept_params;
  return 0;
}

static ssize_t ept_cdev_write(struct file *p_file, const char __user *ubuff,
                              size_t len, loff_t *p_off)
{
  struct ept_params *ept_params = p_file->private_data;
  int err;
  unsigned int size;

  if (len < sizeof(ept_params->tx_buff)) {
    size = len;
  } else {
    size = sizeof(ept_params->tx_buff);
  }

  if (copy_from_user(ept_params->tx_buff, ubuff, size)) {
    dev_err(ept_params->rpmsg_dev, "user to kernel buff copy error.\n");
    return -1;
  }

  /* TODO: support non-blocking write */
  err = rpmsg_sendto(ept_params->rpmsg_chnl, ept_params->tx_buff,
                     size, ept_params->addr);

  if (err) {
    dev_err(ept_params->rpmsg_dev, "rpmsg_sendto (size = %d) error: %d\n",
            size, err);
    size = 0;
  }

  return size;
}

static ssize_t ept_cdev_read(struct file *p_file, char __user *ubuff,
                             size_t len, loff_t *p_off)
{
  struct ept_params *ept_params = p_file->private_data;
  int retval;
  unsigned int bytes_copied;

  /* Acquire lock */
  retval = mutex_lock_interruptible(&ept_params->rx_lock);
  if (retval) {
    return retval;
  }

  while (kfifo_is_empty(&ept_params->rx_fifo)) {
    mutex_unlock(&ept_params->rx_lock);

    /* If non-blocking read is requested return error */
    if (p_file->f_flags & O_NONBLOCK) {
      return -EAGAIN;
    }

    /* Block the calling context until data becomes available */
    wait_event_interruptible(ept_params->usr_wait_q,
                             !kfifo_is_empty(&ept_params->rx_fifo));

    /* Acquire lock */
    retval = mutex_lock_interruptible(&ept_params->rx_lock);
    if (retval) {
      return retval;
    }
  }

  /* Provide requested data size to user space */
  retval = kfifo_to_user(&ept_params->rx_fifo, ubuff, len, &bytes_copied);

  mutex_unlock(&ept_params->rx_lock);

  return retval ? retval : bytes_copied;
}

static unsigned int ept_cdev_poll(struct file *p_file,
                                  struct poll_table_struct *poll_table)
{
  struct ept_params *ept_params = p_file->private_data;
  unsigned int result = 0;

  poll_wait(p_file, &ept_params->usr_wait_q, poll_table);

  if (!kfifo_is_empty(&ept_params->rx_fifo)) {
    result |= POLLIN | POLLRDNORM;
  }

  /* Non-blocking write is not supported yet */
  result |= POLLOUT | POLLWRNORM;

  return result;
}

static long ept_cdev_ioctl(struct file *p_file, unsigned int cmd,
                           unsigned long arg)
{
  struct ept_params *ept_params = p_file->private_data;
  unsigned int tmp;

  switch (cmd) {
    case IOCTL_CMD_GET_KFIFO_SIZE: {
      tmp = kfifo_size(&ept_params->rx_fifo);
      if (copy_to_user((unsigned int *)arg, &tmp, sizeof(int))) {
        return -EACCES;
      }
    }
    break;

    case IOCTL_CMD_GET_AVAIL_DATA_SIZE: {
      tmp = kfifo_len(&ept_params->rx_fifo);
      if (copy_to_user((unsigned int *)arg, &tmp, sizeof(int))) {
        return -EACCES;
      }
    }
    break;

    case IOCTL_CMD_GET_FREE_BUFF_SIZE: {
      tmp = kfifo_avail(&ept_params->rx_fifo);
      if (copy_to_user((unsigned int *)arg, &tmp, sizeof(int))) {
        return -EACCES;
      }
    }
    break;

    default: {
      return -EINVAL;
    }
  }

  return 0;
}

static int ept_cdev_release(struct inode *inode, struct file *p_file)
{
  return 0;
}

static const struct file_operations ept_cdev_fops = {
  .owner = THIS_MODULE,
  .read = ept_cdev_read,
  .write = ept_cdev_write,
  .poll = ept_cdev_poll,
  .open = ept_cdev_open,
  .unlocked_ioctl = ept_cdev_ioctl,
  .release = ept_cdev_release,
};

static void ept_rpmsg_default_cb(struct rpmsg_channel *rpdev, void *data,
                                 int len, void *priv, u32 src)
{

}

static void ept_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
                         int len, void *priv, u32 src)
{
  struct ept_params *ept_params = priv;
  int len_in;

  /* Do not write zero-length records to the FIFO, as this would
   * cause read() to return zero, aka EOF */
  if (len == 0) {
    dev_info(&rpdev->dev, "Dropping zero-length message.\n");
    return;
  }

  len_in = kfifo_in(&ept_params->rx_fifo, data, (unsigned int)len);
  if (len_in != len) {
    /* There was no space for incoming data */
    return;
  }

  /* Wake up any blocking contexts waiting for data */
  wake_up_interruptible(&ept_params->usr_wait_q);
}

static int drv_probe(struct rpmsg_channel *rpdev);
static void drv_remove(struct rpmsg_channel *rpdev);

static const struct rpmsg_device_id rpmsg_dev_id_table[] = {
  {.name = CHANNEL_NAME},
  {},
};

static struct rpmsg_driver rpmsg_driver = {
  .drv.name = KBUILD_MODNAME,
  .drv.owner = THIS_MODULE,
  .id_table = rpmsg_dev_id_table,
  .probe = drv_probe,
  .remove = drv_remove,
  .callback = ept_rpmsg_default_cb,
};

static int ept_setup(struct ept_params *ept_params,
                     struct rpmsg_channel *rpdev, u32 addr)
{
  /* Initialize mutex */
  mutex_init(&ept_params->rx_lock);

  /* Initialize wait queue head that provides blocking rx for userspace */
  init_waitqueue_head(&ept_params->usr_wait_q);

  /* Initialize kfifo for RX */
  INIT_KFIFO(ept_params->rx_fifo);

  ept_params->rpmsg_chnl = rpdev;
  ept_params->addr = addr;

  /* Create RPMSG endpoint */
  ept_params->rpmsg_ept = rpmsg_create_ept(ept_params->rpmsg_chnl, ept_rpmsg_cb,
                                           ept_params, ept_params->addr);
  if (!ept_params->rpmsg_ept) {
    dev_err(&rpdev->dev, "Failed to create rpmsg endpoint.\n");
    goto error0;
  }

  /* Get device minor number */
  if (dev_minor_next < DEV_CLASS_MAX_MINORS) {
    ept_params->dev_minor = dev_minor_next++;
  } else {
    dev_err(&rpdev->dev, "Minor file number %d exceeded the max minors %d.\n",
            dev_minor_next, DEV_CLASS_MAX_MINORS);
    goto error1;
  }

  /* Initialize character device */
  cdev_init(&ept_params->cdev, &ept_cdev_fops);
  ept_params->cdev.owner = THIS_MODULE;
  if (cdev_add(&ept_params->cdev, MKDEV(dev_major, ept_params->dev_minor), 1)) {
    dev_err(&rpdev->dev, "Failed to add character device.\n");
    goto error1;
  }

  /* Create device */
  ept_params->rpmsg_dev =
      device_create(dev_class, &rpdev->dev,
                    MKDEV(dev_major, ept_params->dev_minor), NULL,
                    DEV_CLASS_NAME "%u", ept_params->dev_minor);
  if (ept_params->rpmsg_dev == NULL) {
    dev_err(&rpdev->dev, "Failed to create device.\n");
    goto error2;
  }

  goto out;

error2:
  cdev_del(&ept_params->cdev);
error1:
  rpmsg_destroy_ept(ept_params->rpmsg_ept);
error0:
  return -ENODEV;
out:
  return 0;
}

static void ept_remove(struct ept_params *ept_params)
{
  device_destroy(dev_class, MKDEV(dev_major, ept_params->dev_minor));
  rpmsg_destroy_ept(ept_params->rpmsg_ept);
  cdev_del(&ept_params->cdev);
}

static void startup_message_send(struct rpmsg_channel *rpdev)
{
  char msg[] = "startup";
  rpmsg_send(rpdev, msg, strlen(msg));
}

static int drv_probe(struct rpmsg_channel *rpdev)
{
  struct dev_params *dev_params;
  int status;
  int i;

  dev_params = devm_kzalloc(&rpdev->dev, sizeof(struct dev_params), GFP_KERNEL);
  if (!dev_params) {
    dev_err(&rpdev->dev, "Failed to allocate memory for device.\n");
    return -ENOMEM;
  }
  memset(dev_params, 0x0, sizeof(struct dev_params));

  dev_set_drvdata(&rpdev->dev, dev_params);

  for (i=0; i<NUM_ENDPOINTS; i++) {
    status = ept_setup(&dev_params->epts[i], rpdev, endpoint_addr_config[i]);
    if (status) {
      /* Remove any endpoints that were successfully set up */
      while (--i >= 0) {
        ept_remove(&dev_params->epts[i]);
      }
      return -ENODEV;
    }
  }

  startup_message_send(rpdev);

  return 0;
}

static void drv_remove(struct rpmsg_channel *rpdev)
{
  struct dev_params *dev_params = dev_get_drvdata(&rpdev->dev);
  int i;

  for (i=0; i<NUM_ENDPOINTS; i++) {
    ept_remove(&dev_params->epts[i]);
  }
}

static int __init init(void)
{
  dev_t dev;

  /* Create device class for this device */
  dev_class = class_create(THIS_MODULE, DEV_CLASS_NAME);

  if (dev_class == NULL) {
    printk(KERN_ERR "Failed to register " DEV_CLASS_NAME " class.\n");
    return -1;
  }

  /* Allocate character device region for this driver */
  if (alloc_chrdev_region(&dev, 0, DEV_CLASS_MAX_MINORS, DEV_CLASS_NAME) < 0) {
    printk(KERN_ERR "Failed to allocate character device region for "
           DEV_CLASS_NAME ".\n");
    class_destroy(dev_class);
    return -1;
  }

  dev_major = MAJOR(dev);
  return register_rpmsg_driver(&rpmsg_driver);
}

static void __exit fini(void)
{
  unregister_rpmsg_driver(&rpmsg_driver);
  unregister_chrdev_region(MKDEV(dev_major, 0), DEV_CLASS_MAX_MINORS);
  class_destroy(dev_class);
}

module_init(init);
module_exit(fini);

MODULE_DESCRIPTION("RPMSG Piksi driver");
MODULE_LICENSE("GPL v2");
