#include<iostream>
#include<fstream>
#include<stdlib.h>
#include<ctime>
#include<bitset>
#include<vector>
#include<string>
#include<sstream>

#include<boost/dynamic_bitset.hpp>

#include<windows.h>

#include<immintrin.h>  // AVX

#include<pthread.h>
#include<semaphore.h>
#include<mutex>
#include<condition_variable>

using namespace std;
using namespace boost;

//--------------------------------------函数声明--------------------------------------

dynamic_bitset<> get_head(size_t index);
void output();
void GrobnerGE();
void db_GrobnerGE();  // 动态位集版本的特殊高斯消去，未来考虑补上稀疏矩阵版本
void readData_bitset();
void readData_reverse_bitset();
void reverse_output();


const int THREAD_NUM = 7;  // 线程数量
int n = 0;  // 矩阵大小
const int k = 1;

int ek_num;  // 非零消元子个数
int et_num;  // 导入被消元行行数
vector<dynamic_bitset<>> eks;
dynamic_bitset<>* ets = nullptr;
int* lp_ets = nullptr;

long long head, tail, freq;

//------------------------------------数据导入工具------------------------------------

string dir = "./data/t5/";

stringstream ss;

//------------------------------------线程数据结构------------------------------------

typedef struct {
	int t_id;  // 线程 id
	int tasknum;  // 任务数量
}PT_EliminationParam;

typedef struct {
	int t_id; //线程 id
	size_t index;
}PT_GetHeadParam;

//-------------------------------------信号量定义-------------------------------------
sem_t sem_gethead;
sem_t sem_workerstart[THREAD_NUM]; // 每个线程有自己专属的信号量
sem_t sem_workerend[THREAD_NUM];

sem_t sem_leader;
sem_t sem_Divsion[THREAD_NUM - 1];
sem_t sem_Elimination[THREAD_NUM - 1];

//------------------------------------barrier定义-------------------------------------
pthread_barrier_t barrier_gethead;
pthread_barrier_t barrier_Elimination;

//-------------------------------条件变量和互斥锁定义---------------------------------
pthread_mutex_t _mutex;
pthread_cond_t _cond;

//--------------------------------------线程函数--------------------------------------

void* PT_Elimination(void* param) {
	PT_EliminationParam* tt = (PT_EliminationParam*)param;
	int t_id = tt->t_id;
	bool find_flag = false;  // true说明存在首项
	dynamic_bitset<>* temp = nullptr;
	/*
	for (int i = t_id; i < et_num; i += THREAD_NUM)
	{
		find_flag = false;
		while (ets[i].any())
		{
			//size_t first = ets[i].find_first();  // lp(E[i])
			pthread_mutex_lock(&_mutex);  // 使用互斥锁保证数据间同步
			//cout << t_id << " lock1" << endl;
			size_t index = ets[i].find_first();
			for (int i = 0; i < eks.size(); i++)  // R[lp(E[i])
			{
				if (index == eks[i].find_first())
				{
					temp = new dynamic_bitset<>(eks[i]);
					find_flag = true;
					break;
				}
			}
			//pthread_mutex_unlock(&_mutex);  // 解锁
			//cout << t_id << " unlock1" << endl;
			if (find_flag)
			{
				//pthread_mutex_lock(&_mutex);  // 使用互斥锁保证数据间同步
				ets[i] ^= *temp;  // 考虑拆开
				//pthread_mutex_unlock(&_mutex);  // 解锁
			}
			else
			{
				//pthread_mutex_lock(&_mutex);  // 使用互斥锁保证数据间同步
				//cout << t_id << " lock2" << endl;
				eks.push_back(ets[i]);
				//pthread_mutex_unlock(&_mutex);  // 解锁
				//cout << t_id << " unlock2" << endl;
				break;
			}
			//pthread_mutex_unlock(&_mutex);  // 解锁
		}
		cout << t_id << " waiting" << endl;
		//pthread_barrier_wait(&barrier_Elimination);
	}
	*/

	for (int i = t_id; i < et_num; i += THREAD_NUM)
	{
		while (ets[i].any())
		{
			find_flag = false;
			size_t index = ets[i].find_first();
			for (int j = 0; j < eks.size(); j++)  // R[lp(E[i])
			{
				if (index == eks[j].find_first())
				{
					ets[i] ^= eks[j];
					find_flag = true;
					break;
				}
			}
			if (!find_flag)
			{
				pthread_mutex_lock(&_mutex);  // 使用互斥锁保证数据间同步
				eks.push_back(ets[i]);
				pthread_mutex_unlock(&_mutex);  // 解锁
				break;
			}
		}
	}

	//pthread_barrier_wait(&barrier_Elimination);
	pthread_exit(nullptr);
	return nullptr;
}

void* watch() {
	pthread_mutex_lock(&_mutex);
	while (true)
	{
		pthread_cond_wait(&_cond, &_mutex);
	}
	pthread_mutex_unlock(&_mutex);
	return nullptr;
}



//-------------------------------------PThread算法------------------------------------
void PT_GrobnerGE()
{
	pthread_cond_init(&_cond, nullptr);
	pthread_mutex_init(&_mutex, nullptr);
	pthread_barrier_init(&barrier_Elimination, nullptr, THREAD_NUM);
	//int tasknum = eks.size() / THREAD_NUM;
	pthread_t* handles = (pthread_t*)malloc(THREAD_NUM * sizeof(pthread_t));  // 为线程句柄分配内存空间
	PT_EliminationParam* param = (PT_EliminationParam*)malloc(THREAD_NUM * sizeof(PT_EliminationParam));  // 存储线程参数
	for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
		param[t_id].t_id = t_id;
		pthread_create(&handles[t_id], nullptr, PT_Elimination, (void*)&param[t_id]);
	}

	for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
		pthread_join(handles[t_id], nullptr);
	}
	
	for (int i = 0; i < et_num; i++)
	{
		/*
		while (ets[i].any())
		{
			count1++;
			//size_t first = ets[i].find_first();  // lp(E[i])
			temp = get_head(ets[i].find_first());
			//temp = get_head(lp_ets[i]);  // R[lp(E[i])]，即消元子中与被消元行i中首项相同的项
			if (!(temp.size() == 0))
			{
				ets[i] ^= temp;
				//lp_ets[i] = ets[i].find_first();
				count2++;
			}
			else
			{
				count3++;
				eks.push_back(ets[i]);
				break;
			}
		}
		*/
	}
	pthread_mutex_destroy(&_mutex);
	pthread_barrier_destroy(&barrier_Elimination);
}

//------------------------------------计算辅助函数------------------------------------

dynamic_bitset<> get_head(size_t index) {  // 获取首项
	for (int i = 0; i < eks.size(); i++)  // R[lp(E[i])
	{
		if (index == eks[i].find_first())
		{
			return eks[i];
		}
	}
	return dynamic_bitset<>(0);
}

//--------------------------------计算辅助函数-PThread--------------------------------
void* PT_Rotation_gethead_Inthread(void* param) {
	PT_GetHeadParam* pa = (PT_GetHeadParam*)param;
	int t_id = pa->t_id;
	size_t index = pa->index;
	dynamic_bitset<>* ret = nullptr;
	for (int i = t_id; i < eks.size(); i+= THREAD_NUM)  // R[lp(E[i])
	{
		if (index == eks[i].find_first())
		{
			ret = new dynamic_bitset<>(eks[i]);
			break;
		}
	}
	sem_post(&sem_gethead);// 通知 leader, 已完成消去任务
	pthread_exit(ret);
	return ret;
}

dynamic_bitset<> PT_Static_Rotation_get_head(size_t index) {  // 获取首项
	int tasknum = eks.size() / THREAD_NUM;
	pthread_t* handles = (pthread_t*)malloc(THREAD_NUM * sizeof(pthread_t));  // 为线程句柄分配内存空间
	PT_GetHeadParam* param = (PT_GetHeadParam*)malloc(THREAD_NUM * sizeof(PT_GetHeadParam));  // 存储线程参数
	void** ret = new void*[THREAD_NUM];
	sem_init(&sem_gethead, 0, 0);
	if (tasknum < THREAD_NUM)  // 如果一个线程的任务数量还不如线程数量多，那么就直接采用主线程
	{
		for (int i = 0; i < eks.size(); i++)  // R[lp(E[i])
		{
			if (index == eks[i].find_first())
			{
				return eks[i];
			}
		}
		return dynamic_bitset<>(0);
	}
	else {
		for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
			param[t_id].t_id = t_id;
			param[t_id].index = index;
		}
		//创建线程
		for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
			pthread_create(&handles[t_id], nullptr, PT_Rotation_gethead_Inthread, (void*)&param[t_id]);
		}
		for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
			pthread_join(handles[t_id], &ret[t_id]);
		}
		//主线程睡眠（等待所有的工作线程完成此轮任务）
		for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
			sem_wait(&sem_gethead);
		}
		//被唤醒，处理返回值
		for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
			if (ret[t_id]) return *(dynamic_bitset<>*)(ret[t_id]);
		}
		return dynamic_bitset<>(0);
	}

}

//--------------------------------------算法主体--------------------------------------

void GrobnerGE()
{
	int count1 = 0, count2 = 0, count3 = 0, count4 = 0;
	dynamic_bitset<> temp = dynamic_bitset<>(n);
	for (int i = 0; i < et_num; i++)
	{
		//QueryPerformanceCounter((LARGE_INTEGER*)&head);
		while (ets[i].any())
		{
			count1++;
			//size_t first = ets[i].find_first();  // lp(E[i])
			temp = get_head(ets[i].find_first());
			//temp = get_head(lp_ets[i]);  // R[lp(E[i])]，即消元子中与被消元行i中首项相同的项
			if (!(temp.size() == 0))
			{
				ets[i] ^= temp;
				//lp_ets[i] = ets[i].find_first();
				count2++;
			}
			else
			{
				count3++;
				eks.push_back(ets[i]);
				break;
			}
		}
		/*
		QueryPerformanceCounter((LARGE_INTEGER*)&tail);
		cout << "while: " << (tail - head) * 1000.0 / freq
			<< "ms" << endl;
		*/

	}
	cout << count1 << " " << count2 << " " << count3 << endl;
}

void getheadOpt_GrobnerGE()
{
	dynamic_bitset<> temp = dynamic_bitset<>(n);
	for (int i = 0; i < et_num; i++)
	{
		while (ets[i].any())
		{
			//size_t first = ets[i].find_first();  // lp(E[i])
			temp = PT_Static_Rotation_get_head(ets[i].find_first());  // R[lp(E[i])]，即消元子中与被消元行i中首项相同的项
			if (!(temp.size() == 0))
			{
				ets[i] ^= temp;
			}
			else
			{
				eks.push_back(ets[i]);
				break;
			}
		}
	}
}

void test_GE() {
	bool find_flag = false;  // true说明存在首项
	dynamic_bitset<>* temp = nullptr;
	for (int i = 0; i < et_num; i ++)
	{
		while (ets[i].any())
		{
			find_flag = false;
			size_t index = ets[i].find_first();
			for (int j = 0; j < eks.size(); j++)  // R[lp(E[i])
			{
				if (index == eks[j].find_first())
				{
					ets[i] ^= eks[j];
					find_flag = true;
					break;
				}
			}
			if (!find_flag)
			{
				eks.push_back(ets[i]);
				break;
			}
		}
	}
}

void my_xor_AVX(dynamic_bitset<>& db1, dynamic_bitset<>& db2) {
	
	for (int i = 0; i < db1.size(); i += 8)
	{

	}

}

//------------------------------------输出调试函数------------------------------------

void output()
{
	ofstream outp(dir + "output.txt");
	for (int i = 0; i < et_num; i++)
	{
		for (int j = 0; j < n; j++)
		{
			if (ets[i].test(j))
			{
				outp << j << " ";
			}
		}
		outp << endl;
	}
	outp.close();
}

void reverse_output()
{
	ofstream outp(dir + "output.txt");
	for (int i = 0; i < et_num; i++)
	{
		for (int j = 0; j < n; j++)
		{
			if (ets[i].test(j))
			{
				outp << n - j - 1 << " ";
			}
		}
		outp << endl;
	}
	outp.close();
}

//------------------------------------数据读取函数------------------------------------

void readData_bitset()
{
	string inek;
	stringstream ss_inek;
	ifstream inElimKey(dir + "elimkey.txt");  // 消元子
	ifstream inElimTar(dir + "elimtar.txt");  // 被消元行
	int inek_loc, p_ek = 0;  // 用于数据读入
	while (true)  // 读取消元子
	{
		getline(inElimKey, inek);
		ss_inek = stringstream(inek);
		while (ss_inek >> inek)
		{
			inek_loc = stoi(inek);
			//cout << inek_loc << " ";
			eks[p_ek].set(inek_loc);
		}
		if (inek.empty())
		{
			//cout << "ek_complete" << endl;
			break;
		}
		//cout << eks[p_ek] << endl;
		p_ek++;
	}

	string inet;
	stringstream ss_inet;
	int inet_loc, p_et = 0;  // 用于数据读入
	while (true)  // 读取被消元行
	{
		getline(inElimTar, inet);
		ss_inet = stringstream(inet);
		while (ss_inet >> inet)
		{
			inet_loc = stoi(inet);
			//cout << inet_loc << " ";
			ets[p_et].set(inet_loc);
		}
		if (inet.empty())
		{
			//cout << "et_complete" << endl;
			break;
		}
		//cout << ets[p_et] << endl;
		p_et++;
	}
}

void readData_reverse_bitset()
{  // 倒序读入数据
	string inek;
	stringstream ss_inek;
	ifstream inElimKey(dir + "elimkey.txt");  // 消元子
	ifstream inElimTar(dir + "elimtar.txt");  // 被消元行
	int inek_loc, p_ek = 0;  // 用于数据读入
	while (true)  // 读取消元子
	{
		getline(inElimKey, inek);
		ss_inek = stringstream(inek);
		while (ss_inek >> inek)
		{
			inek_loc = stoi(inek);
			//cout << inek_loc << " ";
			eks[p_ek].set(n - inek_loc - 1);
		}
		if (inek.empty())
		{
			//cout << "ek_complete" << endl;
			break;
		}
		//cout << eks[p_ek] << endl;
		p_ek++;
	}

	string inet;
	stringstream ss_inet;
	int inet_loc, p_et = 0;  // 用于数据读入
	while (true)  // 读取被消元行
	{
		getline(inElimTar, inet);
		ss_inet = stringstream(inet);
		while (ss_inet >> inet)
		{
			inet_loc = stoi(inet);
			//cout << inet_loc << " ";
			ets[p_et].set(n - inet_loc - 1);
		}
		if (inet.empty())
		{
			//cout << "et_complete" << endl;
			break;
		}
		//cout << ets[p_et] << endl;
		p_et++;
	}
}

void init() {
	eks.clear();
	ets = new dynamic_bitset<>[et_num];

	for (int i = 0; i < ek_num; i++)
	{
		eks.push_back(dynamic_bitset<>(n));
	}

	for (int i = 0; i < et_num; i++)
	{
		ets[i] = dynamic_bitset<>(n);
		
	}

	// readData_bitset(eks, ets);  // 初始化消元子和被消元行阵列

	readData_reverse_bitset();  // 逆序初始化消元子和被消元行阵列

	//cout << eks[0].any() << endl;

	lp_ets = new int[et_num];
	for (int i = 0; i < et_num; i++)
	{
		lp_ets[i] = ets[i].find_first();//计算被消元行首项
		//cout << lp_ets[i] << " ";
	}
}


int main() {
	ifstream inParam(dir + "param.txt");
	inParam >> n;  // 导入矩阵大小
	inParam >> ek_num;  // 导入非零消元子个数
	inParam >> et_num;  // 导入被消元行行数

	cout << "矩阵大小为" << n << "，消元子个数为" << ek_num << "，被消元行行数为" << et_num << endl;

	QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
	//-----------------------------------------------------------------

	init();
	QueryPerformanceCounter((LARGE_INTEGER*)&head);
	GrobnerGE();  // 动态位集存储的矩阵的特殊高斯消去
	QueryPerformanceCounter((LARGE_INTEGER*)&tail);
	cout << "db_GrobnerGE: " << (tail - head) * 1000.0 / freq
		<< "ms" << endl;

	//-----------------------------------------------------------------

	init();
	QueryPerformanceCounter((LARGE_INTEGER*)&head);
	getheadOpt_GrobnerGE();  // 动态位集存储的矩阵的特殊高斯消去
	QueryPerformanceCounter((LARGE_INTEGER*)&tail);
	cout << "getheadOpt_GrobnerGE: " << (tail - head) * 1000.0 / freq
		<< "ms" << endl;

	//-----------------------------------------------------------------

	init();
	QueryPerformanceCounter((LARGE_INTEGER*)&head);
	PT_GrobnerGE();  // 动态位集存储的矩阵的特殊高斯消去
	QueryPerformanceCounter((LARGE_INTEGER*)&tail);
	cout << "PT_GrobnerGE: " << (tail - head) * 1000.0 / freq
		<< "ms" << endl;
	reverse_output();
	//-----------------------------------------------------------------

	init();
	QueryPerformanceCounter((LARGE_INTEGER*)&head);
	test_GE();  // 动态位集存储的矩阵的特殊高斯消去
	QueryPerformanceCounter((LARGE_INTEGER*)&tail);
	cout << "test_GE: " << (tail - head) * 1000.0 / freq
		<< "ms" << endl;
	//reverse_output();
	//-----------------------------------------------------------------
	/*
	init();
	int c = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&head);
	for (int i = 0; i < 47820; i++)
	{
		if (c < et_num)
		{
			ets[c++].find_first();
		}
		else
		{
			c -= et_num;
		}
	}
	QueryPerformanceCounter((LARGE_INTEGER*)&tail);
	cout << "gethead: " << (tail - head) * 1000.0 / freq
		<< "ms" << endl;

	//-----------------------------------------------------------------
	
	init();
	c = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&head);
	for (int i = 0; i < 47820; i++)
	{
		if (c < et_num)
		{
			ets[c++] ^= eks[ek_num - 1];
		}
		else
		{
			c -= et_num;
		}
	}
	QueryPerformanceCounter((LARGE_INTEGER*)&tail);
	cout << "^: " << (tail - head) * 1000.0 / freq
		<< "ms" << endl;

	//-----------------------------------------------------------------

	init();
	c = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&head);
	for (int i = 0; i < 47820; i++)
	{
		ets[0].any();
	}
	QueryPerformanceCounter((LARGE_INTEGER*)&tail);
	cout << "^: " << (tail - head) * 1000.0 / freq
		<< "ms" << endl;
	*/


	//output();


	
}


