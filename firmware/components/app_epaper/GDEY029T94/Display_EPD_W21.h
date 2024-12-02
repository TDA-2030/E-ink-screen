#ifndef _DISPLAY_EPD_W21_H_
#define _DISPLAY_EPD_W21_H_


#define EPD_WIDTH   128 
#define EPD_HEIGHT  296
#define EPD_ARRAY_SIZE  EPD_WIDTH*EPD_HEIGHT/8  


//Full screen refresh display
void EPD_HW_Init(epd_handle_t epd_handle); 
void EPD_HW_Init_180(epd_handle_t epd_handle);  
void EPD_WhiteScreen_ALL(epd_handle_t epd_handle, const unsigned char *datas);
void EPD_WhiteScreen_White(epd_handle_t epd_handle);
void EPD_WhiteScreen_Black(epd_handle_t epd_handle);
void EPD_DeepSleep(epd_handle_t epd_handle);
//Partial refresh display 
void EPD_SetRAMValue_BaseMap(epd_handle_t epd_handle, const unsigned char * datas);
void EPD_Dis_PartAll(epd_handle_t epd_handle, const unsigned char * datas);
void EPD_Dis_Part(epd_handle_t epd_handle, unsigned int x_start,unsigned int y_start,const unsigned char * datas,unsigned int PART_COLUMN,unsigned int PART_LINE);
void EPD_Dis_Part_Time(epd_handle_t epd_handle, unsigned int x_startA,unsigned int y_startA,const unsigned char * datasA,
                         unsigned int x_startB,unsigned int y_startB,const unsigned char * datasB,
                         unsigned int x_startC,unsigned int y_startC,const unsigned char * datasC,
                         unsigned int x_startD,unsigned int y_startD,const unsigned char * datasD,
                         unsigned int x_startE,unsigned int y_startE,const unsigned char * datasE,
                         unsigned int PART_COLUMN,unsigned int PART_LINE
                        );                         
//Fast refresh display
void EPD_HW_Init_Fast(epd_handle_t epd_handle);
void EPD_WhiteScreen_ALL_Fast(epd_handle_t epd_handle, const unsigned char *datas);   
void EPD_HW_Init_Fast2(epd_handle_t epd_handle);
void EPD_WhiteScreen_ALL_Fast2(epd_handle_t epd_handle, const unsigned char *datas);

                         
#endif
