

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <sys/stat.h>
#include <dirent.h>
#include "esp_spiffs.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "file_manage.h"

static const char *TAG = "file manage";

#define USE_SPIFFS

static fs_info_t g_fs_info = {0};

#ifdef USE_FATFS
static wl_handle_t g_wl_handle = WL_INVALID_HANDLE;
#endif

static void TraverseDir(char *direntName, int level, int indent)
{
    //定义一个目录流指针
    DIR *p_dir = NULL;

    //定义一个目录结构体指针
    struct dirent *p_dirent = NULL;

    //打开目录，返回一个目录流指针指向第一个目录项
    p_dir = opendir(direntName);

    if (p_dir == NULL)
    {
        printf("opendir error\n");
        return;
    }

    //循环读取每个目录项, 当返回NULL时读取完毕
    while ((p_dirent = readdir(p_dir)) != NULL)
    {
        //备份之前的目录名
        char *backupDirName = NULL;

        // if(p_dirent->d_name[0] == "." || p_dirent->d_name[0] == "..")
        if (p_dirent->d_name[0] == '.')
        {
            continue;
        }

        int i;

        for (i = 0; i < indent; i++)
        {
            // printf("|");
            printf("     ");
        }

        printf("|--- %s", p_dirent->d_name);

        //如果目录项仍是一个文件的话，打印信息
        if (p_dirent->d_type == DT_REG)
        {
            //当前目录长度
            int curDirentNameLen = strlen(direntName) + strlen(p_dirent->d_name) + 2;

            //准备下一级
            backupDirName = (char *)malloc(curDirentNameLen);
            struct stat *st = NULL;
            st = malloc(sizeof(struct stat));

            if (NULL == backupDirName || NULL == st)
            {
                goto _err;
            }

            memset(backupDirName, 0, curDirentNameLen);
            memcpy(backupDirName, direntName, strlen(direntName));

            strcat(backupDirName, "/");
            strcat(backupDirName, p_dirent->d_name);

            int statok = stat(backupDirName, st);

            if (0 == statok)
            {
                printf("[%dB]\n", (int)st->st_size);
            }
            else
            {
                printf("\n");
            }

            free(backupDirName);
            backupDirName = NULL;
        }
        else
        {
            printf("\n");
        }

        //如果目录项仍是一个目录的话，进入目录
        if (p_dirent->d_type == DT_DIR)
        {
            //当前目录长度
            int curDirentNameLen = strlen(direntName) + strlen(p_dirent->d_name) + 2;

            //准备下一级
            backupDirName = (char *)malloc(curDirentNameLen);

            if (NULL == backupDirName)
            {
                goto _err;
            }

            memset(backupDirName, 0, curDirentNameLen);
            memcpy(backupDirName, direntName, curDirentNameLen);

            strcat(backupDirName, "/");
            strcat(backupDirName, p_dirent->d_name);

            if (level > 0)
            {
                TraverseDir(backupDirName, level - 1, indent + 1); //递归调用
            }

            free(backupDirName);
            backupDirName = NULL;
        }
    }

_err:
    closedir(p_dir);
}

void PrintDirentStruct(char *direntName, int level)
{
    printf("Traverse directory %s\n", direntName);
    TraverseDir(direntName, level, 0);
    printf("\r\n");
}

static esp_err_t init_filesystem(void)
{
    esp_err_t ret;

#ifdef USE_SPIFFS
    g_fs_info.base_path = "/spiffs";
    g_fs_info.label = "storage";
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = g_fs_info.base_path,
        .partition_label = g_fs_info.label,
        .max_files = 5, // This decides the maximum number of files that can be created on the storage
        .format_if_mount_failed = true};

    ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(g_fs_info.label, &total, &used);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);

#elif defined USE_FATFS

    g_fs_info.base_path = "/fatfs";
    g_fs_info.label = "audio";

    esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 5,
        .format_if_mount_failed = true,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE};
    ESP_LOGI(TAG, "Initializing FATFS path=%s lable=%s", g_fs_info.base_path, g_fs_info.label);
    ret = esp_vfs_fat_spiflash_mount(g_fs_info.base_path, g_fs_info.label, &mount_config, &g_wl_handle);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    // size_t bytes_total, bytes_free;
    // esp_vfs_fat_usage(g_fs_info.base_path, &bytes_total, &bytes_free);

    // ESP_LOGI(TAG, "FATFS: %d kB total, %d kB free", bytes_total / 1024, bytes_free / 1024);

#endif

    return ESP_OK;
}

esp_err_t fm_init(void)
{
    esp_err_t ret;
    ret = init_filesystem();

    if (ESP_OK != ret)
    {
        return ret;
    }

    PrintDirentStruct(g_fs_info.base_path, 2);

    return ESP_OK;
}

esp_err_t fm_get_info(fs_info_t **info)
{
    if (strlen(g_fs_info.base_path))
    {
        *info = &g_fs_info;
        return ESP_OK;
    }
    ESP_LOGW(TAG, "filesystem not initilaze");
    return ESP_ERR_NOT_FOUND;
}


esp_err_t fm_file_table_create(char ***list_out, uint16_t *files_number)
{
    DIR *p_dir = NULL;
    struct dirent *p_dirent = NULL;
    
    p_dir = opendir(g_fs_info.base_path);

    if (p_dir == NULL)
    {
        ESP_LOGE(TAG, "opendir error");
        return ESP_FAIL;
    }

    (*files_number) = 0;
    while ((p_dirent = readdir(p_dir)) != NULL)
    {
        if (p_dirent->d_type == DT_REG)
        {
            (*files_number)++;
        }
    }
    ESP_LOGI(TAG, "find total files number: %d", (*files_number));

    rewinddir(p_dir);
    uint16_t index = 0;

    *list_out = calloc((*files_number), sizeof(char *));
    if (NULL == (*list_out))
    {
        goto _err;
    }
    for (size_t i = 0; i < (*files_number); i++)
    {
        (*list_out)[i] = malloc(CONFIG_SPIFFS_OBJ_NAME_LEN);
        if(NULL == (*list_out)[i]){
            ESP_LOGE(TAG, "malloc failed at %d", i);
            fm_file_table_free(list_out, (*files_number));
            goto _err;
        }
    }

    while ((p_dirent = readdir(p_dir)) != NULL)
    {
        ESP_LOGI(TAG, "find file %s", p_dirent->d_name);
        if (p_dirent->d_type == DT_REG)
        {
            strncpy((*list_out)[index], p_dirent->d_name, CONFIG_SPIFFS_OBJ_NAME_LEN-1);
            (*list_out)[index][CONFIG_SPIFFS_OBJ_NAME_LEN-1] = '\0';
            index++;
        }
    }
    
    closedir(p_dir);
    return ESP_OK;
_err:
    closedir(p_dir);

    return ESP_FAIL;
}

esp_err_t fm_file_table_free(char ***list, uint16_t files_number)
{
    for (size_t i = 0; i < files_number; i++)
    {
        free((*list)[i]);
    }
    free((*list));
    return ESP_OK;
}