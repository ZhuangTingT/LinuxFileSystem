#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <string.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <mutex>
#include <pthread.h>
#include <fcntl.h>
#include <string>
#include <time.h>
#include <math.h>
#include <iostream>
#include <sstream>
using namespace std;

#pragma once
#pragma pack(4)

#define BLOCK_SIZE 4096
#define INODE_NUM 1024
#define BLOCK_NUM 2048

extern string root_path;
extern int root_inode_set;
extern struct shared_mem_st *shared_stuff;

extern pthread_mutex_t m_i;
extern pthread_mutex_t m_b;

struct Super_bolck
{
	int inode_num; // inode个数
	int block_num; // 块总数
	int free_inode_num; // 空闲inode数
	int free_block_num; // 空闲块数
	int block_size; // 块的大小，4096字节
};

struct Inode
{
	int block_location[12]; // 每个文件分配的盘块位置
	int size; // 字节数
	time_t st_time; // 创建时间
	int i_mode; // 文件类型：0文件，1目录
};

struct shared_mem_st
{
	struct Super_bolck superblock;
	bool i_map[INODE_NUM];
	bool b_map[BLOCK_NUM];
	struct Inode inodes[INODE_NUM];
	char blocks[BLOCK_NUM][BLOCK_SIZE];
	int read_count[INODE_NUM];
};

void init_superblock(int, int , int, int);

int find_free_inode(int*); // 
int new_inode(int*, int, int);
void free_inode(int);

int find_free_block(int ,int*); // 
void free_block(int, int*);

void unlink_sem_r(int);
void unlink_sem_w(int);

void read_file(int, string &); // 
int get_by_name(int*, string, string);
int get_by_path(int, string, int*); 
int write(int, string &, int); // 

void del_by_dir(int, string);

int Mkdir(int &, string);
void Rmdir(int, string);
void Mv(int, string, string);
int Open(int, string);
void Edit(int, string);
void Rm(int &, string);
void Cd(int &, string);
void Cat(int, string);
void Ls(int, string, int);
void Lsc(int);
/*
g++ -c fileSystem.cpp -lpthread
g++ fileSystem.o systemInit.cpp -o sys.out -lpthread
g++ fileSystem.o user.cpp -o user.out -lpthread
*/
