#include "fileSystem.h"
using namespace std;

string cur_path = ".";
int cur_inode_set = 0;

int main()
{
	pthread_mutex_init(&m_i, NULL);
	pthread_mutex_init(&m_b, NULL);

	// 创建共享内存
	void *shared_memory = (void *)0;
	int shm_id = shmget((key_t)1234, 100*1024*1024, 0666|IPC_CREAT);
	if(shm_id < 0)
	{
		perror("shmget fail!\n");
		exit(1);
	}

	// 共享内存挂入进程
	shared_memory = shmat(shm_id, 0, 0);
	if(shared_memory < 0)
	{
		perror("shmat fail!\n");
		exit(1);
	}
	shared_stuff = (struct shared_mem_st*)shared_memory;

	// 初始化磁盘
	init_superblock(INODE_NUM, BLOCK_NUM, INODE_NUM, BLOCK_NUM);
	// 初始化根目录
	Mkdir(root_inode_set, root_path);
	Ls(root_inode_set, root_path, 0);

	string op = "";
	while(1)
	{
		cout << "退出请输入“quit”..." << endl;
		cin >> op;
		if(strcmp(op.data(), "quit") == 0)
			break;
	}
	
	if((shmdt(shared_stuff)) < 0)
	{
		perror("shmdt");
		exit(1);
	}
	return 0;
}
