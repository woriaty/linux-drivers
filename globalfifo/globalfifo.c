/*
 * a simple char device drive: globalfifo with wait_queue
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
//#include <linux/interrupt.h>
#include <linux/sched/signal.h>

#define GLOBALFIFO_SIZE   0x1000
#define MEM_CLEAR        0x01
#define GLOBALFIFO_MAJOR      231

static int globalfifo_major = GLOBALFIFO_MAJOR;
module_param(globalfifo_major, int, S_IRUGO);

struct globalfifo_dev{
	struct cdev cdev;
	unsigned int current_len;
	unsigned char mem[GLOBALFIFO_SIZE];
	struct mutex mutex;      //添加互斥体
	wait_queue_head_t r_wait;    //添加等待队列头部
	wait_queue_head_t w_wait;
	struct fasync_struct *async_queue;      //定义异步通知结构体
};

struct globalfifo_dev *globalfifo_devp;

static int globalfifo_fasync(int fd, struct file *filp, int mode)    //支持异步通知的fasync()函数
{
	struct globalfifo_dev *dev = filp->private_data;
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int globalfifo_open(struct inode *inode, struct file *filp)
{
	filp->private_data = globalfifo_devp;      //将文件的私有数据指向结构体，通过私有数据进行结构体的访问
	return 0;
}

static int globalfifo_release(struct inode *inode, struct file *filp)
{
	globalfifo_fasync(-1, filp,0);    //将文件从异步通知列表删除
	return 0;
}

static long globalfifo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct globalfifo_dev *dev = filp->private_data;      //设备结构体与私有数据的连接
	switch(cmd){
		case MEM_CLEAR:
			mutex_lock(&dev->mutex);
			memset(dev->mem, 0, GLOBALFIFO_SIZE);
			mutex_unlock(&dev->mutex);
			printk(KERN_INFO"globalfifo is set to zero\n");
			break;
		default:
			return -EINVAL;
			//break;
	}
	return 0;
}

static ssize_t globalfifo_read(struct file *filp, char __user *buf, size_t size, loff_t * ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct globalfifo_dev *dev = filp->private_data;      //设备结构体与私有数据的连接

	DECLARE_WAITQUEUE(wait,current);

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->r_wait, &wait);     //与DECLARE_WAITQUEUE()一起将自己添加到r_wait队列中

	while(dev->current_len == 0){
		if(filp->f_flags & O_NONBLOCK){
			ret = -EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE);  //设置浅睡眠标记
		mutex_unlock(&dev->mutex);    //进行任务调度之前主动释放互斥体

		schedule();       //任务调度，读进程进入睡眠状态
		if(signal_pending(current)){
			ret = -ERESTARTSYS;
			goto out2;
		}
        	mutex_lock(&dev->mutex);
	}
	if(count > dev->current_len)
		count = dev->current_len;

	if(copy_to_user(buf, dev->mem, count)){
		ret = -EFAULT;
		goto out;
	}
	else{
		memcpy(dev->mem, dev->mem + count, dev->current_len - count);
		dev->current_len -= count;
		printk(KERN_INFO "read %d byte(s), current_len:%d\n",count,dev->current_len);

		wake_up_interruptible(&dev->w_wait);    //唤醒可能阻塞的写进程

		ret = count;
	}
out:
mutex_unlock(&dev->mutex);
out2:
remove_wait_queue(&dev->r_wait,&wait);
set_current_state(TASK_RUNNING);
return ret;
}

static ssize_t globalfifo_write(struct file *filp, const char __user * buf, size_t size, loff_t * ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct globalfifo_dev *dev = filp->private_data;      //设备结构体与私有数据的连接

	DECLARE_WAITQUEUE(wait,current);

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->w_wait,&wait);

	while(dev->current_len == GLOBALFIFO_SIZE){
		if(filp->f_flags & O_NONBLOCK){
			ret = -EAGAIN;
			goto out;
		}

		__set_current_state(TASK_INTERRUPTIBLE);

		mutex_unlock(&dev->mutex);

		schedule();
		if(signal_pending(current)){
			ret = -ERESTARTSYS;
			goto out2;
		}
		mutex_lock(&dev->mutex);
	}

	if(count > GLOBALFIFO_SIZE - dev->current_len)
		count = GLOBALFIFO_SIZE - dev->current_len;
	if(copy_from_user(dev->mem + dev->current_len,buf,count)){
		ret = -EFAULT;
		goto out;
	}else{
		dev->current_len += count;
		printk(KERN_INFO"written %d byte(s),current_len: %d\n",count, dev->current_len);

		wake_up_interruptible(&dev->r_wait);

		if(dev->async_queue){
			kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
			printk(KERN_DEBUG "%s kill SIGIO\n", __func__);
		}

		ret = count;
	}

out:
mutex_unlock(&dev->mutex);
out2:
remove_wait_queue(&dev->w_wait,&wait);
set_current_state(TASK_RUNNING);
return ret;

}

static loff_t globalfifo_llseek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret = 0;
	switch(orig){
		case 0:
			if(offset < 0){
				ret = -EINVAL;
				break;
			}
			if((unsigned int)offset > GLOBALFIFO_SIZE){
				ret = -EINVAL;
				break;
			}
			filp->f_pos = (unsigned int)offset;
			ret = filp->f_pos;
			break;
		case 1:
			if((filp->f_pos + offset) > GLOBALFIFO_SIZE){
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

static const struct file_operations globalfifo_fops = {
	.owner = THIS_MODULE,
	.llseek = globalfifo_llseek,
	.read = globalfifo_read,
	.write = globalfifo_write,
	.unlocked_ioctl = globalfifo_ioctl,
	.open = globalfifo_open,
	.release = globalfifo_release,
};

static void globalfifo_setup_cdev(struct globalfifo_dev *dev, int index)
{
	int err,devno = MKDEV(globalfifo_major, index);

	cdev_init(&dev->cdev,&globalfifo_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if(err)
		printk(KERN_NOTICE"Error %d adding globalfifo %d", err, index);
}

static int __init globalfifo_init(void)
{
	int ret;
	dev_t devno = MKDEV(globalfifo_major,0);

	if(globalfifo_major)
		ret = register_chrdev_region(devno,1,"globalfifo");
	else{
		ret = alloc_chrdev_region(&devno, 0, 1, "globalfifo");
		globalfifo_major = MAJOR(devno);
	}
	if(ret < 0)
		return ret;

	globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev), GFP_KERNEL);
	if(!globalfifo_devp){
		ret = -ENOMEM;
		goto fail_malloc;
	}
	globalfifo_setup_cdev(globalfifo_devp,0);

	mutex_init(&globalfifo_devp->mutex);            //初始化互斥体变量
	init_waitqueue_head(&globalfifo_devp->r_wait);  //对等待队列头进行初始化
	init_waitqueue_head(&globalfifo_devp->w_wait);
	printk(KERN_INFO"globalfifo init ok\n");
	return 0;

fail_malloc:
unregister_chrdev_region(devno,1);
return ret;
}

module_init(globalfifo_init);

static void __exit globalfifo_exit(void)
{
	cdev_del(&globalfifo_devp->cdev);
	kfree(globalfifo_devp);
	unregister_chrdev_region(MKDEV(globalfifo_major,0),1);
	printk(KERN_INFO"test data in function %s\n", __func__);
}

module_exit(globalfifo_exit);

MODULE_AUTHOR("Woriaty @ ubuntu 17.04");
MODULE_LICENSE("GPL v2");
