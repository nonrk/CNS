/*
@Author:  nonrk
@Date:    2016/06/11
@File:    NeuronBase.cpp
@Use:     NeuronBase class
@Ver:     0.109
*/
#include "stdafx.h"
#include "NeuronBase.h"
#include "NeuronIO.h"
#include<process.h>
#define M         20//def neuron max number
#define UM_MSG   WM_USER+1
#define UM_DIS   WM_USER+2
HANDLE            g_tEvent;   //synchronization
CRITICAL_SECTION  g_tCritial;
NeuronBase::NeuronBase(){}
NeuronBase::~NeuronBase(){}

//def neuron struct
struct NeuronData {
	DWORD           n_Id;    //neuron thread ID
	DWORD           g_Id;    //所属组ID
	//string          n_md5Id; //MD5 encode neuron ID
	double          n_time;  //neuron Information integration unit:ms
	double          w_val;   //neuron threshold (-1<w_val<1)
	vector<DWORD>   n_exId;  //out neuron ID
	vector<DWORD>   n_imId;  //input neuron ID
	vector<double>  w_imVal; //input weight
	int             n_Max;   //neuron max link val
	//Init function
	void Init(double th, int ioNum)
	{
		n_time  = 1;
		w_val   = th;
		n_Max   = ioNum;
		n_exId.reserve(ioNum);
		n_imId.reserve(ioNum);
		w_imVal.reserve(ioNum);
	}
	~NeuronData() {

		n_exId.clear();
		n_imId.clear();
		w_imVal.clear();
		vector<DWORD>(n_exId).swap(n_exId);
		vector<DWORD>(n_imId).swap(n_imId);
		vector<double>(w_imVal).swap(w_imVal);
	}
	//insert IO neuron ID
	void insertId(int ex,DWORD eId)
	{
		switch (ex)
		{
		case 0:
			n_imId.push_back(eId);
			break;
		case 1:
			n_exId.push_back(eId);
			break;
		case 9:
			w_imVal.push_back(eId);
			break;
		default:
			break;
		}
	}

};

//get vector index in val
int getvecIndex(vector<DWORD>vec, DWORD val)//get vector index
{

	for (unsigned int i = 0; i < vec.size(); i++)
	{
		DWORD s = vec[i];
		if (s == val)
		{
			return i;
			break;
		}
	}
	return -1;
}
//rand weight or threshold -1 to 1
double getRand()
{
	SYSTEMTIME sys_time;
	GetLocalTime(&sys_time);
	srand(sys_time.wMilliseconds);
	int r = rand() % 2000;
	double rv = r / 1000.0 - 1;
	return rv;
}
//is new link?
int isNewLink(DWORD nId,vector<DWORD>vec)
{
	vector <DWORD>::iterator iter;
	iter = find(vec.begin(), vec.end(), nId);
	int result = iter ==vec.end() ? 0 : 1;
	return result;
}
//calculation out val
double outSum(vector<double>vecw, vector<double>vecim)
{
	int tp = vecw.size();
	double s = 0.0;
	for (int i=0;i<tp;i++)
	{
		s += vecw[i] * vecim[i];
	}
	return s;
}
//send between of neuron msg
bool sendMsg(DWORD myid,DWORD gid,double exval,vector<DWORD>veei)
{
	NeuronIO nio;
	if (veei.size()==0)
	{
		for (unsigned int i=0;i<=nio.gx_Group[gid].size();i++)
		{
			int ref=0;
			DWORD refId;
			for (unsigned j=0;j<=veei.size();j++)
			{
				if (nio.gx_Group[gid][i]== veei[j])
				{
					ref=1;
					break;
				}
				else
				{
					ref = -1;
					break;
				}				
			}
			if (ref == -1)
			{
				veei.push_back(refId = nio.gx_Group[gid][i]);
			}
			break;
		}
	}
	for (unsigned int i=0;i<=veei.size();i++)
	{
		
		if (getvecIndex(nio.gx_gEx[gid],myid)==-1)//判断是否组输出ID，是传到输出组，否组内消息发送 提取全局变量
		{
			if (!PostThreadMessage(veei[i], UM_MSG, (WPARAM)exval, (LPARAM)myid))
			{
				printf("post %d message failed,errno:%d\n",i, ::GetLastError());
			}
		}
		else
		{
			//1:根据gid获取输出组NID，
			for (unsigned int i=0;i<=nio.gx_gEx[gid].size();i++)
			{
				if (!PostThreadMessage(nio.gx_gEx[gid][i], UM_MSG, (WPARAM)exval, (LPARAM)myid))
				{
					printf("post %d message failed,errno:%d\n", i, ::GetLastError());
				}
			}
		}

	}
	
	return true;
}
//update weight
vector<double> weightDecay(vector<double>vecw) 
{
	double wcv = 0.001;
	vector<double> nw;
	for (unsigned int i = 0; i < vecw.size(); i++)
	{

		nw.push_back((vecw[i] - wcv));

	}
	return nw;
}
//neuron 
unsigned __stdcall neuronThread(void *Data)
{
	int    nowId = 0;
	double exout = 0.0;
	int       ps = 0;
	NeuronIO nio;

	DWORD   gId   = *(DWORD *)Data;
	vector <double> t_weight;
	vector <double> t_outval;
	DWORD   nId    = GetCurrentThreadId();
	int     otNum  = 0;
	//Init neuron
	NeuronData N;
	N.n_Id         = nId;
	N.Init(getRand(), 3);
	N.g_Id = gId;
	//CMd5 md5;
	//N.n_md5Id = md5.md5(dwId);
	//test neuron send msg
	printf("make neuron：%d\n", nId);
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
		if (!SetEvent(g_tEvent)) //set thread start event 
		{
			printf("set start event failed,err:%d\n", ::GetLastError());
			return 1;
		}
	while (true)
	{
		if (GetMessage(&msg, 0, 0, 0)) //get msg from message queue
		{
			switch (msg.message)
			{
			case UM_MSG:
				
				if (isNewLink(msg.lParam,N.n_imId)==0)
				{
					printf("is new link neuron\n");
					N.n_imId.push_back(msg.lParam);
					N.w_imVal.push_back(getRand());
					printf("new link neuron:%d weight:%f\n", msg.lParam, N.w_imVal[0]);
				}
				nowId = getvecIndex(N.n_imId, msg.lParam);
				printf("rec size:%d\n", N.n_imId.size());
				for (unsigned int i = 0; i <= N.n_imId.size(); i++)
				{
					DWORD s = N.n_imId[i];
					printf("rec index:%d rec old:%d\n", N.n_imId[i], msg.lParam);
					if (s == msg.lParam)
					{
						printf("test index:%d\n", i);
						break;
					}
				}

				printf("come form:%d\n", msg.lParam);
				printf("insert neuron:%d\n", N.n_imId[0]);
				t_weight.push_back(N.w_imVal[nowId]);
				t_outval.push_back(msg.lParam);
				N.w_imVal[nowId] = N.w_imVal[nowId] + 0.0001;
				for (unsigned int i = 0; i < N.w_imVal.size(); i++)
				{
					if (i!=nowId)
					{
						N.w_imVal[i] = N.w_imVal[i] - 0.001;
					}
				}
				
				printf("form %d msg\n", msg.lParam);

				exout = 0.0;
				ps = 0;
				exout = 2.0 / (1.0 + exp(-outSum(t_weight, t_outval))) - 1.0;
				printf("to msg\n");
				//sendMsg(N.n_Id, N.g_Id, exout, N.n_exId);
				//send
				printf("check gx_group %d\n", nio.gx_Group[N.g_Id].size());
				if (N.n_exId.size() == -1)
				{
					printf("check gx_group %d\n",N.g_Id);
					for (unsigned int i = 0; i <= nio.gx_Group[N.g_Id].size(); i++)
					{
						int ref = 0;
						DWORD refId;
						for (unsigned j = 0; j <= N.n_exId.size(); j++)
						{
							if (nio.gx_Group[N.g_Id][i] == N.n_exId[j])
							{
								ref = 1;
								break;
							}
							else
							{
								ref = -1;
								break;
							}
						}
						if (ref == -1)
						{
							N.n_exId.push_back(refId = nio.gx_Group[N.g_Id][i]);
						}
						break;
					}
				}
				for (unsigned int i = 0; i <= N.n_exId.size(); i++)
				{

					if (getvecIndex(nio.gx_gEx[N.g_Id], N.n_Id) == -1)//判断是否组输出ID，是传到输出组，否组内消息发送 提取全局变量
					{
						if (!PostThreadMessage(N.n_exId[i], UM_MSG, (WPARAM)exout, (LPARAM)N.n_Id))
						{
							printf("post %d message failed,errno:%d\n", i, ::GetLastError());
						}
					}
					else
					{
						//1:根据gid获取输出组NID，
						for (unsigned int i = 0; i < nio.gx_gEx[N.g_Id].size(); i++)
						{
							if (!PostThreadMessage(nio.gx_gEx[N.g_Id][i], UM_MSG, (WPARAM)exout, (LPARAM)N.n_Id))
							{
								printf("post %d message failed,errno:%d\n", i, ::GetLastError());
							}
						}
					}

				}
				//
				t_weight.clear();
				vector<double>(t_weight).swap(t_weight);
				t_outval.clear();
				vector<double>(t_outval).swap(t_outval);
				break;
			case WM_QUIT:
				return 0; // 退出线程
			}
		}
	}
	return 0;
}
//create neuron
vector<DWORD> NeuronBase::createNeuron(DWORD gId)
{
	unsigned tId[M];//test data
	vector<DWORD> data;
	DWORD g_id = gId;
	g_tEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	InitializeCriticalSection(&g_tCritial);

	HANDLE  handle[M];
	int i = 0;
	while (i < M)
	{
		handle[i] = (HANDLE)_beginthreadex(NULL, 0, neuronThread, &g_id, 0, &tId[i]);
		WaitForSingleObject(g_tEvent, INFINITE); 
		data.push_back(tId[i]);
		g_group.push_back(tId[i]);
		Sleep(100);
		i++;
	}
	printf("create ok\n");
	printf("start %d neuron\n", data[0]);
	PostThreadMessage(data[0], WM_USER + 1, (WPARAM)0.5, data[1]);
	WaitForMultipleObjects(M, handle, TRUE, INFINITE);
	//printf("WaitForMultipleObjects ok\n");
	CloseHandle(g_tEvent);
	printf("CloseHandle ok\n");
	DeleteCriticalSection(&g_tCritial);
	printf("DeleteCriticalSection ok\n");
	
	return data;
	

}