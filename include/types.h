#ifndef _TYPES_H_
#define _TYPES_H_

/******************************************************************************
* SECTION: 参数
*******************************************************************************/
#define MAX_NAME_LEN    128
#define FALSE           0
#define TRUE            1
#define UINT8_BITS      8

#define NEWFS_MAGIC           	0x12345678  /* TODO: Define by yourself */
#define NEWFS_SUPER_OFS			0
#define NEWFS_ROOT_INO  		2
#define NEWFS_DEFAULT_PERM    	0777   		/* 全权限打开 */

#define ROUND_DOWN(value, round) ((value) / (round) * (round))
#define ROUND_UP(value, round) (((value) + (round) - 1) / (round) * (round))

#define NEWFS_DATA_PER_FILE 6
#define NEWFS_FILE_NUM 500
#define NEWFS_IO_SZ 512
#define NEWFS_BLK_SZ (NEWFS_IO_SZ << 1)
#define NEWFS_NODE_PER_FILE 1

#define NEWFS_BLKS_SZ(num) ((num) * NEWFS_BLK_SZ)
#define NEWFS_DRIVER (super.fd)
#define NEWFS_INO_OFS(ino) (super.inode_offset + (ino) * sizeof(struct newfs_inode_d))
#define NEWFS_DATA_OFS(ino) (super.data_offset + (ino) * NEWFS_BLK_SZ)

#define NEWFS_ERROR_NONE          0
#define NEWFS_ERROR_ACCESS        EACCES
#define NEWFS_ERROR_SEEK          ESPIPE     
#define NEWFS_ERROR_ISDIR         EISDIR
#define NEWFS_ERROR_NOSPACE       ENOSPC
#define NEWFS_ERROR_EXISTS        EEXIST
#define NEWFS_ERROR_NOTFOUND      ENOENT
#define NEWFS_ERROR_UNSUPPORTED   ENXIO
#define NEWFS_ERROR_IO            EIO     /* Error Input/Output */
#define NEWFS_ERROR_INVAL         EINVAL  /* Invalid Args */

typedef enum {
    NEWFS_DIR, NEWFS_FILE
} FILE_TYPE;

typedef int boolean;

struct custom_options {
	char*        device;
};


/******************************************************************************
* SECTION: 内存存储结构
*******************************************************************************/

struct newfs_dentry;
struct newfs_inode;
struct newfs_super;

/**
 * @brief 节点
 * 存储节点自身信息、指向的目录项及个数、匹配的目录项、数据块号及内存指针
 * 注意文件或目录类型，在更改内容时，需要修改对应的 blks
 */
struct newfs_inode {
    int         ino;                // 在 inode 位图中的下标
    int         size;               // 文件已占用空间
    int         link;               // 链接数，选做需要用到
    int         blks;               // 占用数据块个数
    int                     dir_cnt;

    struct newfs_dentry*    dentry;     // 指向该 inode 的dentry
    struct newfs_dentry*    dentrys;    // 所有目录项

    char*                block_pointer[NEWFS_DATA_PER_FILE]; // 数据块指针
    int                     block_pos[NEWFS_DATA_PER_FILE];     // 数据块号
};

/**
 * @brief 目录项
 * 存储目录项自身信息、指向的节点号、节点内容、父亲目录项、兄弟目录项
 */
struct newfs_dentry {
    char        fname[MAX_NAME_LEN];// 指向 ino 文件名
    FILE_TYPE   ftype;              // 指向 ino 文件类型
    int         ino;                // 指向的 ino 号

    struct newfs_inode*     inode;  // 指向 inode
    struct newfs_dentry*    parent;
    struct newfs_dentry*    brother;
};

/**
 * @brief 超级块
 */
struct newfs_super {
    int         fd;                 // 挂载的设备

    int         sz_io;
    int         sz_disk;
    int         sz_usage;

    int         max_ino;            // 最多节点数
    int         max_data_blks;      // 最多数据块数

    char*    map_inode;          // 指向内存中 inode 位图地址
    int         map_inode_blks;     // inode 位图占用块数
    int         map_inode_offset;   // inode 位图在磁盘中的偏移量

    char*    map_data;           // 指向内存中 data 位图地址
    int         map_data_blks;      // data 位图占用块数
    int         map_data_offset;    // data 位图在磁盘中的偏移量

    int         inode_offset;       // 索引节点起始地址
    int         data_offset;        // 数据块起始地址

    struct newfs_dentry* root_dentry;     // 根目录 dentry
    boolean        is_mounted;
};

/******************************************************************************
* SECTION: 设备存储结构
*******************************************************************************/


struct newfs_inode_d {
    int         ino;                // 在 inode 位图中的下标
    int         size;               // 文件已占用空间
    int         link;               // 链接数
    int         dir_cnt;
    FILE_TYPE   ftype;
    int         blks;               // 占用数据块个数
    int         block_pos[NEWFS_DATA_PER_FILE];     // 数据块号
};

struct newfs_dentry_d {
    char        fname[MAX_NAME_LEN];// 指向 ino 文件名
    FILE_TYPE   ftype;              // 指向 ino 文件类型
    int         ino;                // 指向的 ino 号
};

struct newfs_super_d {
    uint32_t    magic_num;          // 幻数
    int         sz_usage;

    int         max_ino;            // 最多节点数
    int         max_data_blks;      // 最多数据块数

    int         map_inode_blks;     // inode 位图占用块数
    int         map_inode_offset;   // inode 位图在磁盘中的偏移量

    int         map_data_blks;      // data 位图占用块数
    int         map_data_offset;    // data 位图在磁盘中的偏移量

    int         inode_offset;       // 索引节点起始地址
    int         data_offset;        // 数据块起始地址

};

#endif /* _TYPES_H_ */