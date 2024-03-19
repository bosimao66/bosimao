

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//子项由两个字节构成
#define SUBITEM_SIZE (65536)
//地址范围0~4096，代表的块，4096 * 2 = 8192 字节
#define BLOCK_SIZE (8192)
#define ADDR_SCALE (4096)
#define ADDR_BIT_NUM (12)
//压缩后文件头部，识别码
#define HEAD_CODE (0xff00ff00)
//界限
#define SELECT_BY_CNT_NUM 11
//扩展字节数
#define EXPEND_BYTE (1024)


typedef struct __mark_obj{
	int mark;
	int cnt;
	int pos;
	int start;
	int len;
}mark_obj_t;
#define MARK_NUM  1024




int main()
{
	//最后一个字节落单
	unsigned char endSingleByteMark = 0;
	unsigned char endSingleByte = 0;

	int i, j, k;
	int readOk;
	int readLen;
	int twoByte;
	
	char buf8192[BLOCK_SIZE + EXPEND_BYTE] = {0};		//扩展一下，不然会有溢出风险
	int bitMap65536[SUBITEM_SIZE] = {0}; 

	mark_obj_t mark1024[MARK_NUM] = {0};
	mark_obj_t emptyMark = {0};
	int markCnt;

	unsigned char bufData8192[BLOCK_SIZE + EXPEND_BYTE] = {0};
	unsigned char bufShadow8192[BLOCK_SIZE] = {0};

	int shift, seek, base;
	int tmpInt, tmpInt2, tmpResult, tmpCnt;
	int writeToShadow;
	int shadowByteCnt;
	int toAddrCnt;
	int markTotleBitLen; //bit位长
	int addrTable_totleLen;
	int targetFile_totleLen;

	int needByte_headCode;
	int needByte_markCnt;
	int needByte_bitLen;	
	int needByte_mark;
	int needByte_tableLen;
	int needByte_targetFile;
	


	int readStart;
	unsigned char buf1024[1024] = {0};
	unsigned char buf32[32] = {0};

	int readPos;
	int nextReadLen;

	int decom_byteCnt;

	//文件操作
	FILE *from = fopen("from", "rb+");
	if(from == NULL)printf("======>from from\n");

	FILE *to = fopen("to", "wb+");
	if(to == NULL)printf("======>from to\n");
	
	readPos = 0;
	readOk = 1;



	printf(	"//================================================================\n"
			"//	压缩测试\n"
			"//================================================================\n");
	readPos = 0;
	readOk = 1;
while(readOk > 0)
{
	endSingleByteMark = 0;
	endSingleByte = 0;
	//读取
	readOk = fread(buf8192 , BLOCK_SIZE, 1, from);
	tmpInt = ftell(from);
	readLen = tmpInt - readPos;
	readPos = tmpInt;
	
	if(readLen % 2 == 1)	//最后一个字节落单
	{
		endSingleByteMark = 1;
		//printf("------------------------------------------------------------>readLen=%d\n",readLen );
		endSingleByte = (unsigned char)buf8192[readLen - 1];
	}
	for(i = 0; i < SUBITEM_SIZE; i++)
	{
		bitMap65536[i] = 0;
	}
	//统计
	for(i = 0; i < readLen - endSingleByteMark; i += 2)
	{
		twoByte = 0xff & buf8192[i];
		twoByte += (0xff & buf8192[i + 1]) << 8;
		bitMap65536[twoByte]++;
	}
	//mark
	markCnt = 0;
	for(i = 0; i < SUBITEM_SIZE; i++)
	{
		if(bitMap65536[i] >= SELECT_BY_CNT_NUM)
		{
			mark1024[markCnt].mark = i;
			mark1024[markCnt].cnt = bitMap65536[i];
			markCnt++;
		}
	}
	//显示
	printf("====================mark================\n");
	printf("--->markCnt=%d  SELECT_BY_CNT_NUM=%d\n", markCnt, SELECT_BY_CNT_NUM);
	for(i = 0; i < markCnt; i++)
	{
		printf("%d[%d-%04X-%d-%04X] \t", i, mark1024[i].mark, mark1024[i].mark, mark1024[i].cnt, (unsigned int)mark1024[i].cnt * ADDR_BIT_NUM);
		if((i + 1) % 5 == 0)
		{
			printf("\n");
		}
	}
	printf("\n");


	//存地址组（4-前导码 2-总长 2-mark个数 ){(3-位长度、2-mark、n-数据) ……}
	needByte_headCode = 4;
	needByte_tableLen = 2;
	needByte_markCnt = 2;
	needByte_bitLen = 3;	
	needByte_mark = 2;
	needByte_targetFile = 2;
	

	//mark1024[0].len = (needByte_bitLen + needByte_mark) * 8 + mark1024[0].cnt * ADDR_BIT_NUM;
	mark1024[0].len = mark1024[0].cnt * ADDR_BIT_NUM;
	mark1024[0].start = 0;
	mark1024[0].pos = mark1024[0].start;
	markTotleBitLen = mark1024[0].len;
	for(i = 1; i < markCnt; i++)
	{
		//mark1024[i].len = (needByte_bitLen + needByte_mark) * 8 + mark1024[i].cnt * ADDR_BIT_NUM;
		mark1024[i].len = mark1024[i].cnt * ADDR_BIT_NUM;
		mark1024[i].start = mark1024[i - 1].start + mark1024[i - 1].len;
		mark1024[i].pos = mark1024[i].start;
		markTotleBitLen += mark1024[i].len;
	}
	 

	//表的前面部分（存头部信息）						
	tmpInt =  (needByte_bitLen + needByte_mark) * markCnt +		
							needByte_headCode + needByte_targetFile + needByte_tableLen + needByte_markCnt;	
	//加上表后面部分（存的地址）
	addrTable_totleLen = (markTotleBitLen / 8 + 1) + tmpInt;



	#if 1
	seek = 0;
	//shift = 0;
	
	for(i = 0; i < markCnt; i++)
	{

		for(j = 0; j < needByte_bitLen; j++)  //needByte_bitLen不要大于4
		{
			bufData8192[seek + j] = (unsigned char)(((mark1024[i].len) >> (8 * j)) & (0xff));
			
		}
		seek += needByte_bitLen;
		for(j = 0; j < needByte_mark; j++)  //needByte_bitLen不要大于4
		{
			bufData8192[seek + j] = (unsigned char)(((mark1024[i].mark) >> (8 * j)) & (0xff));
			
		}
		seek += needByte_mark;
	
	}
	base = seek;
	printf("-------------------->seek=%d\n", seek);
	#endif

	//存地址组（数据)、存shadow
	#if 1
	shadowByteCnt = 0;
	toAddrCnt = 0;
	//seek = 0;
	tmpCnt = 0;
	for(k = 0; k < readLen - endSingleByteMark; k+=2)
	{
		twoByte = 0xff & (unsigned char)buf8192[k];
		twoByte += (0xff & (unsigned char)buf8192[k + 1]) << 8;	
		writeToShadow = 1;
		
		
		for(i = 0; i < markCnt; i++)
		{
			tmpInt2 = base * 8 + mark1024[i].pos;
			if(twoByte == mark1024[i].mark)
			{
				for(j = 0; j < ADDR_BIT_NUM; j++)
				{
					tmpInt = ((unsigned int)0x1 << j) & (k / 2);    //k/2  代表地址
					shift = tmpInt2 + j;
					if(tmpInt == 0)
					{
						bufData8192[shift / 8] &= ~((unsigned int)0x1 << (shift % 8));
					}
					else
					{
						bufData8192[shift / 8] |= ((unsigned int)0x1 << (shift % 8));
					}
					//BIT_PRO;
				}
				mark1024[i].pos += ADDR_BIT_NUM;
				toAddrCnt += 1;
				writeToShadow = 0;

				
				//if(mark1024[i].mark == 3387)printf("--------->cnt=%d k/2=%d bufData8192=%02X-%02X\n", tmpCnt++, k/2 , bufData8192[tmpInt2 / 8], bufData8192[(tmpInt2 + 11) / 8]);
				
				break;
			}
		}
		if(writeToShadow )
		{
			//printf("---->shadowByteCnt=%d k=%d\n", shadowByteCnt, k);
			//memset((void *)(bufShadow8192 + shadowByteCnt), buf8192[k], 1);
			//memset((void *)(bufShadow8192 + shadowByteCnt + 1), buf8192[k + 1], 1);
			bufShadow8192[shadowByteCnt] = (unsigned char)buf8192[k];
			bufShadow8192[shadowByteCnt + 1] = (unsigned char)buf8192[k + 1];
			shadowByteCnt += 2;

		}
	}
	
	if(endSingleByteMark)
	{
		bufShadow8192[shadowByteCnt++] = endSingleByte;
	}

	printf("=================shadow========================\n");
	printf("--->shadowByteCnt=%d toAddrCnt=%d *12=%d markTotleBitLen=%d /8+1=%d\n", shadowByteCnt, 
									toAddrCnt, toAddrCnt*12, markTotleBitLen, markTotleBitLen / 8 + 1);
								
	printf("--->target=%d origin=%d rate=%d%% tableLen=%d\n", shadowByteCnt + addrTable_totleLen, 
									readLen, 100 * (shadowByteCnt + addrTable_totleLen) / readLen, addrTable_totleLen);
	#endif
	
	//存入文件（4-前导码 2-总长 2-表总长 2-mark个数 ){(3-位长度、2-mark)……}、{（n1-数据)、(n2数据) ……}
	tmpInt = HEAD_CODE;
	targetFile_totleLen = addrTable_totleLen + shadowByteCnt;
	fwrite((const void *)&tmpInt, needByte_headCode, 1, to);
	fwrite((const void *)&targetFile_totleLen, needByte_targetFile, 1, to);
	fwrite((const void *)&addrTable_totleLen, needByte_tableLen, 1, to);
	fwrite((const void *)&markCnt, needByte_markCnt, 1, to);
	//10字节之后
	fwrite(bufData8192, (markTotleBitLen / 8 + 1) + (needByte_bitLen + needByte_mark) * markCnt, 1, to);
	fwrite(bufShadow8192, shadowByteCnt, 1, to);
	printf("=================to file msg========================\n");
	printf("--->HEAD_CODE=%08x addrTable_totleLen=%d bufData8192-len=%d shadowByteCnt=%d \n", HEAD_CODE, 
									addrTable_totleLen, markTotleBitLen / 8 + 1, shadowByteCnt);

}
	fclose(from);
	fclose(to);


//===========================================================================
//									解压测试
//===========================================================================

	
	printf(	"\n//================================================================\n"
			"//	解压测试"
			"\n//================================================================\n");
	//文件操作
	to = fopen("to", "rb+");
	if(to == NULL)printf("======>from to  rb+\n");

	FILE *decom = fopen("decom", "wb+");
	if(decom == NULL)printf("======>from decom  wb+\n");
	
	
	//读取
	readPos = 0;
	readOk = 1;
while(readOk > 0)
{
//==============================================================	
	//读取和解析
	//readStart = 0;
	fseek(to, readPos, SEEK_SET);
	readOk = fread(buf1024, 1024, 1, to);
	readLen = ftell(to) - readPos;

	if(readLen <= 0)
	{
		printf("========>fread to buf1024\n");
		break;
	}
	for(i = 0; i < readLen - 4; i++)		//针对与小端存储
	{
		if(buf1024[i] == 0x00 &
			buf1024[i + 1] == 0xff &
			buf1024[i + 2] == 0x00 &
			buf1024[i + 3] == 0xff )
		{
			printf("readStart=%d\n", i);
			readStart = i;
			break;
		}
	}

	targetFile_totleLen = (unsigned char)buf1024[readStart + 4];//目标总长
	targetFile_totleLen += (unsigned char)buf1024[readStart + 5] << 8;
	nextReadLen = targetFile_totleLen;
//==============================================================

	printf("-----------------------------------------\n");
	printf("nextReadLen=%d\n", nextReadLen);

	fseek(to, readPos, SEEK_SET);
	readOk = fread(buf8192, nextReadLen, 1, to);
	tmpInt = ftell(from);
	readLen = tmpInt - readPos;
	readPos = tmpInt;
	//readLen = ftell(to) - readStart;
	if(readLen <= 0)printf("========>fread to buf8192\n");
	//printf("---------->readOk=%d readLen=%d\n", readOk, readLen);

	//清空mark
	//mark_obj_t emptyMark = {0};
	for(i = 0; i < MARK_NUM; i++)		
	{
		mark1024[i] = emptyMark;
	}

	
	//解析【4-前导码】【2-字节总长】{【】
	/*存地址组（4-前导码 2-总长 2-mark个数 ){(3-位长度、2-mark、n-数据) ……}
	needByte_headCode = 4;
	needByte_tableLen = 2;
	needByte_markCnt = 2;
	needByte_bitLen = 3;	
	needByte_mark = 2;*/
	seek = needByte_headCode;
	targetFile_totleLen = (unsigned char)buf8192[seek];//目标总长
	targetFile_totleLen += (unsigned char)buf8192[seek + 1] << 8;
	//nextReadLen = targetFile_totleLen;
	//readPos += targetFile_totleLen;
	seek += needByte_targetFile;
	addrTable_totleLen = (unsigned char)buf8192[seek];//表总长
	addrTable_totleLen += (unsigned char)buf8192[seek + 1] << 8;
	seek += needByte_tableLen;

	decom_byteCnt = targetFile_totleLen - addrTable_totleLen;		//加上shadow byte
	markCnt = (unsigned char)buf8192[seek];//mark个数
	markCnt += (unsigned char)buf8192[seek + 1] << 8;
	seek += needByte_markCnt;
	//printf("==============================info=======================\n");
	printf("targetLen=%d tableLen=%d markCnt=%d readOk=%d\n", targetFile_totleLen, addrTable_totleLen, markCnt, readOk);
	for(i = 0; i < markCnt; i++)
	{
		//printf("-------->i=%d markCnt=%d seek=%d 0x%02x 0x%02x 0x%02x \n", i, markCnt, seek, 
		//					(unsigned char)buf8192[seek], (unsigned char)buf8192[seek + 1], (unsigned char)buf8192[seek +2]);
		
		mark1024[i].len = 0;
		for(j = 0; j < needByte_bitLen; j++)
		{
			mark1024[i].len += (unsigned char)buf8192[seek + j] << (j * 8);
		}
		seek += needByte_bitLen;
		//if(i == 0)printf("======================>len=%x\n", mark1024[0].len);

		mark1024[i].mark = 0;
		for(j = 0; j < needByte_mark; j++)
		{
			mark1024[i].mark += (unsigned char)buf8192[seek + j] << (j * 8);
		}
		seek += needByte_mark;
	}
	/*printf("===========================mark info=======================\n");
	printf("---->markCnt=%d\n", markCnt);
	for(i = 0; i < markCnt; i++)
	{
		printf("%d[%d-%08x]\t", i, mark1024[i].mark, mark1024[i].len);
		if((i + 1) % 5 == 0)
		{
			printf("\n");

		}
	}
	printf("\n");*/

	//利用地址信息和mark，重构源文件 from
	//占位
	for(i = 0; i < SUBITEM_SIZE; i++)
	{
		bitMap65536[i] = 1;
		//if(i > BLOCK_SIZE)break;
	}

	tmpInt = seek * 8;
	//printf(">>>>>>>>>>>>>>>>>>>>>>>>>seek=%d mark1024[0].len/8=%d\n",seek, mark1024[0].len / 12);
	tmpCnt = 0;
	for(i = 0; i < markCnt; i++)
	{
		if(i >= 1)
		{	
			tmpInt += mark1024[i - 1].len;
		
		}
		for(j = 0; j < mark1024[i].len; j+=ADDR_BIT_NUM)
		{
			tmpInt2 = 0;    	//地址，12位
			for(k = 0; k < ADDR_BIT_NUM; k++)
			{
				shift = tmpInt + j + k;
				tmpResult = (0x1 << (shift % 8)) & (unsigned char)(buf8192[shift / 8]);
				if(tmpResult > 0)
				{
					tmpInt2 += (0x1) << k;
				}
				
				
			}
			//if(i == 2)printf("----------------------->len=%d mark=%d cnt=%d tmpInt2=%d\n", mark1024[i].len, mark1024[i].mark, tmpCnt++, tmpInt2);
			bufData8192[tmpInt2 * 2] = (unsigned char)(mark1024[i].mark & 0xff);	//占位,一个位置，两个字节
			bufData8192[tmpInt2 * 2 + 1] = (unsigned char)((mark1024[i].mark >> 8) & 0xff);
			bitMap65536[tmpInt2 * 2] = 0;
			bitMap65536[tmpInt2 * 2 + 1] = 0;

			decom_byteCnt += 2;
			//if(tmpInt2 == 2)printf("========================>mark=%d i=%d<====================================\n", mark1024[i].mark, i);
		}
		
	

		
		//printf(">>>>>>>>>>>>>>>>>>>>>>tmpInt=%d\n", tmpInt);
	}

	//printf("===>bufData8192 =  %02x %02x %02x %02x \n", bufData8192[0],  bufData8192[1],  bufData8192[2],  bufData8192[3]);

	//shadow填充
	seek = addrTable_totleLen;
	tmpCnt = 0;
	for(i = 0; i < BLOCK_SIZE ; i++)
	{
		if(bitMap65536[i] == 1)
		{
			bufData8192[i] = (unsigned char)(buf8192[tmpCnt + seek]);
			tmpCnt++;
		}
	}

	//printf("buf8192= %02X %02X %02X %02X \n", (unsigned char)buf8192[seek + 0],  buf8192[seek + 1],  buf8192[seek + 2],  buf8192[seek + 3]);
	//printf("bitMap65536= %02X %02X %02X %02X %02X \n", bitMap65536[0],  bitMap65536[1],  bitMap65536[2],  bitMap65536[3],  bitMap65536[4]);

	//写入decom
	//fseek(decom, 0, SEEK_SET);
	fwrite(bufData8192, decom_byteCnt , 1, decom);
}



	fclose(decom);
	fclose(to);
	//printf("\n----------------------------------------------end\n");
}




