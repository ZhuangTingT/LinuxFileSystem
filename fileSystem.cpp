#include "fileSystem.h"
using namespace std;

shared_mem_st *shared_stuff = 0;
string root_path = "/";
int root_inode_set = 0;

pthread_mutex_t m_i;
pthread_mutex_t m_b;

// void init_superblock() : 初始化超级块
void init_superblock(int inode_num, int block_num, int free_inode_num, int free_block_num)
{
	shared_stuff->superblock.inode_num = inode_num;
	shared_stuff->superblock.block_num = block_num;
	shared_stuff->superblock.free_inode_num = free_inode_num;
	shared_stuff->superblock.free_block_num = free_block_num;
	shared_stuff->superblock.block_size = BLOCK_SIZE;
}

int find_free_inode(int *inode_set)
{
	pthread_mutex_lock(&m_i);
	// 无空闲inode
	if(shared_stuff->superblock.free_inode_num == 0)
	{
		pthread_mutex_unlock(&m_i);
		cout << "无空闲Inode节点。" << endl;
		return 0;
	}
	shared_stuff->superblock.free_inode_num--;
	bool *p = shared_stuff->i_map;
	for(int i = 0; i < INODE_NUM; i++)
	{
		if(!p[i])
		{
			p[i] = true;
			*inode_set = i;
			break;
		}
	}
	pthread_mutex_unlock(&m_i);
	return 1;
}

// void new_inode() : 申请inode节点
int new_inode(int *inode_set, int size, int i_mode)
{
	if(find_free_inode(inode_set))
	{
		struct Inode *p = shared_stuff->inodes;
		p[*inode_set].size = size;
		time(&(p[*inode_set].st_time));
		p[*inode_set].i_mode = i_mode;
		return 1;
	}
	else
		return 0;
}

void free_inode(int inode_set)
{
	bool *p = shared_stuff->i_map;
	p[inode_set] = false;
	shared_stuff->inodes[inode_set].size = 0;
}

int find_free_block(int num, int block_set[])
{
	pthread_mutex_lock(&m_b);
	if(shared_stuff->superblock.free_inode_num < num)
	{
		pthread_mutex_unlock(&m_b);
		cout << "无空闲数据盘块。" << endl;
		return 0;
	}
	
	shared_stuff->superblock.free_inode_num -= num;
	int count = 0;
	for(int i = 0; (i < BLOCK_NUM)&&(count != num); i++)
	{
		bool *p = shared_stuff->b_map;
		if(!p[i])
		{
			p[i] = true;
			block_set[count] = i;
			count++;
		}
	}
	pthread_mutex_unlock(&m_b);
	return 1;
}

void free_block(int num, int *block_set)
{
	bool *p = shared_stuff->b_map;
	for(int i = 0; i < num; i++)
	{
		p[block_set[i]] = false;
	}
}

void unlink_sem_r(int inode_set)
{
	stringstream ss;
	ss << inode_set;
	string r;
	ss >> r;
	r.append("r");
	sem_unlink(r.data());
}

void unlink_sem_w(int inode_set)
{
	stringstream ss;
	ss << inode_set;
	string w;
	ss >> w;
	w.append("w");
	sem_unlink(w.data());
}

// 读取inode_set对应的文件内容
void read_file(int inode_set, string &content)
{
	sem_t *sem_r, *sem_w;
	stringstream ss1;
	ss1 << inode_set;
	string r;
	ss1 >> r;
	r.append("r");
	sem_r = sem_open(r.data(), O_CREAT, 0666, 1);
	stringstream ss;
	ss << inode_set;
	string w;
	ss >> w;
	w.append("w");
	sem_w = sem_open(w.data(), O_CREAT, 0666, 1);

	sem_wait(sem_r);
	if(shared_stuff->read_count[inode_set] == 0)
	{
		sem_wait(sem_w);
	}
	shared_stuff->read_count[inode_set]++;
	sem_post(sem_r);
	sleep(2);
	struct Inode inode = shared_stuff->inodes[inode_set];
	for(int i = 0; i < inode.size/BLOCK_SIZE+1; i++)
	{
		int block_set = inode.block_location[i];
		char *p = shared_stuff->blocks[block_set];
		content.append(p);
	}
	sem_wait(sem_r);
	shared_stuff->read_count[inode_set]--;
	if(shared_stuff->read_count[inode_set] == 0)
		sem_post(sem_w);
	sem_post(sem_r);
	unlink_sem_r(inode_set);
	unlink_sem_w(inode_set);
}

// 查看目录文件，找二级目录/文件的Inode节点
// inode_set: 二级目录/文件的Inode节点号
// str: 二级目录名/文件名
// content: 目录文件内容
int get_by_name(int *inode_set, string str, string content)
{
	char *name, *temp, *p;
	char *content_ = new char[content.length()+1];
	strcpy(content_, content.data());
	temp = strtok_r(content_, ":#", &p); // temp : inode节点号
	// flag: 判断路径是否合法
	int flag = 0;
	while(temp)
	{
		name = strtok_r(NULL, ":#", &p);
		// 找到下一级/文件
		if(strcmp(name, str.data()) == 0)
		{
			*inode_set = atoi(temp);
			flag = 1;
			break;
		}
		temp = strtok_r(NULL, ":#", &p);
	}
	if(content_)
		delete []content_;

	if(flag == 0)
		return 0; // 无该二级目录或文件
	else
		return 1;
}

// 寻找路径path对应的索引节点，path中的每一级都是目录名
// 函数返回 0 表示路径不合法
int get_by_path(int cur_inode_set, string path, int *inode_set)
{
	// “/”开头为绝对路径，否则为相对路径
	if(path[0] == '/')
		*inode_set = root_inode_set;
	else
		*inode_set = cur_inode_set;
	struct Inode temp_inode = shared_stuff->inodes[*inode_set];

	// 对路径进行分割，根据目录获得inode
	char *path_ = new char[path.length()+1];
	strcpy(path_, path.data());
	char *str, *p;
	str = strtok_r(path_, "/", &p); // str : 下一级目录文件名
	// flag: 判断路径是否合法
	int flag = 0;
	while(str)
	{
		if(temp_inode.i_mode != 1)
		{
			break;
		}
		// 补 '.'
		string s = str;
		if(str[0] != '.')
		{
			s = ".";
			s = s+str;
		}
		// content : 目录信息
		string content = "";
		read_file(*inode_set, content);
		// 分割目录信息，遍历content寻找下一级子目录的Inode节点号
		flag = get_by_name(inode_set, s, content);
		temp_inode = shared_stuff->inodes[*inode_set];
		str = strtok_r(NULL, "/", &p);
	}
	if(flag == 0)
	{
		cout << "路径或文件名不合法。" << endl;
		return 0;
	}

	if(path_)
		delete []path_;
	return 1;
}

// 将内容写入inode_set对应的文件末尾
// inode_set : 文件索引节点，content : 写入内容，flag : 0 需要新盘块/ 1 有旧盘块
int write(int inode_set, string &content, int flag)
{
	sem_t  *sem_w;
	stringstream ss;
	ss << inode_set;
	string w;
	ss >> w;
	w.append("w");
	sem_w = sem_open(w.data(), O_CREAT, 0666, 1);

	sem_wait(sem_w);
	//sleep(3);
	struct Inode *p = shared_stuff->inodes;
	// 检查空闲盘块是否充足，申请数据盘块
	int block_num = -1; // 最后一个盘块在Inode.blocks中的位置
	int left = 0;       // 剩余字节
	int *block_set;     // block_set : 空闲盘块
	int new_num = 0;    // 申请的空闲盘块数

	if(p[inode_set].size == 0)
		flag = 0;

	// 最后一个盘块剩余的字节
	if(flag == 1)
	{
		block_num = p[inode_set].size/BLOCK_SIZE;
		left = BLOCK_SIZE - p[inode_set].size%BLOCK_SIZE;
	}
	// 若最后一个盘块的内存不充足，检查剩余数据盘块是否充足
	if(left < (content.length()+1))
	{
		// 更新空闲盘块数量
		new_num = (content.length()+1-left)/BLOCK_SIZE+1;
		block_set = new int [new_num];

		// 申请数据盘块
		if( !find_free_block(new_num, block_set) )
		{
			cout << "缺少数据盘块。" << endl;
			delete []block_set;
			return 0;
		}
	}

	// 有旧盘块
	if(flag == 1)
	{
		// last_block_set : 文件最后一个盘块的位置
		int last_block_set = p[inode_set].block_location[block_num];
		// block : 文件末尾
		char *block = shared_stuff->blocks[last_block_set];
		block = block + p[inode_set].size%BLOCK_SIZE - 1;
		strcpy(block, "#");
		block++;
		strcpy(block, content.substr(0,left).data());
		
		if(new_num != 0)
		{
			p[inode_set].size += left;
			// 新盘块存放的内容
			content = content.substr(left);
		}
		else
		{
			p[inode_set].size += (content.length()+1); 
		}
	}
	
	if(new_num != 0)
	{
		// 按盘块的长度写入内存
		char (*blocks)[BLOCK_SIZE] = shared_stuff->blocks;
		for(int i = 1; i <= new_num; i++)
		{
			strcpy(blocks[block_set[i-1]], content.substr((i-1)*BLOCK_SIZE, BLOCK_SIZE).data());
			p[inode_set].block_location[block_num+i] = block_set[i-1];
		}
		p[inode_set].size = p[inode_set].size + content.length()+1;
		delete []block_set;
	}
	sem_post(sem_w);
	unlink_sem_w(inode_set);
	return 1;
}

// 在当前目录下创建二级目录，name: [目录名]
int Mkdir(int &cur_inode_set, string name)
{
	// 获得Inode节点号,并设置Inode节点属性,size = 0,i_mode = 1(1目录)
	int inode_set = 0;
	if( !new_inode(&inode_set, 0, 1) )
		return 0;
	// "inode_set:.#cur_inode_set:.."
	string content = to_string(inode_set);
	content.append(":.#"); // 本级
	content.append(to_string(cur_inode_set));
	content.append(":.."); // 上一级
	// 找新盘块，写入数据，并在Inode中记录盘块位置
	if( !write(inode_set, content, 0) )
	{
		// 数据盘块不足，释放Inode节点
		free_inode(inode_set);
		return 0;
	}
	
	// 新建的目录不是根节点时，把二级目录索引节点号及名字写入父目录中
	if(inode_set != root_inode_set)
	{
		// "inode_set:name"
		content = to_string(inode_set);
		content.append(":.");
		content.append(name);
		// 把索引节点号及名字写入父目录中
		if( !write(cur_inode_set, content, 1) )
		{
			// 数据盘块不足，释放Inode节点
			struct Inode inode = shared_stuff->inodes[inode_set];
			free_block(inode.size/BLOCK_SIZE+1, inode.block_location);
			free_inode(inode_set);
			return 0;
		}
	}
	return 1;
}

void del_by_dir(int inode_set, string dir)
{
	char *name, *temp, *p;
	char *dir_ = new char[dir.length()+1];
	strcpy(dir_, dir.data());
	temp = strtok_r(dir_, ":#", &p); // temp : inode节点号
	// flag: 判断路径是否合法
	int flag = 0;
	while(temp)
	{
		name = strtok_r(NULL, ":#", &p);
		// 当前文件是目录时递归删除目录
		if(name[0] == '.')
		{
			string next_dir;
			read_file(atoi(temp), next_dir);
			int pos = next_dir.find(":..#");
			if(pos != -1)
			{
				next_dir = next_dir.substr(pos+4);
				del_by_dir(atoi(temp), next_dir);
			}
			else
				dir = "";
		}
		Rm(inode_set, name);
		temp = strtok_r(NULL, ":#", &p);
	}
	if(dir_)
		delete []dir_;
}

void Rmdir(int cur_inode_set, string name)
{
	string cur_dir;
	read_file(cur_inode_set, cur_dir);
	int inode_set = -1;
	if( get_by_name(&inode_set, name, cur_dir) )
	{
		string dir;
		read_file(inode_set, dir);
		int pos = dir.find(":..#");
		// 目录不为空时清空目录下的内容
		if(pos != -1)
		{
			dir = dir.substr(pos+4);
			del_by_dir(inode_set, dir);
		}
		else
			dir = "";
		// 删除目录
		Rm(cur_inode_set, name);
	}
}

void Mv(int cur_inode_set, string name1, string name2)
{
	int inode_set;
	string cur_dir;
	read_file(cur_inode_set, cur_dir);
	if(get_by_name(&inode_set, name1, cur_dir))
	{
		// block_num_bef/block_num_aft: gaiming前后的数据盘快数
		int block_num_bef = (cur_dir.length()+1)/BLOCK_SIZE + 1;
		string temp = "#";
		temp = temp + to_string(inode_set) + ":" + name1;
		cur_dir = cur_dir.replace(cur_dir.find(temp.data()), temp.length(), "");
		// 在末尾补上新名
		cur_dir.append("#");
		cur_dir.append(to_string(inode_set)+":"+name2);
		int block_num_aft = (cur_dir.length()+1)/BLOCK_SIZE + 1;
		
		// sem_w
		sem_t  *sem_w;
		stringstream ss;
		ss << cur_inode_set;
		string w;
		ss >> w;
		w.append("w");
		sem_w = sem_open(w.data(), O_CREAT, 0666, 1);
		sem_wait(sem_w);
		
		// 判断是否需要释放数据盘块
		int *loc = shared_stuff->inodes[cur_inode_set].block_location;
		if(block_num_aft < block_num_bef)
		{
			bool *p = shared_stuff->b_map;
			for(int i = block_num_aft; i < block_num_bef; i++)
			{
				p[loc[i]] = false;
			}
		}
		// 判断是否需要申请数据盘块
		if(block_num_aft > block_num_bef)
		{	
			if( !find_free_block(block_num_aft-block_num_bef, &(loc[block_num_bef])) )
				return;
		}
		// 写回数据
		char (*blocks)[BLOCK_SIZE] = shared_stuff->blocks;
		for(int i = 1; i <= block_num_aft; i++)
		{
			strcpy(blocks[loc[i-1]], cur_dir.substr((i-1)*BLOCK_SIZE, BLOCK_SIZE).data());
		}
		shared_stuff->inodes[cur_inode_set].size = cur_dir.length()+1;
		sem_post(sem_w);
		unlink_sem_w(cur_inode_set);
	}
	else
		cout << "文件或目录不存在" << endl;
}

// 创建文件，name: [文件名]
int Open(int cur_inode_set, string name)
{
	// 获得Inode节点号,并设置Inode节点属性, size = 0, i_mode = 0(0文件)
	int inode_set = 0;
	if( !new_inode(&inode_set, 0, 0) )
		return 0;	

	// "inode_set:name"
	string content = to_string(inode_set);
	content.append(":");
	content.append(name);
	// 在目录中记录盘块位置
	if( !write(cur_inode_set, content, 1) )
	{
		// 数据盘块不足，释放Inode节点
		free_inode(inode_set);
		return 0;
	}
	return 1;
}

void Edit(int cur_inode_set, string name)
{
	int inode_set = 0;
	string cur_dir;
	read_file(cur_inode_set, cur_dir);
	// 根据当前目录获得文件的索引节点号
	if( !get_by_name(&inode_set, name, cur_dir) )
	{
		cout << "文件不存在" << endl;
		return;
	}

	string content = "";
	cout << "请输入添加的内容('#'表示回车)：" << endl;
	cin.ignore();
	getline(cin, content);
	// 找新盘块，写入数据，并在Inode中记录盘块位置
	if( !write(inode_set, content, 1) )
	{
		return;
	}
}

// 删除文件
void Rm(int &dir_inode_set, string name)
{
	int inode_set;
	string dir;
	read_file(dir_inode_set, dir);
	// 判断文件是否存在
	if(get_by_name(&inode_set, name, dir))
	{
		struct Inode inode = shared_stuff->inodes[inode_set];
		if(inode.size != 0)
			free_block(inode.size/BLOCK_SIZE+1, inode.block_location);
		// free_block
		free_inode(inode_set);
		
		// block_num_bef/block_num_aft: 删除目录条前后的数据盘快数
		int block_num_bef = (dir.length()+1)/BLOCK_SIZE + 1;
		string temp = "#";
		temp = temp + to_string(inode_set) + ":" + name;
		dir = dir.replace(dir.find(temp.data()), temp.length(), "");
		int block_num_aft = (dir.length()+1)/BLOCK_SIZE + 1;
		
		// sem_w
		sem_t  *sem_w;
		stringstream ss;
		ss << dir_inode_set;
		string w;
		ss >> w;
		w.append("w");
		sem_w = sem_open(w.data(), O_CREAT, 0666, 1);
		sem_wait(sem_w);
		// 判断是否需要释放数据盘块
		int *loc = shared_stuff->inodes[dir_inode_set].block_location;
		if(block_num_aft < block_num_bef)
		{
			bool *p = shared_stuff->b_map;
			for(int i = block_num_aft; i < block_num_bef; i++)
			{
				p[loc[i]] = false;
				loc[i] = 0;
			}
		}
		// 写回数据
		char (*blocks)[BLOCK_SIZE] = shared_stuff->blocks;
		for(int i = 1; i <= block_num_aft; i++)
		{
			strcpy(blocks[loc[i-1]], dir.substr((i-1)*BLOCK_SIZE, BLOCK_SIZE).data());
		}
		shared_stuff->inodes[dir_inode_set].size = dir.length()+1;
		sem_post(sem_w);
		unlink_sem_w(dir_inode_set);
	}
}

// 打印文件内容
void Cat(int cur_inode_set, string name)
{
	string cur_dir;
	int inode_set = 0;
	read_file(cur_inode_set, cur_dir);
	if( get_by_name(&inode_set, name, cur_dir) )
	{
		string content;
		read_file(inode_set, content);
		int pos = content.find("#");
		while(pos != -1)
		{
			content.replace(pos, 1, "\n");
			pos = content.find("#");
		}
		cout << content << endl;
	}
}

void Cd(int &cur_inode_set, string path)
{
	int inode_set = -1;
	// 判断路径是否合法
	if(get_by_path(cur_inode_set, path, &inode_set))
	{
		cur_inode_set = inode_set;
	}
}

void Ls(int cur_inode_set, string cur_name, int count)
{
	string dir;
	read_file(cur_inode_set, dir);
	for(int i = 0; i < count; i++)
		cout << "\t";
	cout << "|" << cur_name << endl;
	
	int pos = dir.find(":..#");
	if(pos != -1)
	{
		dir = dir.substr(pos+4);
		// 输出目录下的其他文件
		char *name, *temp, *p;
		char *dir_ = new char[dir.length()+1];
		strcpy(dir_, dir.data());
		temp = strtok_r(dir_, ":#", &p); // temp : inode节点号
		while(temp)
		{
			name = strtok_r(NULL, ":#", &p);
			if(name[0] == '.')
			{
				// 搜索子目录
				Ls(atoi(temp), name, count+1);
			}
			else
			{
				// 文件

				for(int i = 0; i < count+1; i++)
					cout << "\t";
				cout << "|" << name << endl;
			}
			temp = strtok_r(NULL, ":#", &p);
		}
		if(dir_)
			delete []dir_;
	}
	else
	{
		return;
	}
}

void Lsc(int cur_inode_set)
{
	string cur_dir;
	read_file(cur_inode_set, cur_dir);

	char *cur_dir_ = new char [cur_dir.length()+1];
	strcpy(cur_dir_, cur_dir.data());
	char* str = strtok(cur_dir_, ":#");
	while(str)
	{
		cout << str << " ";
		str = strtok(NULL, ":#");
		cout << str << endl;
		str = strtok(NULL, ":#");
	}
	delete []cur_dir_;
}
/*
g++ -c shm_com_sem.cpp
g++ shm_com_sem.o files.cpp -o f.out
g++ shm_com_sem.o cus.cpp -o c.out
*/
