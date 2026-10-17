/* ======================== 【为什么 main.c 这么短】 ========================
 * main() 里每多一行，就多一个「上电顺序」的坑。所以配置全部推进 app_init()，
 * main 只留「配时钟 -> 开调试串口 -> 进主循环」，顺序一眼可见。
 *
 * 先 clock_init 再 debug_init：串口波特率是用 system_clock 算出来的，
 * 时钟没定就开串口，波特率必然是错的。
 *
 * 主循环里没有任何延时：所有「等一会儿」都交给第 6 步的 1ms 节拍。
 * ======================================================================== */
/********************************************************************************
* File Name          : main.c
* Description        : CH32V307VCT6 + SeekFree CH32V307 opensource library
*
*   本文件只做三件事：配置时钟 -> 初始化调试串口 -> 转交 app 层
*   业务代码一律不写在这里（答辩要点）
********************************************************************************/
#include "zf_common_headfile.h"
#include "app.h"

int main(void)
{
    clock_init(SYSTEM_CLOCK_120M);      /* 120 MHz —— 擦写 FLASH 必须 <=120MHz（见步骤 8），所以从一开始就定在 120M */
    debug_init();                       /* 调试串口 UART3: PB10(TX) / PB11(RX) / 115200 */

    printf("\r\n\r\n===== CH32V307 board boot =====\r\n");
    printf("[BOOT] system_clock = %d MHz\r\n", system_clock / 1000000);

    app_init();                         /* 所有模块的初始化都在这里面 */

    while(1)
    {
        app_task();                     /* 主循环只调度，不做长阻塞 */
    }
}
