#include<iostream>
#include "./gdal/gdal_priv.h"
#pragma comment(lib, "gdal_i.lib")

using namespace std;

//max为卷积核最大维数
const int max = 5;
//knum为卷积核数
const int knum = 6;

//卷积处理函数，kernel为卷积核，para为该核的参数，dimen为该核的维数
float* convolution(int imgXlen, int imgYlen, float* buffTmp[2], float* kernel, float para1, float para2, int dimen);

int main()
{
	//输入图像
	GDALDataset* poSrcDS;
	//输出图像
	GDALDataset* poDstDS[knum];
	//输入图像路径
	const char* srcPath = "lena.jpg";
	//输出图像的路径
	const char* dstPath[knum] = { "k1.tif", "k2.tif", "k3.tif", "k4.tif", "k5.tif", "k6.tif"};
	//输入图像的宽度和高度
	int imgXlen, imgYlen;
	//图像波段数
	int bandNum;
	//图像内存存储
	float *buffTmp[2];
	//卷积核集合
	float k[knum][max][max] = {{	{0,1,0},
									{1,1,1},
									{0,1,0}										},
								{	{1,0,0,0,0},
									{0,1,0,0,0},
									{0,0,1,0,0},
									{0,0,0,1,0},
									{0,0,0,0,1}									},
								{	{-1,-1,-1},
									{-1, 8,-1},
									{-1,-1,-1}									},
								{	{-1,-1,-1},
									{-1, 9,-1},
									{-1,-1,-1}									},
								{	{-1,-1, 0},
									{-1, 0 ,1},
									{ 0, 1, 1}									},
								{	{0.0120, 0.1253, 0.2736, 0.1253, 0.0120},
									{0.1253, 1.3054, 2.8514, 1.3054, 0.1253},
									{0.2736, 2.8514, 6.2279, 2.8514, 0.2736},
									{0.1253, 1.3054, 2.8514, 1.3054, 0.1253},
									{0.0120, 0.1253, 0.2736, 0.1253, 0.0120}	}};
	//卷积核参数1
	float p1[knum] = { 0.2, 0.2, 1, 1, 1, 0.04 };
	//卷积核参数2
	float p2[knum] = { 0, 0, 0, 0, 128, 0 };
	//卷积核维数
	int d[knum] = { 3, 5, 3, 3, 3, 5 };

	//注册驱动
	GDALAllRegister();

	//打开图像
	poSrcDS = (GDALDataset*)GDALOpenShared(srcPath, GA_ReadOnly);

	//获取图像宽度，高度，波段数
	imgXlen = poSrcDS->GetRasterXSize();
	imgYlen = poSrcDS->GetRasterYSize();
	bandNum = poSrcDS->GetRasterCount();

	//输出获取的结果
	cout << "...Inputing..." << endl;
	cout << "IMG " << srcPath << " X Length:" << imgXlen;
	cout << ", Y Length:" << imgYlen << endl;
	cout << "Band Number:" << bandNum << endl;
	
	//根据图像的宽度和高度分配内存
	for (int i = 0; i < 2; i++)
		buffTmp[i] = (float*)CPLMalloc(imgXlen * imgYlen * sizeof(float));

	for (int i = 0; i < knum; i++) {
		cout << "Kernel " << i + 1 << ":" << endl;
		//创建输出图像
		poDstDS[i] = GetGDALDriverManager()->GetDriverByName("GTiff")->Create(
			dstPath[i], imgXlen, imgYlen, bandNum, GDT_Byte, NULL);
		//分波段处理图像
		for (int j = 0; j < bandNum; j++) {
			cout << "... ... band " << j + 1 << " processing ... ..." << endl;
			//获取图像像素
			poSrcDS->GetRasterBand(j + 1)->RasterIO(GF_Read,
				0, 0, imgXlen, imgYlen, buffTmp[0], imgXlen, imgYlen, GDT_Float32, 0, 0);
			//初始化输出像素，此处是为了防止图像边缘黑边
			for (int p = 0; p < imgYlen; p++)
				for (int q = 0; q < imgXlen; q++)
					buffTmp[1][p * imgXlen + q] = buffTmp[0][p * imgXlen + q];
			//调用卷积函数对图像进行处理
			convolution(imgXlen, imgYlen, buffTmp, *k[i], p1[i], p2[i], d[i]);
			//输出图片
			poDstDS[i]->GetRasterBand(j + 1)->RasterIO(GF_Write,
				0, 0, imgXlen, imgYlen, buffTmp[1], imgXlen, imgYlen, GDT_Float32, 0, 0);
		}
	}

	//清除内存
	for (int i = 0; i < 2; i++)
		CPLFree(buffTmp[i]);
	//关闭dataset
	for(int i = 0; i < knum; i++)
		GDALClose(poDstDS[i]);
	GDALClose(poSrcDS);

	cout << "...Complete..." << endl;
	return 0;
}

float* convolution(int imgXlen, int imgYlen, float* buffTmp[2], float* kernel, float para1, float para2, int dimen) {
	//暂存卷积计算结果
	float temp;
	//计算图像卷积边缘位置
	int edge = dimen / 2;
	//逐个像素处理
	for (int i = edge; i < imgYlen - edge; i++)
		for (int j = edge; j < imgXlen - edge; j++) {
			//初始化像素值
			temp = 0;
			//卷积实现，进行卷积运算
			for (int p = 0; p < dimen; p++)
				for (int q = 0; q < dimen; q++)
					temp += buffTmp[0][(i - edge + p) * imgXlen + (j - edge + q)] * kernel[p * max + q];
			//与给定参数进行计算
			temp *= para1;
			temp += para2;
			//超过像素范围的优化处理
			if (temp < 0)
				temp = 0;
			if (temp > 255)
				temp = 255;
			//像素赋值
			buffTmp[1][i * imgXlen + j] = temp;
		}
	//返回像素值
	return buffTmp[1];
}