#include<iostream>
#include "./gdal/gdal_priv.h"
#pragma comment(lib, "gdal_i.lib")

using namespace std;

int main()
{
	//输入图像
	GDALDataset* poMulDS;
	GDALDataset* poPanDS;
	//输出图像
	GDALDataset* poFusDS;
	//输入图像路径
	const char* mulPath = "American_MUL.bmp";
	const char* panPath = "American_PAN.bmp";
	//输出图像的路径
	const char* fusPath = "American_FUS.tif";
	//输入图像的宽度和高度
	int imgXlen, imgYlen;
	//图像波段数
	int mulBandNum, panBandNum;
	//图像内存存储
	float *bandR, *bandG, *bandB;
	float *bandI, *bandH, *bandS;
	//RGB ==> IHS 正变换矩阵
	float tran1[3][3] = { {1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f},
							{-sqrt(2.0f) / 6.0f, -sqrt(2.0f) / 6.0f, sqrt(2.0f) / 3.0f},
							{1.0f / sqrt(2.0f), -1.0f / sqrt(2.0f), 0} };
	//IHS ==> RGB 逆变换矩阵
	float tran2[3][3] = { {1.0f, -1.0f / sqrt(2.0f), 1.0f / sqrt(2.0f)},
							{1.0f, -1.0f / sqrt(2.0f), -1.0f / sqrt(2.0f)},
							{1.0f, sqrt(2.0f), 0} };
	//注册驱动
	GDALAllRegister();

	//打开图像
	poMulDS = (GDALDataset*)GDALOpenShared(mulPath, GA_ReadOnly);
	poPanDS = (GDALDataset*)GDALOpenShared(panPath, GA_ReadOnly);

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
	
	//根据图像的宽度和高度分配内存
	bandR = (float*)CPLMalloc(imgXlen * imgYlen * sizeof(float));
	bandG = (float*)CPLMalloc(imgXlen * imgYlen * sizeof(float));
	bandB = (float*)CPLMalloc(imgXlen * imgYlen * sizeof(float));
	bandI = (float*)CPLMalloc(imgXlen * imgYlen * sizeof(float));
	bandH = (float*)CPLMalloc(imgXlen * imgYlen * sizeof(float));
	bandS = (float*)CPLMalloc(imgXlen * imgYlen * sizeof(float));
			
	//获取图像像素
	poMulDS->GetRasterBand(1)->RasterIO(GF_Read,
		0, 0, imgXlen, imgYlen, bandR, imgXlen, imgYlen, GDT_Float32, 0, 0);
	poMulDS->GetRasterBand(2)->RasterIO(GF_Read,
		0, 0, imgXlen, imgYlen, bandG, imgXlen, imgYlen, GDT_Float32, 0, 0);
	poMulDS->GetRasterBand(3)->RasterIO(GF_Read,
		0, 0, imgXlen, imgYlen, bandB, imgXlen, imgYlen, GDT_Float32, 0, 0);
	poPanDS->GetRasterBand(1)->RasterIO(GF_Read,
		0, 0, imgXlen, imgYlen, bandI, imgXlen, imgYlen, GDT_Float32, 0, 0);
	
	cout << "...Processing..." << endl;

	//逐个像素处理
	for(int i = 0; i < imgXlen * imgYlen; i++) {
		//RGB转换为IHS
		bandH[i] = bandR[i] * tran1[1][0] + bandG[i] * tran1[1][1] + bandB[i] * tran1[1][2];
		bandS[i] = bandR[i] * tran1[2][0] + bandG[i] * tran1[2][1] + bandB[i] * tran1[2][2];
		//IHS逆转换为RGB
		bandR[i] = bandI[i] * tran2[0][0] + bandH[i] * tran2[0][1] + bandS[i] * tran2[0][2];
		bandG[i] = bandI[i] * tran2[1][0] + bandH[i] * tran2[1][1] + bandS[i] * tran2[1][2];
		bandB[i] = bandI[i] * tran2[2][0] + bandH[i] * tran2[2][1] + bandS[i] * tran2[2][2];
	}
	
	cout << "...Outputting..." << endl;
	
	//输出图像像素
	poFusDS->GetRasterBand(1)->RasterIO(GF_Write,
		0, 0, imgXlen, imgYlen, bandR, imgXlen, imgYlen, GDT_Float32, 0, 0);
	poFusDS->GetRasterBand(2)->RasterIO(GF_Write,
		0, 0, imgXlen, imgYlen, bandG, imgXlen, imgYlen, GDT_Float32, 0, 0);
	poFusDS->GetRasterBand(3)->RasterIO(GF_Write,
		0, 0, imgXlen, imgYlen, bandB, imgXlen, imgYlen, GDT_Float32, 0, 0);
	
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
	
	return 0;
}