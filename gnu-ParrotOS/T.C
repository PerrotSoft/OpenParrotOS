#include <stdint.h>
#include "src/kernel/include/drivers/video_driver.h"
void main(){
    draw_line(1,1,10,100,White);
    while(1);
}