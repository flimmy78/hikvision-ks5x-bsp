/******************************************************************************
 *    COPYRIGHT (C) 2013 Czyong. Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-07-20
 *
 * Support nand dynamic change partitions.
 * command:
 *   echo "hinand:12M(p1),64m(p2),-(p3)" >/proc/partchange
 *   echo "/dev/xxx/mmcblk0:8M(p1),10M(p2),-(p3)" >/proc/partchange
 *
******************************************************************************/

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <linux/cmdline-parser.h>

#ifdef CONFIG_MTD_PART_CHANGE
extern int mtd_part_change(struct cmdline_parts *parts);
#endif
extern int cmdline_parts_setup(char *s);

/*****************************************************************************/

static int block_part_change(struct cmdline_parts *parts, char *cmdline,
			     int length)
{
	char *p = cmdline;
	int ret = -EFAULT;
	struct file *filp;

	filp = filp_open(parts->name, O_RDWR, 0);
	if (IS_ERR(filp)) {
		pr_err("Can't open  %s  device inode.\n", parts->name);
		return -ENODEV;
	}

	p = strnstr(cmdline, "mmcblk", length);
	if (!p) {
		pr_err("Can't find mmcblk from string %s.\n", cmdline);
		return -ENODEV;
	}

	cmdline_parts_setup(p);

	if (filp->f_op && filp->f_op->unlocked_ioctl)
		ret = filp->f_op->unlocked_ioctl(filp, BLKRRPART, 0);

	pr_notice("Add block device '%s' partitions %s.\n",
		  parts->name, (!ret ? "success" : "fail"));

	filp_close(filp, NULL);

	return ret;
}

static int parts_table_change(struct cmdline_parts *parts, char *cmdline,
			       int length)
{
	int ret = -ENODEV;

	for (; parts; parts = parts->next_parts) {
#ifdef CONFIG_MTD_PART_CHANGE
		ret = mtd_part_change(parts);
		if (ret != -ENODEV)
			continue;
#endif
		ret = block_part_change(parts, cmdline, length);
		if (ret != -ENODEV)
			continue;
	}

	return ret;
}

static ssize_t parts_proc_write(struct file *file, const char __user *buf,
				size_t size, loff_t *_pos)
{
	int ret = -EINVAL;
	char *cmdline;
	struct cmdline_parts *parts;

	cmdline = (char *)__get_free_page(GFP_KERNEL);
	if (!cmdline) {
		pr_err("Can't get free page.\n");
		return -ENOMEM;
	}

	if (size <= 1 || size >= PAGE_SIZE)
		goto done;

	if (copy_from_user(cmdline, buf, size) != 0)
		goto done;

	cmdline[size] = 0;

	if (!cmdline_parts_parse(&parts, cmdline)) {
		ret = parts_table_change(parts, cmdline, size + 1);
		if (!ret)
			ret = size;
		cmdline_parts_free(&parts);
	}

done:
	free_page((unsigned long)cmdline);
	return ret;
}

static int parts_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, "\n");
	return 0;
}

static int parts_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, parts_proc_show, NULL);
}

static const struct file_operations cmdline_parts_fops = {
	.open = parts_proc_open,
	.read = seq_read,
	.write = parts_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static int __init parts_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("partchange", 0755, NULL, &cmdline_parts_fops);
	if (!entry) {
		pr_err("Can't create parts file\n");
		return -ENOMEM;
	}

	return 0;
}

static void __exit parts_exit(void)
{
	remove_proc_entry("partchange", NULL);
}
/*****************************************************************************/

module_init(parts_init);
module_exit(parts_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Czyong");
MODULE_DESCRIPTION("partitions change driver.");
