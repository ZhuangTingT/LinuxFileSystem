#include "fileSystem.h"
using namespace std;

string cur_path = ".";
int cur_inode_set = 0;

void help()
{
	cout << "创建目录----mkdir [目录名]" << endl;
	cout << "删除目录----rmdir [目录名]" << endl;
	cout << "修改名称----mv [旧目录名] [新目录名]" << endl;
	cout << "创建文件----open [文件名]" << endl;
	cout << "修改文件----edit [文件名]" << endl;
	cout << "删除文件----rm [文件名]" << endl;
	cout << "查看系统目录----ls" << endl;
	cout << "查看当前目录----lsc" << endl;
	cout << "访问目录----cd [路径]" << endl;
	cout << "显示文件内容---cat [文件名]" << endl;
	cout << "退出系统----quit" << endl;
}

int main()
{
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

	cout << "------输入“help”查看功能------" << endl;
	string op;
	while(1)
	{
		cout << "\033[33m~:\033[0m";
		cin >> op;
		if(strcmp(op.data(), "mkdir") == 0)
		{
			string path;
			cin >> path;
			Mkdir(cur_inode_set, path);
		}
		else if(strcmp(op.data(), "rmdir") == 0)
		{
			string path;
			cin >> path;
			string s = ".";
			path = s+path;
			Rmdir(cur_inode_set, path);
		}
		else if(strcmp(op.data(), "mv") == 0)
		{
			string name1, name2;
			cin >> name1 >> name2;
			Mv(cur_inode_set, name1, name2);
		}
		else if(strcmp(op.data(), "open") == 0)
		{
			string name;
			cin >> name;
			Open(cur_inode_set, name);
		}
		else if(strcmp(op.data(), "edit") == 0)
		{
			string name;
			cin >> name;
			Edit(cur_inode_set, name);
		}
		else if(strcmp(op.data(), "rm") == 0)
		{
			string name;
			cin >> name;
			Rm(cur_inode_set, name);
		}
		else if(strcmp(op.data(), "cd") == 0)
		{
			string path;
			cin >> path;
			Cd(cur_inode_set, path);
		}
		else if(strcmp(op.data(), "cat") == 0)
		{
			string name;
			cin >> name;
			Cat(cur_inode_set, name);
		}
		else if(strcmp(op.data(), "ls") == 0)
		{
			Ls(root_inode_set, root_path, 0);
		}
		else if(strcmp(op.data(), "lsc") == 0)
		{
			Lsc(cur_inode_set);
		}
		else if(strcmp(op.data(), "help") == 0)
		{
			help();
		}
		else if(strcmp(op.data(), "quit") == 0)
		{
			break;
		}
		else
		{
			cout << "无该操作。" << endl;
		}
		cin.sync();
	}
	
	if((shmdt(shared_stuff)) < 0)
	{
		perror("shmdt");
		exit(1);
	}
	//sem_unlink(queue_mutex);
	//sem_unlink(queue_empty);
	return 0;
}
