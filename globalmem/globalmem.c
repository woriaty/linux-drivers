/*
 * a simple char device drive: globalmem with mutex
 *
 * Copyright (C) 2017 Wang Jianjun use name Woriaty @ ubuntu 17.04
 *
 * Licensed under GPLv2 or later
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define GLOBALMEM_SIZE   0x1000
#define MEM_CLEAR        0x01
#define GLOBALMEM_MAJOR      230

static int globalmem_major = GLOBALMEM_MAJOR;
module_param(globalmem_major, int, S_IRUGO);

struct globalmem_dev{
	struct cdev cdev;
	unsigned char mem[GLOBALMEM_SIZE];
	struct mutex mutex;      //添加互斥体
};

struct globalmem_dev *globalmem_devp;

static int globalmem_open(struct inode *inode, struct file *filp)
{
	filp->private_data = globalmem_devp;      //将文件的私有数据指向结构体，通过私有数据进行结构体的访问
	return 0;
}

static int globalmem_release(struct inod *inode, struct file *filp)
{
	return 0;
}

static long globalmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct globalmem_dev *dev = filp->private_data;      //设备结构体与私有数据的连接
	switch(cmd){
		case MEM_CLEAR:
			mutex_lock(&dev->mutex);
			memset(dev->mem, 0, GLOBALMEM_SIZE);
			mutex_unlock(&dev->mutex);
			printk(KERN_INFO"globalmem is set to zero\n");
			break;
		default:
			return -EINVAL;
			//break;
	}
	return 0;
}

static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size, loff_t * ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct globalmem_dev *dev = filp->private_data;      //设备结构体与私有数据的连接

	if(p >= GLOBALMEM_SIZE)
		return 0;
	if(count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE -p;

	mutex_lock(&dev->mutex);         //访问dev中的共享资源时要先获取互斥体
	if(copy_to_user(buf, dev->mem + p, count))
		ret = -EFAULT;
	else{
		*ppos += count;
		ret = count;

		printk(KERN_INFO"read %u byte(s) from %1u\n",count,p);
	}

	mutex_unlock(&dev->mutex);     //访问完成后释放互斥体资源
	return ret;
}

static ssize_t globalmem_write(struct file *filp, const char __user * buf, size_t size, loff_t * ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct globalmem_dev *dev = filp->private_data;      //设备结构体与私有数据的连接

	if(p >= GLOBALMEM_SIZE)
		return 0;
	if(count > GLOBALMEM_SIZE -p)
		count = GLOBALMEM_SIZE -p;

	mutex_lock(&dev->mutex);
	if(copy_from_user(dev->mem + p, buf, count))
		ret = -EFAULT;
	else{
		*ppos += count;
		ret = count;

		printk(KERN_INFO"written %u byte(s) from %1u\n",count,p);
	}
	mutex_unlock(&dev->mutex);
	return ret;
}

static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret = 0;
	switch(orig){
		case 0:
			if(offset < 0){
				ret = -EINVAL;
				break;
			}
			if((unsigned int)offset > GLOBALMEM_SIZE){
				ret = -EINVAL;
				break;
			}
			filp->f_pos = (unsigned int)offset;
			ret = filp->f_pos;
			break;
		case 1:
			if((filp->f_pos + offset) > GLOBALMEM_SIZE){
				ret = -EINVAL;
				break;
			}
			if((filp->f_pos + offset) < 0){
				ret = -EINVAL;
				break;
			}
			filp->f_pos += offset;
			ret = filp->f_pos;
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}

static const struct file_operations globalmem_fops = {
	.owner = THIS_MODULE,
	.llseek = globalmem_llseek,
	.read = globalmem_read,
	.write = globalmem_write,
	.unlocked_ioctl = globalmem_ioctl,
	.open = globalmem_open,
	//.release = globalmem_release,
};

static void globalmem_setup_cdev(struct globalmem_dev *dev, int index)
{
	int err,devno = MKDEV(globalmem_major, index);

	cdev_init(&dev->cdev,&globalmem_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if(err)
		printk(KERN_NOTICE"Error %d adding globalmem %d", err, index);
}

static int __init globalmem_init(void)
{
	int ret;
	dev_t devno = MKDEV(globalmem_major,0);

	if(globalmem_major)
		ret = register_chrdev_region(devno,1,"globalmem");
	else{
		ret = alloc_chrdev_region(&devno, 0, 1, "globalmem");
		globalmem_major = MAJOR(devno);
	}
	if(ret < 0)
		return ret;

	globalmem_devp = kzalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
	if(!globalmem_devp){
		ret = -ENOMEM;
		goto fail_malloc;
	}

	mutex_init(&globalmem_devp->mutex);       //初始化互斥体变量
	globalmem_setup_cdev(globalmem_devp,0);
	return 0;

fail_malloc:
unregister_chrdev_region(devno,1);
return ret;
}

module_init(globalmem_init);

static void __exit globalmem_exit(void)
{
	cdev_del(&globalmem_devp->cdev);
	kfree(globalmem_devp);
	unregister_chrdev_region(MKDEV(globalmem_major,0),1);
	printk(KERN_INFO"test data in function %s\n", __func__);
}

module_exit(globalmem_exit);

MODULE_AUTHOR("Woriaty @ ubuntu 17.04");
MODULE_LICENSE("GPL v2");
