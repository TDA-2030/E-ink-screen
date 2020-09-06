#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "pretty_effect.h"
#include "file_manage.h"
#include "file_server.h"
#include "decode_image.h"

#include "epd2in13.h"
#include "epdpaint.h"
#include "cc936.h"

static const char *TAG = "text show";


static Paint *g_paint;
static Epd *g_epd;

#define FONT_GBK 1

/**
 * 
 * Reference from https://blog.csdn.net/bladeandmaster88/article/details/54837338
 */
static int UnicodeToUtf8(char* pInput, char *pOutput)  
{  
    int len = 0; //记录转换后的Utf8字符串的字节数
    while (*pInput)
    {
        //处理一个unicode字符
        char low = *pInput;//取出unicode字符的低8位
        pInput++;
        char high = *pInput;//取出unicode字符的高8位
        unsigned  wchar = (high<<8)+low;//高8位和低8位组成一个unicode字符,加法运算级别高
    
        if (wchar <= 0x7F ) //英文字符
        {   
            pOutput[len] = (char)wchar;  //取wchar的低8位
            len++;
        }  
        else if (wchar >=0x80 && wchar <= 0x7FF)  //可以转换成双字节pOutput字符
        {  
            pOutput[len] = 0xc0 |((wchar >> 6)&0x1f);  //取出unicode编码低6位后的5位，填充到110yyyyy 10zzzzzz 的yyyyy中
            len++;
            pOutput[len] = 0x80 | (wchar & 0x3f);  //取出unicode编码的低6位，填充到110yyyyy 10zzzzzz 的zzzzzz中
            len++;
        }  
        else if (wchar >=0x800 && wchar < 0xFFFF)  //可以转换成3个字节的pOutput字符
        {  
            pOutput[len] = 0xe0 | ((wchar >> 12)&0x0f);  //高四位填入1110xxxx 10yyyyyy 10zzzzzz中的xxxx
            len++;
            pOutput[len] = 0x80 | ((wchar >> 6) & 0x3f);  //中间6位填入1110xxxx 10yyyyyy 10zzzzzz中的yyyyyy
            len++;
            pOutput[len] = 0x80 | (wchar & 0x3f);  //低6位填入1110xxxx 10yyyyyy 10zzzzzz中的zzzzzz
            len++;
        }  
    
        else //对于其他字节数的unicode字符不进行处理
        {
            return -1;
        }
        pInput ++;//处理下一个unicode字符
    }
    //utf8字符串后面，有个\0
    pOutput [len]= 0;
    return len;  
}
/*************************************************************************************************
* 将UTF8编码转换成Unicode（UCS-2LE）编码  高地址存低位字节
* 参数：
*    char* pInput     输入字符串
*    char*pOutput   输出字符串
* 返回值：转换后的Unicode字符串的字节数，如果出错则返回-1
**************************************************************************************************/
//utf8转unicode
static int Utf8ToUnicode(char* pInput, char* pOutput)
{
	int outputSize = 0; //记录转换后的Unicode字符串的字节数
    char *p = pOutput;
 
	while (*pInput)
	{
		if (*pInput > 0x00 && *pInput <= 0x7F) //处理单字节UTF8字符（英文字母、数字）
		{
			*pOutput = *pInput;
			 pOutput++;
			*pOutput = 0; //小端法表示，在高地址填补0
		}
		else if (((*pInput) & 0xE0) == 0xC0) //处理双字节UTF8字符
		{
			char high = *pInput;
			pInput++;
			char low = *pInput;
			if ((low & 0xC0) != 0x80)  //检查是否为合法的UTF8字符表示
			{
				return -1; //如果不是则报错
			}
 
			*pOutput = (high << 6) + (low & 0x3F);
			pOutput++;
			*pOutput = (high >> 2) & 0x07;
		}
		else if (((*pInput) & 0xF0) == 0xE0) //处理三字节UTF8字符
		{
			char high = *pInput;
			pInput++;
			char middle = *pInput;
			pInput++;
			char low = *pInput;
			if (((middle & 0xC0) != 0x80) || ((low & 0xC0) != 0x80))
			{
				return -1;
			}
			*pOutput = (middle << 6) + (low & 0x3F);//取出middle的低两位与low的低6位，组合成unicode字符的低8位
			pOutput++;
			*pOutput = (high << 4) + ((middle >> 2) & 0x0F); //取出high的低四位与middle的中间四位，组合成unicode字符的高8位
		}
		else //对于其他字节数的UTF8字符不进行处理
		{
			return -1;
		}
		pInput ++;//处理下一个utf8字符
		pOutput ++;
		outputSize += 2;
	}

	//unicode字符串后面，有两个\0
	*pOutput = 0;
	 pOutput++;
	*pOutput = 0;

    // 字节顺序翻转
    printf("fanzhuan\n");
    for (size_t i = 0; i < outputSize; i+=2)
    {  
        char t = p[i];
        p[i] = p[i+1];
        p[i+1] = t;
    }
    
	return outputSize;
}


//code 字符指针开始
//从字库中查找出字模
//code 字符串的开始地址,GBK码
//mat  数据存放地址 (size/8+((size%8)?1:0))*(size) bytes大小
//size:字体大小
static esp_err_t Get_HzMat(char *code, uint8_t *mat, uint16_t *len, uint8_t font_size)
{
    uint8_t qh, ql;
    uint8_t i;
    uint32_t foffset;
    char file_path[64] = {0};

    uint8_t csize = (font_size / 8 + ((font_size % 8) ? 1 : 0)) * (font_size); //得到字体一个字符对应点阵集所占的字节数
    qh = *code;
    ql = *(++code);
    ESP_LOGI(TAG, "code=[%x,%x]", qh, ql);
#if FONT_GBK
    if (qh < 0x81 || ql < 0x40 || ql == 0xff || qh == 0xff) //非 常用汉字
    {
        for (i = 0; i < csize; i++)
            *mat++ = 0x00; //填充满格
        ESP_LOGW(TAG, "Uncommon words");
        return ESP_OK;            //结束访问
    }
    if (ql < 0x7f)
        ql -= 0x40; //注意!
    else
        ql -= 0x41;
    qh -= 0x81;
    foffset = ((uint32_t)190 * qh + ql) * csize; //得到字库中的字节偏移量
#elif FONT_GB2312
    ql -= 0xa1;
    qh -= 0xa1;
    foffset = ((uint32_t)94*qh + ql)*csize;
#endif

    ESP_LOGI(TAG, "code=[%d,%d], csize=%d, foffset=%d", qh, ql, csize, foffset);

    fs_info_t *info;
    fm_get_info(&info);

    switch (font_size)
    {
    // case 12:
    //     sprintf(file_path, "%s/%s", info->base_path, "16x16.DZK");
    //     break;
    case 16:
        sprintf(file_path, "%s/%s", info->base_path, "16x16.DZK");
        break;
    // case 24:
    //     sprintf(file_path, "%s/%s", info->base_path, "16x16.DZK");
    //     break;
    default:
        ESP_LOGE(TAG, "can't find font file");
        return ESP_FAIL;
        break;
    }

    FILE *fd;
    fd = fopen(file_path, "r");
    if (!fd)
    {
        ESP_LOGE(TAG, "Failed to read file : %s", file_path);
        return ESP_FAIL;
    }

    size_t chunksize = 0;
    if(0 != fseek(fd, foffset, SEEK_SET)){
        ESP_LOGE(TAG, "file fseek failed");
        fclose(fd);
        return ESP_FAIL;
    }
    chunksize = fread(mat, 1, csize, fd);
    if(chunksize != csize){
        ESP_LOGE(TAG, "file read failed");
        fclose(fd);
        return ESP_FAIL;
    }
    *len = chunksize;
    fclose(fd);
    
    return ESP_OK;
}

//显示一个指定大小的汉字
//x,y :汉字的坐标
//font:汉字GBK码
//size:字体大小
//mode:0,正常显示,1,叠加显示
extern "C" esp_err_t app_show_text(uint16_t x, uint16_t y, char *font, uint8_t size, uint8_t mode)
{
    esp_err_t ret;
    uint8_t temp, t, t1;
    uint16_t y0 = y, font_len;
    uint8_t dzk[72];
    uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size); //得到字体一个字符对应点阵集所占的字节数
    if (size != 12 && size != 16 && size != 24)
        return ESP_FAIL;                 //不支持的size

    char unicode[4], gbk[2];
    ret = Utf8ToUnicode(font, unicode);
    if (0 > ret){
        ESP_LOGE(TAG, "Utf8ToUnicode failed");
    }
    font_unicode2gbk(unicode, gbk);
    ret = Get_HzMat(gbk, dzk, &font_len, size); //得到相应大小的点阵数据
    if (ESP_OK != ret){
        return ESP_FAIL;
    }

    g_paint->SetRotate(ROTATE_90);
    for (t = 0; t < csize; t++)
    {
        temp = dzk[t]; //得到点阵数据
        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80){
                g_paint->DrawPixel(x, y, 1);
            }
            else if (mode == 0){
                g_paint->DrawPixel(x, y, 0);
            }
            temp <<= 1;
            y++;
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                break;
            }
        }
    }
    g_epd->SetFrameMemory(g_paint->GetImage(), 0, 0, g_paint->GetWidth(), g_paint->GetHeight());
    g_epd->DisplayFrame();
    return ESP_OK;
}


extern "C" esp_err_t app_show_text_init(Paint *paint, Epd *epd)
{
    g_paint = paint;
    g_epd = epd;
    return ESP_OK;
}