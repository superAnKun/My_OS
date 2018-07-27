#ifndef __INODE_H_
#define __INODE_H_
#include "../kernel/list.h"
#include "../stdint.h"

struct inode {
	uint32_t i_no;  //inode号
	uint32_t i_size;  //文件大小

	uint32_t i_open_cnts;  //记录此文件被打开的次数
	bool write_deny;   //写文件不能并行，进程写文件时检查此标识

	uint32_t i_sectors[13];  //0-11是直接块  12用来存储一级间接块指针 二级的不进行实现
	struct list_elem inode_tag;  //用于放入已打开文件列表
};

#endif
