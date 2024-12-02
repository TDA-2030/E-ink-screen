#include "./Display_EPD_W21_spi.h"
#include "./Display_EPD_W21.h"
#include "../app_epaper.h"


#define delay(ms) vTaskDelay(pdMS_TO_TICKS(ms))

////////////////////////////////////E-paper demo//////////////////////////////////////////////////////////
//Busy function
void Epaper_READBUSY(epd_handle_t epd_handle)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    uint32_t timeout = 1000;
    while(timeout--)
    {   //=1 BUSY
        delay(5);
        if(isEPD_W21_BUSY==0) return;
    }
    ESP_LOGW(TAG, "EPD_W21_BUSY timeout");
}
//Full screen refresh initialization
void EPD_HW_Init(epd_handle_t epd_handle)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    EPD_W21_RST_0;  // Module reset
    delay(10);//At least 10ms delay
    EPD_W21_RST_1;
    delay(10); //At least 10ms delay

    Epaper_READBUSY(epd_handle);
    EPD_W21_WriteCMD(0x12);  //SWRESET
    Epaper_READBUSY(epd_handle);

    EPD_W21_WriteCMD(0x01); //Driver output control
    EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);
    EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
    EPD_W21_WriteDATA(0x00);

    EPD_W21_WriteCMD(0x11); //data entry mode
    EPD_W21_WriteDATA(0x01);

    EPD_W21_WriteCMD(0x44); //set Ram-X address start/end position
    EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteDATA(EPD_WIDTH/8-1);

    EPD_W21_WriteCMD(0x45); //set Ram-Y address start/end position
    EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);
    EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
    EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteDATA(0x00);

    EPD_W21_WriteCMD(0x3C); //BorderWavefrom
    EPD_W21_WriteDATA(0x05);

    EPD_W21_WriteCMD(0x21); //  Display update control
    EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteDATA(0x80);

    EPD_W21_WriteCMD(0x18); //Read built-in temperature sensor
    EPD_W21_WriteDATA(0x80);

    EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
    EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X199;
    EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);
    EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
    Epaper_READBUSY(epd_handle);

}
//Fast refresh 1 initialization
void EPD_HW_Init_Fast(epd_handle_t epd_handle)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    EPD_W21_RST_0;  // Module reset
    delay(10);//At least 10ms delay
    EPD_W21_RST_1;
    delay(10); //At least 10ms delay

    EPD_W21_WriteCMD(0x12);  //SWRESET
    Epaper_READBUSY(epd_handle);

    // EPD_W21_WriteCMD(0x11); //data entry mode
    // EPD_W21_WriteDATA(0x3);
    // Epaper_READBUSY(epd_handle);

    EPD_W21_WriteCMD(0x18); //Read built-in temperature sensor
    EPD_W21_WriteDATA(0x80);

    EPD_W21_WriteCMD(0x22); // Load temperature value
    EPD_W21_WriteDATA(0xB1);
    EPD_W21_WriteCMD(0x20);
    Epaper_READBUSY(epd_handle);

    EPD_W21_WriteCMD(0x1A); // Write to temperature register
    EPD_W21_WriteDATA(0x64);
    EPD_W21_WriteDATA(0x00);

    EPD_W21_WriteCMD(0x22); // Load temperature value
    EPD_W21_WriteDATA(0x91);
    EPD_W21_WriteCMD(0x20);
    Epaper_READBUSY(epd_handle);
}


//////////////////////////////Display Update Function///////////////////////////////////////////////////////
//Full screen refresh update function
void EPD_Update(epd_handle_t epd_handle)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    EPD_W21_WriteCMD(0x22); //Display Update Control
    EPD_W21_WriteDATA(0xF7);
    EPD_W21_WriteCMD(0x20); //Activate Display Update Sequence
    Epaper_READBUSY(epd_handle);

}
//Fast refresh 1 update function
void EPD_Update_Fast(epd_handle_t epd_handle)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    EPD_W21_WriteCMD(0x22); //Display Update Control
    EPD_W21_WriteDATA(0xC7);
    EPD_W21_WriteCMD(0x20); //Activate Display Update Sequence
    Epaper_READBUSY(epd_handle);

}
//Partial refresh update function
void EPD_Part_Update(epd_handle_t epd_handle)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    EPD_W21_WriteCMD(0x22); //Display Update Control
    EPD_W21_WriteDATA(0xFF);
    EPD_W21_WriteCMD(0x20); //Activate Display Update Sequence
    Epaper_READBUSY(epd_handle);
}
//////////////////////////////Display Data Transfer Function////////////////////////////////////////////
//Full screen refresh display function
void EPD_WhiteScreen_ALL(epd_handle_t epd_handle, const unsigned char *datas)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    unsigned int i;
    EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)
    for(i=0; i<EPD_ARRAY_SIZE; i++)
    {
        EPD_W21_WriteDATA(datas[i]);
    }
    EPD_Update(epd_handle);
}
//Fast refresh display function
void EPD_WhiteScreen_ALL_Fast(epd_handle_t epd_handle, const unsigned char *datas)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    unsigned int i;
    EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)
    for(i=0; i<EPD_ARRAY_SIZE; i++)
    {
        EPD_W21_WriteDATA(datas[i]);
    }

    EPD_Update_Fast(epd_handle);
}

//Clear screen display
void EPD_WhiteScreen_White(epd_handle_t epd_handle)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    unsigned int i;
    EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)
    for(i=0; i<EPD_ARRAY_SIZE; i++)
    {
        EPD_W21_WriteDATA(0xff);
    }
    EPD_Update(epd_handle);
}
//Display all black
void EPD_WhiteScreen_Black(epd_handle_t epd_handle)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    unsigned int i;
    EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)
    for(i=0; i<EPD_ARRAY_SIZE; i++)
    {
        EPD_W21_WriteDATA(0x00);
    }
    EPD_Update(epd_handle);
}
//Partial refresh of background display, this function is necessary, please do not delete it!!!
void EPD_SetRAMValue_BaseMap(epd_handle_t epd_handle, const unsigned char * datas)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    unsigned int i;
    EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
    for(i=0; i<EPD_ARRAY_SIZE; i++)
    {
        EPD_W21_WriteDATA(datas[i]);
    }
    EPD_W21_WriteCMD(0x26);   //Write Black and White image to RAM
    for(i=0; i<EPD_ARRAY_SIZE; i++)
    {
        EPD_W21_WriteDATA(datas[i]);
    }
    EPD_Update(epd_handle);

}
//Partial refresh display
void EPD_Dis_Part(epd_handle_t epd_handle, unsigned int x_start,unsigned int y_start,const unsigned char * datas,unsigned int PART_COLUMN,unsigned int PART_LINE)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    unsigned int i;
    unsigned int x_end,y_end;

    x_start=x_start/8; //x address start
    x_end=x_start+PART_LINE/8-1; //x address end
    y_start=y_start; //Y address start
    y_end=y_start+PART_COLUMN-1; //Y address end

    EPD_W21_RST_0;  // Module reset
    delay(10);//At least 10ms delay
    EPD_W21_RST_1;
    delay(10); //At least 10ms delay
    EPD_W21_WriteCMD(0x3C); //BorderWavefrom,
    EPD_W21_WriteDATA(0x80);

    EPD_W21_WriteCMD(0x44);       // set RAM x address start/end
    EPD_W21_WriteDATA(x_start);  //x address start
    EPD_W21_WriteDATA(x_end);   //y address end
    EPD_W21_WriteCMD(0x45);    // set RAM y address start/end
    EPD_W21_WriteDATA(y_start%256);  //y address start2
    EPD_W21_WriteDATA(y_start/256); //y address start1
    EPD_W21_WriteDATA(y_end%256);  //y address end2
    EPD_W21_WriteDATA(y_end/256); //y address end1

    EPD_W21_WriteCMD(0x4E);        // set RAM x address count to 0;
    EPD_W21_WriteDATA(x_start);   //x start address
    EPD_W21_WriteCMD(0x4F);      // set RAM y address count to 0X127;
    EPD_W21_WriteDATA(y_start%256);//y address start2
    EPD_W21_WriteDATA(y_start/256);//y address start1


    EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
    for(i=0; i<PART_COLUMN*PART_LINE/8; i++)
    {
        EPD_W21_WriteDATA(datas[i]);
    }
    EPD_Part_Update(epd_handle);

}
//Full screen partial refresh display
void EPD_Dis_PartAll(epd_handle_t epd_handle, const unsigned char * datas)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    unsigned int i;
    unsigned int PART_COLUMN, PART_LINE;
    PART_COLUMN=EPD_HEIGHT,PART_LINE=EPD_WIDTH;

    EPD_W21_RST_0;  // Module reset
    delay(10); //At least 10ms delay
    EPD_W21_RST_1;
    delay(10); //At least 10ms delay
    EPD_W21_WriteCMD(0x3C); //BorderWavefrom,
    EPD_W21_WriteDATA(0x80);


    EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
    for(i=0; i<PART_COLUMN*PART_LINE/8; i++)
    {
        EPD_W21_WriteDATA(datas[i]);
    }
    EPD_Part_Update(epd_handle);

}
//Deep sleep function
void EPD_DeepSleep(epd_handle_t epd_handle)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    EPD_W21_WriteCMD(0x10); //Enter deep sleep
    EPD_W21_WriteDATA(0x01);
    delay(100);
}

//Partial refresh write address and data
void EPD_Dis_Part_RAM(epd_handle_t epd_handle, unsigned int x_start,unsigned int y_start,const unsigned char * datas,unsigned int PART_COLUMN,unsigned int PART_LINE)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    unsigned int i;
    unsigned int x_end,y_end;

    x_start=x_start/8; //x address start
    x_end=x_start+PART_LINE/8-1; //x address end

    y_start=y_start-1; //Y address start
    y_end=y_start+PART_COLUMN-1; //Y address end

    EPD_W21_RST_0;  // Module reset
    delay(10);//At least 10ms delay
    EPD_W21_RST_1;
    delay(10); //At least 10ms delay
    EPD_W21_WriteCMD(0x3C); //BorderWavefrom,
    EPD_W21_WriteDATA(0x80);

    EPD_W21_WriteCMD(0x44);       // set RAM x address start/end
    EPD_W21_WriteDATA(x_start);  //x address start
    EPD_W21_WriteDATA(x_end);   //y address end
    EPD_W21_WriteCMD(0x45);     // set RAM y address start/end
    EPD_W21_WriteDATA(y_start%256);  //y address start2
    EPD_W21_WriteDATA(y_start/256); //y address start1
    EPD_W21_WriteDATA(y_end%256);  //y address end2
    EPD_W21_WriteDATA(y_end/256); //y address end1

    EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
    EPD_W21_WriteDATA(x_start);   //x start address
    EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X127;
    EPD_W21_WriteDATA(y_start%256); //y address start2
    EPD_W21_WriteDATA(y_start/256); //y address start1

    EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
    for(i=0; i<PART_COLUMN*PART_LINE/8; i++)
    {
        EPD_W21_WriteDATA(datas[i]);
    }
}
//Clock display
void EPD_Dis_Part_Time(epd_handle_t epd_handle, unsigned int x_startA,unsigned int y_startA,const unsigned char * datasA,
                       unsigned int x_startB,unsigned int y_startB,const unsigned char * datasB,
                       unsigned int x_startC,unsigned int y_startC,const unsigned char * datasC,
                       unsigned int x_startD,unsigned int y_startD,const unsigned char * datasD,
                       unsigned int x_startE,unsigned int y_startE,const unsigned char * datasE,
                       unsigned int PART_COLUMN,unsigned int PART_LINE
                      )
{
    EPD_Dis_Part_RAM(epd_handle, x_startA,y_startA,datasA,PART_COLUMN,PART_LINE);
    EPD_Dis_Part_RAM(epd_handle, x_startB,y_startB,datasB,PART_COLUMN,PART_LINE);
    EPD_Dis_Part_RAM(epd_handle, x_startC,y_startC,datasC,PART_COLUMN,PART_LINE);
    EPD_Dis_Part_RAM(epd_handle, x_startD,y_startD,datasD,PART_COLUMN,PART_LINE);
    EPD_Dis_Part_RAM(epd_handle, x_startE,y_startE,datasE,PART_COLUMN,PART_LINE);
    EPD_Part_Update(epd_handle);
}




////////////////////////////////Other newly added functions////////////////////////////////////////////
//Display rotation 180 degrees initialization
void EPD_HW_Init_180(epd_handle_t epd_handle)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    EPD_W21_RST_0;  // Module reset
    delay(10); //At least 10ms delay
    EPD_W21_RST_1;
    delay(10); //At least 10ms delay

    Epaper_READBUSY(epd_handle);
    EPD_W21_WriteCMD(0x12);  //SWRESET
    Epaper_READBUSY(epd_handle);

    EPD_W21_WriteCMD(0x3C); //BorderWavefrom
    EPD_W21_WriteDATA(0x05);

    EPD_W21_WriteCMD(0x01); //Driver output control
    EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);
    EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
    EPD_W21_WriteDATA(0x00);

    EPD_W21_WriteCMD(0x11); //data entry mode
    EPD_W21_WriteDATA(0x02);

    EPD_W21_WriteCMD(0x44); //set Ram-X address start/end position
    EPD_W21_WriteDATA(EPD_WIDTH/8-1);
    EPD_W21_WriteDATA(0x00);

    EPD_W21_WriteCMD(0x45); //set Ram-Y address start/end position
    EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);
    EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);

    EPD_W21_WriteCMD(0x21); //  Display update control
    EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteDATA(0x00);

    EPD_W21_WriteCMD(0x18); //Read built-in temperature sensor
    EPD_W21_WriteDATA(0x80);

    EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
    EPD_W21_WriteDATA(EPD_WIDTH/8-1);
    EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X199;
    EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteDATA(0x00);
    Epaper_READBUSY(epd_handle);
}

//Fast refresh 2 initialization
void EPD_HW_Init_Fast2(epd_handle_t epd_handle)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    EPD_W21_RST_0;  // Module reset
    delay(10);//At least 10ms delay
    EPD_W21_RST_1;
    delay(10); //At least 10ms delay

    EPD_W21_WriteCMD(0x12);  //SWRESET
    Epaper_READBUSY(epd_handle);

    EPD_W21_WriteCMD(0x18); //Read built-in temperature sensor
    EPD_W21_WriteDATA(0x80);

    EPD_W21_WriteCMD(0x22); // Load temperature value
    EPD_W21_WriteDATA(0xB1);
    EPD_W21_WriteCMD(0x20);
    Epaper_READBUSY(epd_handle);

    EPD_W21_WriteCMD(0x1A); // Write to temperature register
    EPD_W21_WriteDATA(0x5A);
    EPD_W21_WriteDATA(0x00);

    EPD_W21_WriteCMD(0x22); // Load temperature value
    EPD_W21_WriteDATA(0x91);
    EPD_W21_WriteCMD(0x20);
    Epaper_READBUSY(epd_handle);
}
//Fast 2 display
void EPD_WhiteScreen_ALL_Fast2(epd_handle_t epd_handle, const unsigned char *datas)
{
    epaper_panel_t *panel = (epaper_panel_t*)epd_handle;
    unsigned int i;
    EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)
    for(i=0; i<EPD_ARRAY_SIZE; i++)
    {
        EPD_W21_WriteDATA(*datas);
        datas++;
    }
    EPD_W21_WriteCMD(0x26);   //write RAM for black(0)/white (1)
    for(i=0; i<EPD_ARRAY_SIZE; i++)
    {
        EPD_W21_WriteDATA(0x00);
    }
    EPD_Update_Fast(epd_handle);
}
/***********************************************************
            end file
***********************************************************/
