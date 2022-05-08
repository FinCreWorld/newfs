# 实现的文件系统中的所有命令使用方式

## 挂载

需要满足磁盘中存在ddriver驱动，并指定一个目录为挂载目录，这里假设指定的目录为 ./tests/mnt

执行命令

```bash
./build/newfs --device=/root/ddriver -f -d -s ./tests/mnt
```

```bash
[root@localhost newfs]# ./build/newfs --device=/root/ddriver -f -d -s ./tests/mnt 
FUSE library version: 2.9.9
nullpath_ok: 0
nopath: 0
utime_omit_ok: 0
unique: 1, opcode: INIT (26), nodeid: 0, insize: 56, pid: 0
INIT: 7.27
flags=0x003ffffb
max_readahead=0x00020000
```

打印文件系统日志，此时另开一个终端，进入 ./tests/mnt 目录下执行操作

## 创建目录

```bash
mkdir dir_name
```

```bash
[root@localhost mnt]# mkdir dir0
[root@localhost mnt]# mkdir dir1
[root@localhost mnt]# ls
dir0 dir1
[root@localhost mnt]#
```

## 创建文件

 ```bash
touch file_name
 ```

```bash
[root@localhost mnt]# touch file0
[root@localhost mnt]# touch file1
[root@localhost mnt]# ls
dir0 dir1 file0 file1
[root@localhost mnt]#
```

## 进入目录

```bash
cd dir_name
```

需要保证目录名存在，否则报错

```bash
[root@localhost mnt]# ls
dir0 dir1 file0 file1
[root@localhost mnt]# cd dir0
[root@localhost dir0]# cd dir0
bash: cd: dir0: No such file or directory
[root@localhost dir0]#
```

## 删除目录

```bash
rmdir [dir_list] 或 rm -rf [dir_list]
```

-r表示递归删除子目录，-f表示删除前不询问，需保证目录存在否则报错。

 ```bash
[root@localhost mnt]# ls
dir0 dir1 file0 file1
[root@localhost mnt]# rm -rf dir0
[root@localhost mnt]# ls
dir1 file0 file1
[root@localhost mnt]# rmdir dir1
[root@localhost mnt]# ls
file0 file1
 ```

## 删除文件

```bash
rm -f [file_list] 或 rm [file_list]
```

-f 表示在删除文件前不询问。

 ```bash
[root@localhost mnt]# ls
file0 file1
[root@localhost mnt]# rm file0 file1
rm: remove regular empty file 'file0'? y
rm: remove regular empty file 'file1'? y
[root@localhost mnt]# ls
[root@localhost mnt]#
 ```

## 向文件写入数据

```bash
echo xxx > file_name 或使用 tee 命令
```

```bash
[root@localhost mnt]# ls
a b
[root@localhost mnt]# echo "Hello World" > a
[root@localhost mnt]# cat a
Hello World
[root@localhost mnt]#
[root@localhost mnt]# tee -a b

Hello JiangChenyang!

Hello JiangChenyang!

[root@localhost mnt]# cat b

Hello JiangChenyang!

[root@localhost mnt]#
```

（8）   查看文件内容

```bash
cat file_name
```

效果如上

##  卸载

```bash
fusermount -u ./tests/mnt
```

```bash
[root@localhost newfs]# fusermount -u tests/mnt/

[root@localhost newfs]#
```

