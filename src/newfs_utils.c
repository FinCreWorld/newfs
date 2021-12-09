#include "../include/newfs.h"

/******************************************************************************
* SECTION: 工具函数
*******************************************************************************/
struct newfs_dentry* new_dentry(char *fname, FILE_TYPE ftype) {
    struct newfs_dentry * dentry = (struct newfs_dentry*) malloc(sizeof (struct newfs_dentry));
    memset(dentry, 0, sizeof(struct newfs_dentry));
    memcpy(dentry->fname, fname, MAX_NAME_LEN);
    dentry->ftype = ftype;
    dentry->ino   = -1;
    dentry->inode     = NULL;
    dentry->parent    = NULL;
    dentry->brother   = NULL;
	return dentry;
}

/**
 * @param 驱动读
 * 
 * @param offset 起始地址
 * @param out_content 指针地址
 * @param size 大小
 * @return 0 成功，否则失败
 */
int newfs_driver_read(int offset, char *out_content, int size) {
    int			offset_aligned	= ROUND_DOWN(offset, NEWFS_IO_SZ);
	int			bias			= offset - offset_aligned;
	int			size_aligned	= ROUND_UP(size + bias, NEWFS_IO_SZ);
	char*	tmp_content		= (char*)malloc(size_aligned);
	char*	cur 			= tmp_content;

	// 读取整个块
	ddriver_seek(NEWFS_DRIVER, offset_aligned, SEEK_SET);
	while (size_aligned != 0) {
		ddriver_read(NEWFS_DRIVER, cur, NEWFS_IO_SZ);
		cur 			+= NEWFS_IO_SZ;
		size_aligned 	-= NEWFS_IO_SZ;
	}

	// 将目标区域内容复制到目标地址中
	memcpy(out_content, tmp_content + bias, size);
	free(tmp_content);
	return NEWFS_ERROR_NONE;
}

/**
 * @brief 驱动写
 * 
 * @param offset 
 * @param in_content 
 * @param size 
 * @return int 
 */
int newfs_driver_write(int offset, char *in_content, int size) {
	int			offset_aligned	= ROUND_DOWN(offset, NEWFS_IO_SZ);
	int			bias			= offset - offset_aligned;
	int			size_aligned	= ROUND_UP(size + bias, NEWFS_IO_SZ);
	char*	tmp_content 	= (char*)malloc(size_aligned);
	char*	cur 			= tmp_content;

	newfs_driver_read(offset_aligned, tmp_content, size_aligned);
	memcpy(tmp_content + bias, in_content, size);

	ddriver_seek(NEWFS_DRIVER, offset_aligned, SEEK_SET);
	while (size_aligned != 0) {
		ddriver_write(NEWFS_DRIVER, cur, NEWFS_IO_SZ);
		cur				+= NEWFS_IO_SZ;
		size_aligned	-= NEWFS_IO_SZ;
	}

	free(tmp_content);
	return NEWFS_ERROR_NONE;
}



/**
 * @brief 分配一个 inode，占用位图
 * 注：分配出的 inode 在内存中，需要更改 inode 位图，data 位图
 * @param dentry 该 dentry 指向分配的 inode
 * @return newfs_inode
 */
struct newfs_inode* newfs_alloc_inode(struct newfs_dentry* dentry) {
	struct newfs_inode* inode;
	int byte_cursor = 0;
	int bit_cursor	= 0;
	int ino_cursor	= 0;
	int data_blks	= 0;
	int init_blks	= 1;
	boolean is_find_free_entry = FALSE;

	if (strcmp(dentry->fname, "/") == 0) {
		ino_cursor = NEWFS_ROOT_INO;
		super.map_inode[0] |= (0x1 << NEWFS_ROOT_INO);
		is_find_free_entry = TRUE;
	} else {
		// 分配索引节点位图，找到空闲位置 ino_cursor
		for (byte_cursor = 0; byte_cursor < NEWFS_BLKS_SZ(super.map_inode_blks);
			byte_cursor++) {
			for (bit_cursor = 0; bit_cursor < UINT8_BITS; ++bit_cursor) {
				if ((super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {
					super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
					is_find_free_entry = TRUE;
					break;
				}
				ino_cursor++;
			}
			if (is_find_free_entry)
				break;
		}
	}

	if (!is_find_free_entry || ino_cursor >= super.max_ino)
		return (struct newfs_inode*)-NEWFS_ERROR_NOSPACE;

	// 初始化 inode 属性值
	inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));

	memset(inode->block_pos, 0, sizeof(inode->block_pos));
	inode->ino	= ino_cursor;
	inode->size	= 0;
	inode->link = 0;
	inode->blks = 1;

	dentry->inode	= inode;
	dentry->ino		= inode->ino;

	inode->dentry	= dentry;

	inode->dir_cnt	= 0;
	inode->dentrys	= NULL;

	// 分配数据块位图
	// 只分配 1 个数据块，后续再进行扩展
	ino_cursor = 0;
	while (data_blks < init_blks && ino_cursor < super.max_data_blks) {
		for (byte_cursor = 0; byte_cursor < NEWFS_BLKS_SZ(super.map_data_blks); ++byte_cursor) {
			for (bit_cursor = 0; bit_cursor < UINT8_BITS; ++bit_cursor) {
				if ((super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {
					super.map_data[byte_cursor] |= (0x1 << bit_cursor);
					inode->block_pos[data_blks++] = ino_cursor;
				}
				++ino_cursor;

				if (data_blks >= init_blks || ino_cursor >= super.max_data_blks) break;
			}
			if (data_blks >= init_blks || ino_cursor >= super.max_data_blks) break;
		}
	}
	if (data_blks < init_blks || ino_cursor >= super.max_data_blks)
		return (struct newfs_inode*)-NEWFS_ERROR_NOSPACE;

	// 分配数据块缓存空间
	if (inode->dentry->ftype == NEWFS_FILE) {
		inode->block_pointer[0] = (char*)malloc(sizeof(NEWFS_BLK_SZ));
	}
	
	return inode;
}


/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 * 
 * @param inode 
 * @return int 
 */
int newfs_sync_inode(struct newfs_inode* inode) {
	struct newfs_inode_d  inode_d;
    struct newfs_dentry*  dentry_cursor;
    struct newfs_dentry_d dentry_d;
    int	ino				= inode->ino;

	// 构造 inode_d
	inode_d.ino			= ino;
	inode_d.size		= inode->size;
	inode_d.ftype		= inode->dentry->ftype;
	inode_d.link		= inode->link;
	inode_d.dir_cnt		= inode->dir_cnt;
	inode_d.blks		= inode->blks;
	memcpy(inode_d.block_pos, inode->block_pos, sizeof(inode->block_pos));

	int offset;
	int blk_ino = 0;	// 位于内存节点的第 i 个数据块
	int blk_sz	= 0;

	// 将 inode 刷入内存
	if (newfs_driver_write(NEWFS_INO_OFS(ino), (char*)&inode_d,
						sizeof(struct newfs_inode_d)) != NEWFS_ERROR_NONE) {
		NEWFS_DBG("[%s] io error\n", __func__);
        return -NEWFS_ERROR_IO;
	}

	if (inode->dentry->ftype == NEWFS_DIR) {
		dentry_cursor	= inode->dentrys;
		offset			= NEWFS_DATA_OFS(inode->block_pos[blk_ino]);
		while (dentry_cursor != NULL) {
			memcpy(dentry_d.fname, dentry_cursor->fname, MAX_NAME_LEN);
			dentry_d.ftype  = dentry_cursor->ftype;
			dentry_d.ino	= dentry_cursor->ino;
			if (newfs_driver_write(offset, (char*)&dentry_d,
									sizeof(struct newfs_dentry_d)) != NEWFS_ERROR_NONE) {
				NEWFS_DBG("[%s] io error\n", __func__);
				return -NEWFS_ERROR_IO;
			}

			if (dentry_cursor->inode != NULL)
				newfs_sync_inode(dentry_cursor->inode);
			
			dentry_cursor = dentry_cursor->brother;

			blk_sz		 += sizeof(struct newfs_dentry_d);
			offset		 += sizeof(struct newfs_dentry_d);
			if (blk_sz + sizeof(struct newfs_dentry_d) >= NEWFS_BLK_SZ) {
				blk_sz  = 0;
				blk_ino++;
				offset = NEWFS_DATA_OFS(inode->block_pos[blk_ino]);
			}
		}
	} else if (inode->dentry->ftype == NEWFS_FILE) {
		for (int i = 0; i < inode->blks; ++i) {
			if (newfs_driver_write(NEWFS_DATA_OFS(inode->block_pos[i]), inode->block_pointer[i],
								NEWFS_BLK_SZ) != NEWFS_ERROR_NONE) {
				NEWFS_DBG("[%s] io error\n", __func__);
				return -NEWFS_ERROR_IO;
			}
		}
	}
	return NEWFS_ERROR_NONE;
}

/**
 * @brief 为一个inode分配dentry，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int newfs_alloc_dentry(struct newfs_inode* inode, struct newfs_dentry* dentry) {
	dentry->brother = inode->dentrys;
	inode->dentrys = dentry;
	inode->dir_cnt++;
	return inode->dir_cnt;
}


/**
 * @brief 根据目录项读取对应的索引节点
 * 
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct newfs_inode* 
 */
struct newfs_inode* newfs_read_inode(struct newfs_dentry* dentry, int ino) {
	struct newfs_inode* inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
	struct newfs_inode_d inode_d;
	struct newfs_dentry* sub_dentry;
	struct newfs_dentry_d dentry_d;
	int	   dir_cnt = 0;

	if (newfs_driver_read(NEWFS_INO_OFS(ino), (char *)&inode_d,
							sizeof(struct newfs_inode_d)) != NEWFS_ERROR_NONE) {
		NEWFS_DBG("[%s] io error\n", __func__);
		return NULL;
	}
	inode->dir_cnt = 0;
	inode->ino = inode_d.ino;
	inode->size = inode_d.size;
	inode->link = inode_d.link;
	inode->blks = inode_d.blks;
	inode->dentry = dentry;
	inode->dentrys = NULL;
	memcpy(inode->block_pos, inode_d.block_pos, sizeof(inode_d.block_pos));

	int offset;
	int blk_ino = 0;
	int blk_sz  = 0;
	if (inode->dentry->ftype == NEWFS_DIR) {
		dir_cnt = inode_d.dir_cnt;
		offset  = NEWFS_DATA_OFS(inode->block_pos[blk_ino]);
		for (int i = 0; i < dir_cnt; ++i) {
			if (newfs_driver_read(offset, (char*)&dentry_d, 
									sizeof(struct newfs_dentry_d)) != NEWFS_ERROR_NONE) {
				NEWFS_DBG("[%s] io error\n", __func__);
				return NULL;
			}
			
			sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
			sub_dentry->parent  = inode->dentry;
			sub_dentry->ino		= dentry_d.ino;
			newfs_alloc_dentry(inode, sub_dentry);

			blk_sz += sizeof(struct newfs_dentry_d);
			offset += sizeof(struct newfs_dentry_d);
			if (blk_sz + sizeof(struct newfs_dentry_d) >= NEWFS_BLK_SZ) {
				blk_sz = 0;
				blk_ino++;
				offset = NEWFS_DATA_OFS(inode->block_pos[blk_ino]);
			}
		}
	} else if (inode->dentry->ftype == NEWFS_FILE) {
		for (int i = 0; i < inode->blks; ++i) {
			inode->block_pointer[i] = (char*)malloc(NEWFS_BLK_SZ);
			if (newfs_driver_read(NEWFS_DATA_OFS(inode->block_pos[i]), (char*)inode->block_pointer[i],
								NEWFS_BLK_SZ) != NEWFS_ERROR_NONE) {
				NEWFS_DBG("[%s] io error\n", __func__);
				return NULL;
			}	
		}
	}
	return inode;
}


/**
 * @brief 挂载sfs, Layout 如下
 * 
 * Layout
 * | Super | Inode Map | Data Map | Inode | Data |
 * 
 * BLK_SZ = 2*IO_SZ
 * 
 * 每个Inode占用一个Blk
 * @param options 
 * @return int 
 */
int newfs_mount(struct custom_options olptions) {
    int						ret = NEWFS_ERROR_NONE;
	int						driver_fd;
	struct newfs_dentry* 	root_dentry;
	struct newfs_inode*		root_inode;
	struct newfs_super_d 	super_d;

	int						inode_nums;
	int						super_blks;
	int						map_inode_blks;
	int						map_data_blks;
	int						inode_blks;
	int						data_blks;
	boolean 				is_init = FALSE;	// 用于标记是否为第一次加载

	super.is_mounted = FALSE;

	// 读取超级块
	driver_fd = ddriver_open(newfs_options.device);
	
	if (driver_fd < 0) {
		return driver_fd;
	}

    super.fd = driver_fd;
	ddriver_ioctl(super.fd, IOC_REQ_DEVICE_SIZE, &super.sz_disk);
	ddriver_ioctl(super.fd, IOC_REQ_DEVICE_IO_SZ, &super.sz_io);

	root_dentry = new_dentry("/", NEWFS_DIR);

	if (newfs_driver_read(NEWFS_SUPER_OFS, (char*)&super_d,
                            sizeof(struct newfs_super_d)) != NEWFS_ERROR_NONE)
        return -NEWFS_ERROR_IO;

	printf("%x\n", super_d.magic_num);
	printf("inode_ofs: %d\n", super_d.map_inode_offset);
	printf("data_ofs: %d\n", super_d.map_data_offset);
	printf("-------------------------------------------\n");
	if (super_d.magic_num != NEWFS_MAGIC) {
		// 计算超级块、索引块、数据块、索引位图块、数据位图块数目
		super_blks = ROUND_UP(sizeof(struct newfs_super_d), NEWFS_BLK_SZ) / NEWFS_BLK_SZ;
		inode_nums = NEWFS_FILE_NUM;
		inode_blks = ROUND_UP(inode_nums * sizeof(struct newfs_inode_d), NEWFS_BLK_SZ) / NEWFS_BLK_SZ;
		data_blks  = NEWFS_FILE_NUM * NEWFS_DATA_PER_FILE;
		map_inode_blks = (ROUND_UP(ROUND_UP(inode_nums, UINT8_BITS), NEWFS_BLK_SZ) + (NEWFS_BLK_SZ * sizeof(char)) - 1) / (NEWFS_BLK_SZ * sizeof(char));
		map_data_blks  = (ROUND_UP(ROUND_UP(data_blks, UINT8_BITS), NEWFS_BLK_SZ) + (NEWFS_BLK_SZ * sizeof(char)) - 1) / (NEWFS_BLK_SZ * sizeof(char));

		super.max_ino			= inode_nums;
		super.max_data_blks		= inode_nums * NEWFS_DATA_PER_FILE;
		super_d.max_ino			= inode_nums;
		super_d.max_data_blks	= super.max_data_blks;
		super_d.map_inode_blks	= map_inode_blks;
		super_d.map_inode_offset	= NEWFS_SUPER_OFS + NEWFS_BLKS_SZ(super_blks);
		super_d.map_data_blks		= map_data_blks;
		super_d.map_data_offset	= super_d.map_inode_offset + NEWFS_BLKS_SZ(map_inode_blks);
		super_d.inode_offset		= super_d.map_data_offset + NEWFS_BLKS_SZ(map_data_blks);
		super_d.data_offset		= super_d.inode_offset    + NEWFS_BLKS_SZ(inode_blks);
		super_d.sz_usage			= 0;
		
		printf("This is init\n");
		printf("%x\n", super_d.magic_num);
		printf("inode_ofs: %d\n", super_d.map_inode_offset);
		printf("data_ofs: %d\n", super_d.map_data_offset);
		is_init = TRUE;
	}

	super.sz_usage				 = super_d.sz_usage;
	super.max_ino				 = super_d.max_ino;
	super.max_data_blks			 = super_d.max_data_blks;
	super.map_inode				 = (char*)malloc(NEWFS_BLKS_SZ(super_d.map_inode_blks));
	super.map_data				 = (char*)malloc(NEWFS_BLKS_SZ(super_d.map_data_blks));
	super.map_inode_blks		 = super_d.map_inode_blks;
	super.map_inode_offset		 = super_d.map_inode_offset;
	super.map_data_blks			 = super_d.map_data_blks;
	super.map_data_offset		 = super_d.map_data_offset;
	super.inode_offset			 = super_d.inode_offset;
	super.data_offset			 = super_d.data_offset;


	if (newfs_driver_read(super_d.map_inode_offset, (char*)super.map_inode,
					  NEWFS_BLKS_SZ(super_d.map_inode_blks)) != NEWFS_ERROR_NONE) {
		return -NEWFS_ERROR_IO;
	}
	if (newfs_driver_read(super_d.map_data_offset, (char*)super.map_data,
					  NEWFS_BLKS_SZ(super_d.map_data_blks)) != NEWFS_ERROR_NONE) {
		return -NEWFS_ERROR_IO;
	}

	if (is_init) {
		root_inode	= newfs_alloc_inode(root_dentry);
		// root_inode->ino = NEWFS_ROOT_INO;
		newfs_sync_inode(root_inode);
	}

	root_inode				= newfs_read_inode(root_dentry, NEWFS_ROOT_INO);
	root_dentry->inode		= root_inode;
	super.root_dentry		= root_dentry;
	super.is_mounted		= TRUE;

	newfs_dump_map();
	return ret;
}


/**
 * @brief 卸载文件系统
 * 
 * @return int
 */
int newfs_umount() {
	struct newfs_super_d newfs_super_d;

	if (!super.is_mounted) {
		return NEWFS_ERROR_NONE;
	}

	newfs_sync_inode(super.root_dentry->inode);

	newfs_super_d.magic_num			= NEWFS_MAGIC;
	newfs_super_d.sz_usage			= super.sz_usage;
	newfs_super_d.max_ino			= super.max_ino;
	newfs_super_d.map_inode_blks	= super.map_inode_blks;
	newfs_super_d.map_inode_offset  = super.map_inode_offset;
	newfs_super_d.map_data_blks		= super.map_data_blks;
	newfs_super_d.map_data_offset	= super.map_data_offset;
	newfs_super_d.inode_offset		= super.inode_offset;
	newfs_super_d.data_offset		= super.data_offset;

	if (newfs_driver_write(NEWFS_SUPER_OFS, (char*)&newfs_super_d,
							sizeof(struct newfs_super_d)) != NEWFS_ERROR_NONE) {
		return -NEWFS_ERROR_IO;
	}

	if (newfs_driver_write(newfs_super_d.map_inode_offset, (char*)super.map_inode,
							NEWFS_BLKS_SZ(newfs_super_d.map_inode_blks)) != NEWFS_ERROR_NONE) {
		return -NEWFS_ERROR_IO;
	}
	printf("inode_ofs: %d\n", newfs_super_d.map_inode_offset);
	printf("data_ofs: %d\n", newfs_super_d.map_data_offset);
	printf("--------------------------------------------\n");
	newfs_dump_map();

	if (newfs_driver_write(newfs_super_d.map_data_offset, (char*)super.map_data,
							NEWFS_BLKS_SZ(newfs_super_d.map_data_blks)) != NEWFS_ERROR_NONE) {
		return -NEWFS_ERROR_IO;
	}

	free(super.map_inode);
	free(super.map_data);
	ddriver_close(NEWFS_DRIVER);

	return NEWFS_ERROR_NONE;
}

/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int newfs_calc_lvl(const char * path) {
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    const char* str = path;
    int   lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != 0) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}

/**
 * @brief 
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 * 
 * @param path 
 * @return struct sfs_inode* 
 */
struct newfs_dentry* newfs_lookup(const char * path, boolean* is_find, boolean* is_root) {
	struct newfs_dentry* dentry_cursor  = super.root_dentry;
	struct newfs_dentry* dentry_ret 	= NULL;
	struct newfs_inode*	 inode;
	int total_lvl = newfs_calc_lvl(path);
	int lvl = 0;
	boolean is_hit;
	char *fname = NULL;
	char* path_cpy = (char*)malloc(sizeof(path));
	*is_root = FALSE;
	strcpy(path_cpy, path);

	if (total_lvl == 0) {
		*is_find = TRUE;
		*is_root = TRUE;
		dentry_ret = super.root_dentry;
	}

	fname = strtok(path_cpy, "/");
	while (fname) {
		lvl++;
		if (dentry_cursor->inode == NULL) {
			newfs_read_inode(dentry_cursor, dentry_cursor->ino);
		}

		inode = dentry_cursor->inode;

		if (inode->dentry->ftype == NEWFS_FILE && lvl < total_lvl) {
			NEWFS_DBG("[%s] not a dir\n", __func__);
			dentry_ret = inode->dentry;
			break;
		}

		if (inode->dentry->ftype == NEWFS_DIR) {
			dentry_cursor	= inode->dentrys;
			is_hit			= FALSE;

			while (dentry_cursor) {
				if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0) {
					is_hit = TRUE;
					break;
				}
				dentry_cursor = dentry_cursor->brother;
			}

			if (!is_hit) {
				*is_find = FALSE;
				NEWFS_DBG("[%s] not found %s\n", __func__, fname);
				dentry_ret = inode->dentry;
				break;
			}

			if (is_hit && lvl == total_lvl) {
				*is_find = TRUE;
				dentry_ret = dentry_cursor;
				break;
			}
		}
		fname = strtok(NULL, "/");
	}

	if (dentry_ret->inode == NULL) {
		dentry_ret->inode = newfs_read_inode(dentry_ret, dentry_ret->ino);
	}

	return dentry_ret;
}

/**
 * @brief 获取文件名
 * 
 * @param path 
 * @return char* 
 */
char* newfs_get_fname(const char* path) {
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}

/**
 * @brief 
 * 
 * @param inode 
 * @param dir [0...]
 * @return struct newfs_dentry* 
 */
struct newfs_dentry* newfs_get_dentry(struct newfs_inode * inode, int dir) {
    struct newfs_dentry* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}