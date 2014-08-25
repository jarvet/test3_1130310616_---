#include<iostream>
#include<stdio.h>
#include <stdlib.h>
#include <time.h>
#include "StreamBuffer.h"
using namespace std;

#define TEST_FRAME  //测试代码框架的正确性

void	GenDisOrder(int iSendOrder[],int iPacketNum)
{
    int i,j,k;

    for(i=0;i<iPacketNum;i++)//先产生顺序的序列：0，1,2，.....iPacketNum-1
        iSendOrder[i] = i;

	if( iPacketNum < 5)
		return;

    int n,temp;
    n =  rand()%(iPacketNum/5) +1;    //置乱的次数，最多20%*2个数据包
   for(i=0;i< n; i++)
   {//交换j、k两个数据包的顺序
         j = rand()%(iPacketNum/2) + 1;
         k = rand()%( iPacketNum - j);
        temp =  iSendOrder[j];
        iSendOrder[j] = iSendOrder[k];
        iSendOrder[k]=temp;
   }
}

int main()
{
	char srcfileName[500]="../media/source.mp3";
	char dstfileName[500]="../media/dest.mp3";
	FILE* fpSrcFile = NULL;
	FILE* fpDstFile = NULL;
	const int MTU =  1500;//最大传输单元，网络术语，表示一个数据包大最大尺寸，单位：字节
//	unsigned int iOffset;
	int iReadBytes = 0, iWantReadBytes;

    int  iContinueBytes;
    int iUseBytes;
	unsigned int iOutDataOffset;
	char  *pOutData;

	StreamBuffer MyBuffer;

	const int iMaxPacketNum = 20; //每次读入20个数据包，然后以乱序的形式发给排序模块（StreamBuffer类）
	int iSendOrder[iMaxPacketNum];//记录下发数据包的顺序
	unsigned int iPacketOffset[iMaxPacketNum];//记录每个数据包中第一个字节数据的偏移量
	unsigned int iPacketLen[iMaxPacketNum];//记录每个数据包中的数据长度
	char       (*pDataBuf)[MTU]; //数据包缓冲区区。创建方法1：指向一位数组的指针，下一步会为它一次性申请一个二维数组
	//char     *pDataBuf[iMaxPacketNum];//  创建方法2，指针数组，一步会为它多次申请空间，每次申请一个一维数组，释放也要多次
	int   iPacketNum;
	int i;
	int iPackNo;

    //--------------   added for experiment 03, begin ------------------------------
	int  iTotalPacketNum = 0;
	const int DiscardPacketInterval = 100;//每DiscardPacketInterval个数据包丢一个，建议数值：30~100 （srcfileName的文件尺寸足够大，会有丢失产生）
   //--------------   added for experiment 03, end ------------------------------

     srand(100);//用固定值初始化，会生成固定的随机数序列，方便测试程序，否则用srand( (unsigned)time( NULL ) );

	 pDataBuf = new char[iMaxPacketNum][MTU];//方法1
	 /*for( i=0;i<iMaxPacketNum;i++)//方法2
     {
         pDataBuf[i] = new char[MTU];
     }*/


	fpSrcFile = fopen(srcfileName, "rb");
	if( fpSrcFile == NULL )
	{
	  cout<<"Cann't open file: "<<srcfileName<<endl;
	  return 1;
	}

	fpDstFile = fopen(dstfileName, "wb");
	if( fpDstFile == NULL )
	{
	  cout<<"Cann't create file: "<< dstfileName <<endl;
	  return 2;
	}

	iWantReadBytes = 1024;

   do
   {
        iPacketNum = 0;
        for( i=0;i<iMaxPacketNum;i++)//初始化数据包长度为0，表示没有读入数据
            iPacketLen[i] = 0;//2014.07.11  iPacketNum->i
        //	iReadBytes = fread(pDataBuf[iPacketNum], 1, iWantReadBytes, fpSrcFile);
		do
		{
			iPacketOffset[iPacketNum] = ftell(fpSrcFile);
			iReadBytes = fread(pDataBuf[iPacketNum], 1, iWantReadBytes, fpSrcFile);
            iPacketLen[iPacketNum] = iReadBytes; //当前数据包读取成功，记录数据包长度，否则依旧是0
            iWantReadBytes =  (iPacketOffset[iPacketNum]+iPacketNum*iPacketNum)%500+400;//下一个数据包读取长度
            iPacketNum++;
        }  while( iReadBytes > 0 && iPacketNum < iMaxPacketNum );//读入一组数据包，如果文件结束：iReadBytes<1

        //把刚刚已经读入一组数据包，乱序下发给排序模块
        GenDisOrder(iSendOrder,iMaxPacketNum);
        for(i=0;i<iMaxPacketNum;i++)//只要长度不为0，就发给排序模块
        {
            iPackNo = iSendOrder[i];
            if(iPacketLen[iPackNo] > 0 )//有数据，给给排序模块
            {
                //--------------   added for experiment 03, begin ------------------------------
                iTotalPacketNum++;
                if( iTotalPacketNum%DiscardPacketInterval == DiscardPacketInterval-1)
                    continue;//不给缓冲区发送，相当于丢失
                //--------------   added for experiment 03, end ------------------------------

//#ifdef TEST_FRAME //正常情况，代码框架会产生数据有缺失的结果文件
                 iUseBytes = iPacketLen[iPackNo];
                 pOutData = pDataBuf[iPackNo];
                 iOutDataOffset = iPacketOffset[iPackNo];
//#else
				MyBuffer.ReceiveDate(iPacketOffset[iPackNo],iPacketLen[iPackNo],pDataBuf[iPackNo]);
                iContinueBytes = MyBuffer.ContinueBytes(iOutDataOffset, pOutData);
                iUseBytes =    iContinueBytes - 100;//假设用了一部分
                if( iContinueBytes  > 1024) //示例数值，可以调整
//#endif
                {
                   fseek(fpDstFile,iOutDataOffset,SEEK_SET);
                   fwrite(pOutData,iUseBytes,1,fpDstFile);
                   MyBuffer.RemoveData(iUseBytes);
                }
            }
        }
   }while(iReadBytes>0);//说明输入文件还有数据，继续....

	//输入结束，把缓冲区中剩余排好序的数据取出
    iContinueBytes = MyBuffer.ContinueBytes(iOutDataOffset, pOutData);
	if( iContinueBytes > 0 )
	{
		fseek(fpDstFile,iOutDataOffset,SEEK_SET);
		fwrite(pOutData,iContinueBytes,1,fpDstFile);
	}

    fclose(fpDstFile);
    fclose(fpSrcFile);



	 delete []pDataBuf;//方法1
	 /*for( i=0;i<iMaxPacketNum;i++) //方法2
     {
         delete []pDataBuf[i];
     }*/
   //★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
  //   可用的文件比对软件： BCompare，很好用，强烈推荐
  //★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
     return 0;
	}
