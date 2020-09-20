
#ifdef __cplusplus 
extern "C" {
#endif


#include "esp_log.h"
#include "multi_button.h"


esp_err_t my_button_init(void);

esp_err_t my_button_attach(PressEvent event, BtnCallback cb);

#ifdef __cplusplus 
}
#endif