
/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "fonts.h"

#define FONT_GBK 1
#define FONT_FILE_DIR "/spiffs"

static uint8_t font_data[72] = {0};


//code 字符指针开始
//从字库中查找出字模
//code 字符串的开始地址,GBK码
//mat  数据存放地址 (size/8+((size%8)?1:0))*(size) bytes大小
static int get_char_mask(const char *code, uint8_t const **mask, uint16_t *len)
{
    uint8_t qh, ql;
    uint8_t i;
    uint32_t foffset;
    char file_path[64] = {0};
    uint16_t font_size = 16;
    *mask = font_data;
    uint8_t *mat = font_data;

    uint8_t csize = (font_size / 8 + ((font_size % 8) ? 1 : 0)) * (font_size); //得到字体一个字符对应点阵集所占的字节数
    qh = *code;
    ql = *(++code);
    printf("code=[%x,%x]\n", qh, ql);
#if FONT_GBK
    if (qh < 0x81 || ql < 0x40 || ql == 0xff || qh == 0xff) { //非 常用汉字
        for (i = 0; i < csize; i++) {
            *mat++ = 0x00;    //填充满格
        }
        printf("Uncommon words");
        return 0;            //结束访问
    }
    if (ql < 0x7f) {
        ql -= 0x40;    //注意!
    } else {
        ql -= 0x41;
    }
    qh -= 0x81;
    foffset = ((uint32_t)190 * qh + ql) * csize; //得到字库中的字节偏移量
#elif FONT_GB2312
    ql -= 0xa1;
    qh -= 0xa1;
    foffset = ((uint32_t)94 * qh + ql) * csize;
#endif

    printf("code=[%d,%d], csize=%d, foffset=%u\n", qh, ql, csize, foffset);


    switch (font_size) {
    // case 12:
    //     sprintf(file_path, "%s/%s", info->base_path, "16x16.DZK");
    //     break;
    case 16:
        sprintf(file_path, "%s/%s", FONT_FILE_DIR, "16x16.DZK");
        break;
    // case 24:
    //     sprintf(file_path, "%s/%s", info->base_path, "16x16.DZK");
    //     break;
    default:
        printf("ERROR: can't find font file");
        return -1;
        break;
    }

    FILE *fd;
    fd = fopen(file_path, "r");
    if (!fd) {
        printf("ERROR: Failed to read file : %s", file_path);
        return -2;
    }

    size_t chunksize = 0;
    if (0 != fseek(fd, foffset, SEEK_SET)) {
        printf("ERROR: file fseek failed");
        fclose(fd);
        return -3;
    }
    chunksize = fread(mat, 1, csize, fd);
    if (chunksize != csize) {
        printf("ERROR: file read failed");
        fclose(fd);
        return -4;
    }
    *len = chunksize;
    fclose(fd);

    return 0;
}


const sFONT Font16GBK = {
	.get_char_mask = get_char_mask,
    .table = NULL,
    .Width = 16, /* Width */
    .Height = 16, /* Height */
};

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
