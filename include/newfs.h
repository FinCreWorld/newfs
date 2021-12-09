#ifndef _NEWFS_H_
#define _NEWFS_H_

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include "fcntl.h"
#include "string.h"
#include "fuse.h"
#include <stddef.h>
#include "ddriver.h"
#include "errno.h"
#include "types.h"

typedef int boolean;

/******************************************************************************
* SECTION: 宏定义
*******************************************************************************/
#define OPTION(t, p)        { t, offsetof(struct custom_options, p), 1 }
#define NEWFS_DBG(fmt, ...) do { printf("NEWFS_DBG: " fmt, ##__VA_ARGS__); } while(0)

/******************************************************************************
* SECTION: 全局变量
*******************************************************************************/
static const struct fuse_opt option_spec[] = {		/* 用于FUSE文件系统解析参数 */
	OPTION("--device=%s", device),
	FUSE_OPT_END
};
struct newfs_super super; 
struct custom_options newfs_options;			 /* 全局选项 */

/******************************************************************************
* SECTION: newfs_debug.c
*******************************************************************************/
void newfs_dump_map();

/******************************************************************************
* SECTION: newfs_utils.c
*******************************************************************************/
int newfs_driver_read(int offset, char *out_content, int size);
int newfs_driver_write(int offset, char *in_content, int size);
struct newfs_inode* newfs_alloc_inode(struct newfs_dentry* dentry);
int newfs_syn_inode(struct newfs_inode* inode);
int newfs_alloc_dentry(struct newfs_inode* inode, struct newfs_dentry* dentry);
struct newfs_inode* newfs_read_inode(struct newfs_dentry* dentry, int ino);
int newfs_mount(struct custom_options olptions);
int newfs_umount();
char* newfs_get_fname(const char* path);
struct newfs_dentry* new_dentry(char *fname, FILE_TYPE ftype);
struct newfs_dentry* newfs_lookup(const char * path, boolean* is_find, boolean* is_root);
struct newfs_dentry* newfs_get_dentry(struct newfs_inode * inode, int dir);

/******************************************************************************
* SECTION: newfs.c
*******************************************************************************/
void* 			   newfs_init(struct fuse_conn_info *);
void  			   newfs_destroy(void *);
int   			   newfs_mkdir(const char *, mode_t);
int   			   newfs_getattr(const char *, struct stat *);
int   			   newfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   newfs_mknod(const char *, mode_t, dev_t);
int   			   newfs_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   newfs_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   newfs_access(const char *, int);
int   			   newfs_unlink(const char *);
int   			   newfs_rmdir(const char *);
int   			   newfs_rename(const char *, const char *);
int   			   newfs_utimens(const char *, const struct timespec tv[2]);
int   			   newfs_truncate(const char *, off_t);
			
int   			   newfs_open(const char *, struct fuse_file_info *);
int   			   newfs_opendir(const char *, struct fuse_file_info *);

#endif  /* _newfs_H_ */