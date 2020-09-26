#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "file_manage.h"
#include "epd2in13.h"
#include "epdpaint.h"
#include "cc936.h"

static const char *TAG = "text show";


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
static char* Utf8ToUnicode(char *pInput, char *pOutput)
{
    int outputSize = 0; //记录转换后的Unicode字符串的字节数
    char *p = pOutput;
 
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
            ESP_LOGE(TAG, "utf-8 encounter illegal character");
            return ++pInput;
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
            ESP_LOGE(TAG, "utf-8 encounter illegal character");
            return ++pInput;
        }
        *pOutput = (middle << 6) + (low & 0x3F);//取出middle的低两位与low的低6位，组合成unicode字符的低8位
        pOutput++;
        *pOutput = (high << 4) + ((middle >> 2) & 0x0F); //取出high的低四位与middle的中间四位，组合成unicode字符的高8位
    }
    else //对于其他字节数的UTF8字符不进行处理
    {
        ESP_LOGE(TAG, "Unsupport utf-8 character [%x]", *pInput);
        return ++pInput;
    }
    outputSize += 2;

    // 字节顺序翻转
    for (size_t i = 0; i < outputSize; i+=2)
    {  
        char t = p[i];
        p[i] = p[i+1];
        p[i+1] = t;
    }
    
    return ++pInput;
}


//code 字符指针开始
//从字库中查找出字模
//code 字符串的开始地址,GBK码
//mat  数据存放地址 (size/8+((size%8)?1:0))*(size) bytes大小
//size:字体大小
static esp_err_t Get_HzMat(const char *code, uint8_t *mat, uint16_t *len, uint8_t font_size)
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
esp_err_t app_show_gbk_char(uint16_t x, uint16_t y, const char *gbk, uint8_t size, uint8_t mode)
{
    esp_err_t ret;
    uint8_t temp, t, t1;
    uint16_t x0 = x, font_len;
    uint8_t dzk[72];
    uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size); //得到字体一个字符对应点阵集所占的字节数
    if (size != 12 && size != 16 && size != 24)
        return ESP_FAIL;                 //不支持的size

   
    ret = Get_HzMat(gbk, dzk, &font_len, size); //得到相应大小的点阵数据
    if (ESP_OK != ret){
        return ESP_FAIL;
    }

    
    for (t = 0; t < csize; t++)
    {
        temp = dzk[t]; //得到点阵数据
        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80){
                Paint_DrawPixel(x, y, 0);
            }
            else if (mode == 0){
                Paint_DrawPixel(x, y, 1);
            }
            temp <<= 1;
            x++;
            if ((x - x0) == size)
            {
                x = x0;
                y++;
                break;
            }
        }
    }
    
    return ESP_OK;
}

//在指定位置开始显示一个字符串
//支持自动换行
//(x,y):起始坐标
//width,height:区域
//str  :字符串
//size :字体大小
//mode:0,非叠加方式;1,叠加方式
esp_err_t app_show_text_str(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char *string, uint8_t size, uint8_t mode)
{
    esp_err_t ret;
    char *str = (char*)string;

    char gbk[2];
    char unicode[2];
    
    uint16_t x0=x;
    uint16_t y0=y;
    uint8_t bHz=0;  //字符或者中文

    while(*str!=0)//数据未结束
    {
        str = Utf8ToUnicode(str, unicode);
        font_unicode2gbk(unicode, gbk);

        if(gbk[0] > 0x80){//中文
            bHz=1;
            if(x>(x0+width-size)) {//换行
                y+=size;
                x=x0;
            }
            if(y>(y0+height-size))break;//越界返回
            app_show_gbk_char(x, y, gbk, size, mode);
            x+=size;//下一个汉字偏移
        } else { //字符
            if(x>(x0+width-size/2))//换行
            {
                y+=size;
                x=x0;
            }
            if(y>(y0+height-size))break;//越界返回      
            if(gbk[1]==13)//换行符号
            {         
                y+=size;
                x=x0;
            }  
            else {
                
                Paint_DrawCharAt((int)x, (int)y, gbk[1], &Font16, COLORED);printf("%c", gbk[1]);
            }
            x+=size/2; //字符,为全字的一半 
        }
    }
    return ESP_OK;
}

esp_err_t app_show_text_init(void)
{

    return ESP_OK;
}