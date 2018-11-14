/*
 * Copyright (C) 2016-2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
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

/* rpmsg_piksi driver
 * - character devices are created on module init
 * - rpmsg endpoints are created and attached when probed by rpmsg bus
 * - if rpmsg is not attached:
 *     - character device reads block
 *     - character device writes silently drop data
 */

#define DEV_CLASS_NAME "rpmsg_piksi"
#define CHANNEL_NAME "piksi"
#define NUM_ENDPOINTS 3

#define RPMSG_DATA_SIZE_MAX (512 - 16)
#define RX_FIFO_SIZE (16 * 1024)
#define TX_BUFF_SIZE (RPMSG_DATA_SIZE_MAX)

#define MAX_BUF_SUBMIT (4096)

// clang-format off
#define IOCTL_CMD_GET_KFIFO_SIZE      1
#define IOCTL_CMD_GET_AVAIL_DATA_SIZE 2
#define IOCTL_CMD_GET_FREE_BUFF_SIZE  3
// clang-format on

static const u32 endpoint_addr_config[NUM_ENDPOINTS] = {
  100,
  101,
  102,
};

struct ept_params {
  dev_t dev;
  u32 addr;
  struct cdev cdev;
  struct device *device;
  /* mutex used to protect rx_fifo and rx_wait_queue */
  struct mutex rx_fifo_lock;
  wait_queue_head_t rx_wait_queue;
  STRUCT_KFIFO_REC_2(RX_FIFO_SIZE) rx_fifo;
  /* mutex used to protect tx_buff and rpmsg parameters */
  struct mutex tx_rpmsg_lock;
  char tx_buff[TX_BUFF_SIZE];
  /* rpmsg parameters */
  struct rpmsg_channel *rpmsg_chnl;
  struct rpmsg_endpoint *rpmsg_ept;
  bool rpmsg_ready;
};

struct dev_params {
  struct ept_params epts[NUM_ENDPOINTS];
};

static bool probed = false;
static struct class *dev_class = NULL;
static dev_t dev_start;
static struct dev_params *dev_params = NULL;

static int ept_cdev_open(struct inode *inode, struct file *p_file)
{
  /* Initialize file descriptor with pointer to associated endpoint params */
  struct ept_params *ept_params = container_of(inode->i_cdev, struct ept_params, cdev);
  p_file->private_data = ept_params;
  return 0;
}

static ssize_t ept_cdev_write(struct file *p_file,
                              const char __user *ubuff,
                              size_t len,
                              loff_t *p_off)
{
  struct ept_params *ept_params = p_file->private_data;
  unsigned int size;
  ssize_t retval;
  size_t offset = 0;

  /* Acquire TX / rpmsg lock */
  retval = mutex_lock_interruptible(&ept_params->tx_rpmsg_lock);
  if (retval) {
    return retval;
  }

  /* If rpmsg is not attached, drop the data and return success */
  if (!ept_params->rpmsg_ready) {
    retval = len;
    goto done_locked;
  }

  while (len > 0) {

    if (len <= sizeof(ept_params->tx_buff)) {
      size = len;
    } else {
      size = sizeof(ept_params->tx_buff);
    }

    if (offset + size > MAX_BUF_SUBMIT) {
      break;
    }

    if (copy_from_user(ept_params->tx_buff, &ubuff[offset], size)) {
      dev_err(ept_params->device, "User to kernel buff copy error.\n");
      retval = -1;
      goto done_locked;
    }

    /* TODO: support non-blocking write */
    retval = rpmsg_sendto(ept_params->rpmsg_chnl, ept_params->tx_buff, size, ept_params->addr);

    if (retval) {
      dev_err(ept_params->device, "rpmsg_sendto (size = %d) error: %d\n", size, retval);
      goto done_locked;
    }

    len -= size;
    offset += size;
  }

  if (len != 0) {
    dev_err(ept_params->device, "rpmsg truncated (len = %d)\n", offset);
  }

  retval = offset;

done_locked:
  mutex_unlock(&ept_params->tx_rpmsg_lock);
  return retval;
}

static ssize_t ept_cdev_read(struct file *p_file, char __user *ubuff, size_t len, loff_t *p_off)
{
  struct ept_params *ept_params = p_file->private_data;
  ssize_t retval;
  unsigned int bytes_copied;

  /* Acquire RX lock */
  retval = mutex_lock_interruptible(&ept_params->rx_fifo_lock);
  if (retval) {
    return retval;
  }

  while (kfifo_is_empty(&ept_params->rx_fifo)) {
    mutex_unlock(&ept_params->rx_fifo_lock);

    /* If non-blocking read is requested return error */
    if (p_file->f_flags & O_NONBLOCK) {
      return -EAGAIN;
    }

    /* Block the calling context until data becomes available */
    retval =
      wait_event_interruptible(ept_params->rx_wait_queue, !kfifo_is_empty(&ept_params->rx_fifo));
    if (retval) {
      return retval;
    }

    /* Acquire RX lock */
    retval = mutex_lock_interruptible(&ept_params->rx_fifo_lock);
    if (retval) {
      return retval;
    }
  }

  /* Provide requested data size to user space */
  retval = kfifo_to_user(&ept_params->rx_fifo, ubuff, len, &bytes_copied);

  mutex_unlock(&ept_params->rx_fifo_lock);

  return retval ? retval : bytes_copied;
}

static unsigned int ept_cdev_poll(struct file *p_file, struct poll_table_struct *poll_table)
{
  struct ept_params *ept_params = p_file->private_data;
  unsigned int result = 0;

  poll_wait(p_file, &ept_params->rx_wait_queue, poll_table);

  if (!kfifo_is_empty(&ept_params->rx_fifo)) {
    result |= POLLIN | POLLRDNORM;
  }

  /* Non-blocking write is not supported yet */
  result |= POLLOUT | POLLWRNORM;

  return result;
}

static long ept_cdev_ioctl(struct file *p_file, unsigned int cmd, unsigned long arg)
{
  struct ept_params *ept_params = p_file->private_data;
  unsigned int tmp;

  switch (cmd) {
  case IOCTL_CMD_GET_KFIFO_SIZE: {
    tmp = kfifo_size(&ept_params->rx_fifo);
    if (copy_to_user((unsigned int *)arg, &tmp, sizeof(int))) {
      return -EACCES;
    }
  } break;

  case IOCTL_CMD_GET_AVAIL_DATA_SIZE: {
    tmp = kfifo_len(&ept_params->rx_fifo);
    if (copy_to_user((unsigned int *)arg, &tmp, sizeof(int))) {
      return -EACCES;
    }
  } break;

  case IOCTL_CMD_GET_FREE_BUFF_SIZE: {
    tmp = kfifo_avail(&ept_params->rx_fifo);
    if (copy_to_user((unsigned int *)arg, &tmp, sizeof(int))) {
      return -EACCES;
    }
  } break;

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

static void ept_rpmsg_default_cb(struct rpmsg_channel *rpdev,
                                 void *data,
                                 int len,
                                 void *priv,
                                 u32 src)
{
}

static void ept_rpmsg_cb(struct rpmsg_channel *rpdev, void *data, int len, void *priv, u32 src)
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
  wake_up_interruptible(&ept_params->rx_wait_queue);
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

static int ept_rpmsg_setup(struct ept_params *ept_params, struct rpmsg_channel *rpdev)
{
  int retval;

  /* Acquire TX / rpmsg lock */
  retval = mutex_lock_interruptible(&ept_params->tx_rpmsg_lock);
  if (retval) {
    return retval;
  }

  ept_params->rpmsg_chnl = rpdev;

  /* Create rpmsg endpoint */
  ept_params->rpmsg_ept =
    rpmsg_create_ept(ept_params->rpmsg_chnl, ept_rpmsg_cb, ept_params, ept_params->addr);
  if (ept_params->rpmsg_ept == NULL) {
    dev_err(&rpdev->dev, "Failed to create rpmsg endpoint.\n");
    retval = -ENODEV;
    goto done_locked;
  }

  ept_params->rpmsg_ready = true;
  retval = 0;

done_locked:
  mutex_unlock(&ept_params->tx_rpmsg_lock);
  return retval;
}

static void ept_rpmsg_remove(struct ept_params *ept_params)
{
  /* Acquire TX / rpmsg lock */
  mutex_lock(&ept_params->tx_rpmsg_lock);

  ept_params->rpmsg_ready = false;
  rpmsg_destroy_ept(ept_params->rpmsg_ept);

  mutex_unlock(&ept_params->tx_rpmsg_lock);
}

static int ept_cdev_setup(struct ept_params *ept_params, dev_t dev, u32 addr)
{
  ept_params->dev = dev;
  ept_params->addr = addr;
  ept_params->rpmsg_ready = false;

  /* Initialize mutexes */
  mutex_init(&ept_params->rx_fifo_lock);
  mutex_init(&ept_params->tx_rpmsg_lock);

  /* Initialize wait queue head that provides blocking RX for userspace */
  init_waitqueue_head(&ept_params->rx_wait_queue);

  /* Initialize kfifo for RX */
  INIT_KFIFO(ept_params->rx_fifo);

  /* Initialize character device */
  cdev_init(&ept_params->cdev, &ept_cdev_fops);
  ept_params->cdev.owner = THIS_MODULE;
  if (cdev_add(&ept_params->cdev, ept_params->dev, 1)) {
    printk(KERN_ERR "Failed to add character device.\n");
    goto error0;
  }

  /* Create device */
  ept_params->device =
    device_create(dev_class, NULL, ept_params->dev, NULL, DEV_CLASS_NAME "%u", ept_params->addr);
  if (ept_params->device == NULL) {
    printk(KERN_ERR "Failed to create device.\n");
    goto error1;
  }

  goto out;

error1:
  cdev_del(&ept_params->cdev);
error0:
  return -ENODEV;
out:
  return 0;
}

static void ept_cdev_remove(struct ept_params *ept_params)
{
  device_destroy(dev_class, ept_params->dev);
  cdev_del(&ept_params->cdev);
}

static void ept_cdevs_remove(struct dev_params *dev_params)
{
  int i;

  for (i = 0; i < NUM_ENDPOINTS; i++) {
    ept_cdev_remove(&dev_params->epts[i]);
  }
}

static void startup_message_send(struct rpmsg_channel *rpdev)
{
  char msg[] = "startup";
  rpmsg_send(rpdev, msg, strlen(msg));
}

static int drv_probe(struct rpmsg_channel *rpdev)
{
  int i;
  int status;

  if (probed) {
    dev_err(&rpdev->dev, "Already attached.\n");
    return -ENODEV;
  }
  probed = true;

  dev_set_drvdata(&rpdev->dev, dev_params);

  /* Create and attach rpmsg endpoints */
  for (i = 0; i < NUM_ENDPOINTS; i++) {
    status = ept_rpmsg_setup(&dev_params->epts[i], rpdev);
    if (status) {
      /* Remove any endpoints that were successfully set up */
      while (--i >= 0) {
        ept_rpmsg_remove(&dev_params->epts[i]);
      }
      return -ENODEV;
    }
  }

  startup_message_send(rpdev);

  return 0;
}

static void drv_remove(struct rpmsg_channel *rpdev)
{
  int i;
  struct dev_params *dev_params = dev_get_drvdata(&rpdev->dev);

  for (i = 0; i < NUM_ENDPOINTS; i++) {
    ept_rpmsg_remove(&dev_params->epts[i]);
  }

  probed = false;
}

static int __init init(void)
{
  int i;
  int status;

  /* Initialize device params structure */
  dev_params = kzalloc(sizeof(struct dev_params), GFP_KERNEL);
  if (dev_params == NULL) {
    printk(KERN_ERR "Failed to allocate memory for device.\n");
    goto error0;
  }

  /* Create device class for this device */
  dev_class = class_create(THIS_MODULE, DEV_CLASS_NAME);
  if (dev_class == NULL) {
    printk(KERN_ERR "Failed to register " DEV_CLASS_NAME " class.\n");
    goto error1;
  }

  /* Allocate character device region for this driver */
  if (alloc_chrdev_region(&dev_start, 0, NUM_ENDPOINTS, DEV_CLASS_NAME)) {
    printk(KERN_ERR "Failed to allocate character device region for " DEV_CLASS_NAME ".\n");
    goto error2;
  }

  /* Create character devices */
  for (i = 0; i < NUM_ENDPOINTS; i++) {
    status = ept_cdev_setup(&dev_params->epts[i],
                            MKDEV(MAJOR(dev_start), MINOR(dev_start) + i),
                            endpoint_addr_config[i]);
    if (status) {
      /* Remove any character devices that were successfully set up */
      while (--i >= 0) {
        ept_cdev_remove(&dev_params->epts[i]);
      }
      goto error3;
    }
  }

  /* Register rpmsg driver */
  if (register_rpmsg_driver(&rpmsg_driver)) {
    printk(KERN_ERR "Failed to register rpmsg driver.\n");
    goto error4;
  }

  goto out;

error4:
  ept_cdevs_remove(dev_params);
error3:
  unregister_chrdev_region(dev_start, NUM_ENDPOINTS);
error2:
  class_destroy(dev_class);
error1:
  kfree(dev_params);
error0:
  return -ENODEV;
out:
  return 0;
}

static void __exit fini(void)
{
  unregister_rpmsg_driver(&rpmsg_driver);
  ept_cdevs_remove(dev_params);
  unregister_chrdev_region(dev_start, NUM_ENDPOINTS);
  class_destroy(dev_class);
  kfree(dev_params);
}

module_init(init);
module_exit(fini);

MODULE_DESCRIPTION("RPMSG Piksi driver");
MODULE_LICENSE("GPL v2");
