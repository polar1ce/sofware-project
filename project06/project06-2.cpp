#include<iostream>
#include "./gdal/gdal_priv.h"
#pragma comment(lib, "gdal_i.lib")
#include<time.h>

using namespace std;

//RGB ==> IHS 正变换矩阵
const float tran1[3][3] = { {1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f},
						{-sqrt(2.0f) / 6.0f, -sqrt(2.0f) / 6.0f, sqrt(2.0f) / 3.0f},
						{1.0f / sqrt(2.0f), -1.0f / sqrt(2.0f), 0} };
//IHS ==> RGB 逆变换矩阵
const float tran2[3][3] = { {1.0f, -1.0f / sqrt(2.0f), 1.0f / sqrt(2.0f)},
						{1.0f, -1.0f / sqrt(2.0f), -1.0f / sqrt(2.0f)},
						{1.0f, sqrt(2.0f), 0} };

//图像分块融合函数，startY代表从哪一行开始，blockLen代表每块的长度
int process(int startX, int startY, int blockXLen, int blockYLen, GDALDataset* poMulDS, GDALDataset* poPanDS, GDALDataset* poFusDS,
	float* bandR, float* bandG, float* bandB, float* bandI, float* bandH, float* bandS) {
	//获取图像像素
	poMulDS->GetRasterBand(1)->RasterIO(GF_Read,
		startX, startY, blockXLen, blockYLen, bandR, blockXLen, blockYLen, GDT_Float32, 0, 0);
	poMulDS->GetRasterBand(2)->RasterIO(GF_Read,
		startX, startY, blockXLen, blockYLen, bandG, blockXLen, blockYLen, GDT_Float32, 0, 0);
	poMulDS->GetRasterBand(3)->RasterIO(GF_Read,
		startX, startY, blockXLen, blockYLen, bandB, blockXLen, blockYLen, GDT_Float32, 0, 0);
	poPanDS->GetRasterBand(1)->RasterIO(GF_Read,
		startX, startY, blockXLen, blockYLen, bandI, blockXLen, blockYLen, GDT_Float32, 0, 0);

	//逐个像素处理
	for (int i = 0; i < blockXLen * blockYLen; i++) {
		//RGB转换为IHS
		bandH[i] = bandR[i] * tran1[1][0] + bandG[i] * tran1[1][1] + bandB[i] * tran1[1][2];
		bandS[i] = bandR[i] * tran1[2][0] + bandG[i] * tran1[2][1] + bandB[i] * tran1[2][2];
		//IHS逆转换为RGB
		bandR[i] = bandI[i] * tran2[0][0] + bandH[i] * tran2[0][1] + bandS[i] * tran2[0][2];
		bandG[i] = bandI[i] * tran2[1][0] + bandH[i] * tran2[1][1] + bandS[i] * tran2[1][2];
		bandB[i] = bandI[i] * tran2[2][0] + bandH[i] * tran2[2][1] + bandS[i] * tran2[2][2];
	}

	//输出图像像素
	poFusDS->GetRasterBand(1)->RasterIO(GF_Write,
		startX, startY, blockXLen, blockYLen, bandR, blockXLen, blockYLen, GDT_Float32, 0, 0);
	poFusDS->GetRasterBand(2)->RasterIO(GF_Write,
		startX, startY, blockXLen, blockYLen, bandG, blockXLen, blockYLen, GDT_Float32, 0, 0);
	poFusDS->GetRasterBand(3)->RasterIO(GF_Write,
		startX, startY, blockXLen, blockYLen, bandB, blockXLen, blockYLen, GDT_Float32, 0, 0);

	return 0;
}

int main()
{
	clock_t start, finish;
	double totaltime;
	start = clock();

	//输入图像
	GDALDataset* poMulDS;
	GDALDataset* poPanDS;
	//输出图像
	GDALDataset* poFusDS;
	//输入图像路径
	const char* mulPath = "Mul_large.tif";
	const char* panPath = "Pan_large.tif";
	//输出图像的路径
	const char* fusPath = "Fus_large.tif";
	//输入图像的宽度和高度
	int imgXlen, imgYlen;
	//图像波段数
	int mulBandNum, panBandNum;
	//图像内存存储
	float *bandR, *bandG, *bandB;
	float *bandI, *bandH, *bandS;
	
	//注册驱动
	GDALAllRegister();

	//打开图像
	poMulDS = (GDALDataset*)GDALOpenShared(mulPath, GA_ReadOnly);
	poPanDS = (GDALDataset*)GDALOpenShared(panPath, GA_ReadOnly);

	//找不到图像返回错误信息
	if (poMulDS == NULL || poPanDS == NULL) {
		cout << "...source img do not exist..." << endl;
		return 0;
	}

	//获取图像宽度，高度，波段数
	imgXlen = poMulDS->GetRasterXSize();
	imgYlen = poMulDS->GetRasterYSize();
	mulBandNum = poMulDS->GetRasterCount();
	panBandNum = poPanDS->GetRasterCount();

	//输出获取的结果
	cout << "...Inputing..." << endl;
	cout << "IMG " << mulPath << endl;
	cout << "Band Number:" << mulBandNum << endl;
	cout << "IMG " << panPath << endl;
	cout << "Band Number:" << panBandNum << endl;
	cout << "X Length:" << imgXlen << endl;
	cout << "Y Length:" << imgYlen << endl;

	//创建输出图像
	poFusDS = GetGDALDriverManager()->GetDriverByName("GTiff")->Create(
			fusPath, imgXlen, imgYlen, mulBandNum, GDT_Byte, NULL);
	
	cout << "...Process Begin..." << endl;

	int i, j;
	//从这里开始修改，分段处理，设置每块长宽
	int blockLen = 256;
	//分成(imgXlen/256)*(imgYlen/256)块，每块大小256*256
	int blockX = imgXlen / blockLen;
	int blockY = imgYlen / blockLen;
	int block = blockX * blockY;

	//根据图像的宽度和高度分配内存
	bandR = (float*)CPLMalloc(blockLen * blockLen * sizeof(float));
	bandG = (float*)CPLMalloc(blockLen * blockLen * sizeof(float));
	bandB = (float*)CPLMalloc(blockLen * blockLen * sizeof(float));
	bandI = (float*)CPLMalloc(blockLen * blockLen * sizeof(float));
	bandH = (float*)CPLMalloc(blockLen * blockLen * sizeof(float));
	bandS = (float*)CPLMalloc(blockLen * blockLen * sizeof(float));
	
	cout << "...Total " << block << " Block. Please Wait..." << endl;

	//判断是否存在剩余像素没有处理
	int leftX = imgXlen - blockX * blockLen;
	int leftY = imgYlen - blockY * blockLen;

	//逐块处理
	for (i = 0; i < blockY; i++) {
		cout << "...Processing Block " << (i + 1) * blockX << "/" << block << "..." << endl;
		for (j = 0; j < blockX; j++) {
			process(j * blockLen, i * blockLen, blockLen, blockLen, poMulDS, poPanDS, poFusDS, bandR, bandG, bandB, bandI, bandH, bandS);
		}
	}
	//处理右侧剩余像素
	if (leftX != 0) {
		cout << "...Processing Left X Pixel..." << endl;
		for (i = 0; i < blockY; i++) {
			process(blockX * blockLen, i * blockLen, leftX, blockLen, poMulDS, poPanDS, poFusDS, bandR, bandG, bandB, bandI, bandH, bandS);
		}
	}
	//处理底部剩余像素
	if (leftY != 0) {
		cout << "...Processing Left Y Pixel..." << endl;
		for (j = 0; j < blockX; j++) {
			process(j * blockLen, blockY * blockLen, blockLen, leftY, poMulDS, poPanDS, poFusDS, bandR, bandG, bandB, bandI, bandH, bandS);
		}
		//处理右下角剩余像素
		if (leftX != 0) {
			cout << "...Processing Last Pixel..." << endl;
			process(blockX * blockLen, blockY * blockLen, leftX, leftY, poMulDS, poPanDS, poFusDS, bandR, bandG, bandB, bandI, bandH, bandS);
		}
	}

	//清除内存
	CPLFree(bandR);
	CPLFree(bandG);
	CPLFree(bandB);
	CPLFree(bandI);
	CPLFree(bandH);
	CPLFree(bandS);
	//关闭dataset
	GDALClose(poFusDS);
	GDALClose(poPanDS);
	GDALClose(poMulDS);

	cout << "...Complete..." << endl;
	
	finish = clock();
	totaltime = (double)(finish - start) / CLOCKS_PER_SEC;
	cout << "Running Time:" << totaltime << "s" << endl;

	return 0;
}