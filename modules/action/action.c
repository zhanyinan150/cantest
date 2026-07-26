/**
  ******************************************************************************
  * @file    action.c
  * @brief   K230 通讯测试 (纯收发握手, 无电机动作)
  ******************************************************************************
  * action_1 任务: 上电后测试 STM32-K230 通讯握手, 通过 UART1(printf)
  * 上报每一步收到的数据到串口助手。
  *   Phase 1: 等 K230 豆子数据 -> ACK
  *   Phase 2: 发 LOOK_NUMBER -> 等正面数字 -> ACK
  *   Phase 3: 等 5 数字 -> ACK
  ******************************************************************************
  */

#include "action.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "motor.h"       /* Motor_XYZ */
#include "K230.h"        /* k230_write, k230_read */
#include "usart.h"       /* huart2 */
#include "stdio.h"

/* ---- 任务参数 ---- */
#define ACTION_TASK_STACK_SIZE   1024
#define ACTION_TASK_PRIORITY     osPriorityNormal
#define ACTION_STARTUP_DELAY     1000

static void action_1(void *argument);


//备注一下后面的左边指有电池那边，右边指没电池那边



/**
  * @brief  初始化动作序列: 创建 action_1 任务
  */
// void Action_Init(void)
// {
//     const osThreadAttr_t attr = {
//         .name = "ActionTask",
//         .stack_size = ACTION_TASK_STACK_SIZE * 4,
//         .priority = (osPriority_t)ACTION_TASK_PRIORITY,
//     };
//     osThreadNew(action_1, NULL, &attr);
// }

static void action_1(void *argument)
{
    (void)argument;
    osDelay(ACTION_STARTUP_DELAY);

    printf("=== K230 Comm Test ===\n");

    /* Phase 1: 等 K230 豆子数据 (K230 上电自主识别) */
    printf("[P1] Waiting for bean data...\n");
    while (!bean_flag)
        osDelay(10);
    bean_flag = 0;
    printf("[P1] Bean: %02X %02X %02X\n", bean_color[0], bean_color[1], bean_color[2]);
    k230_write(K230_CMD_CLOSE);
    printf("[P1] ACK sent\n");

    /* Phase 2: 触发正面数字识别 */
    printf("[P2] Trigger front number...\n");
    k230_write(K230_CMD_LOOK_NUMBER);
    while (!front_number_flag)
        osDelay(10);
    front_number_flag = 0;
    printf("[P2] Front: %02X %02X %02X\n", front_number[0], front_number[1], front_number[2]);
    k230_write(K230_CMD_CLOSE);
    printf("[P2] ACK sent\n");

    /* Phase 3: 等 5 数字 (4 识别 + 1 推理) */
    printf("[P3] Waiting for 5 numbers...\n");
    while (!full_number_flag)
        osDelay(10);
    full_number_flag = 0;
    printf("[P3] Full: %02X %02X %02X %02X %02X\n",
           number_position[0], number_position[1], number_position[2],
           number_position[3], number_position[4]);
    k230_write(K230_CMD_CLOSE);
    printf("[P3] ACK sent\n");

    k230_write(1);//给k230发送看豆命令
    osDelay(1000);//等待k230返回结果
    k230_read(&huart2);//解析k230返回结果
    osDelay(500);//等待k230解析完成
    k230_write(6);//给k230发送关闭摄像头命令


    action_douzi_first();   /* 抓豆子 ，动作截止到抓完豆子已经完成升降结构移到右边*/

    //数字识别我放在action_xiangzi_first里面了
    action_xiangzi_first(); /* 放箱子 */

    for (;;)
        osDelay(1000);
}



    }
}









/* ===== 分界线前: 抓豆子 ===== */
/**
  * @brief  抓豆子动作序列
  *         起始点 -> 抓左边豆子 -> 抓中间豆子 -> 准备去放箱子
  */
static void action_douzi_first(void)
{
    /* 起始点到抓左边豆子 */
    (void)Motor_XYZ(1, 300, 30, 44.0f,  /* X: dir=1(右), 300rpm, acc=30, 44cm */
                    1, 100, 20, 185.0f,    /* Y: dir=1(前), 100rpm, acc=20, 185cm */
                    0, 35.0f);           /* Z: dir=0(上), 35cm */
    osDelay(6000);

    /* 抓中间豆子 */
    (void)Motor_XYZ(0, 300, 20, 20.5f,  /* X: dir=0(左), 300rpm, acc=20, 20.5cm */
                    1, 100, 20, 0,      /* Y: 不动 */
                    0, 0.0f);            /* Z: 不动 */
    osDelay(4000);

    /* 准备去放箱子 */
    (void)Motor_XYZ(0, 300, 20, 60.0f,  /* X: dir=0(左), 300rpm, acc=20, 60cm */
                    1, 100, 20, 0,      /* Y: 不动 */
                    0, 0.0f);            /* Z: 不动 */
    osDelay(9000);
}

/* ===== 分界线后: 放箱子(含障碍物避让) ===== */
/**
  * @brief  放箱子动作序列
  *         障碍物动作1 -> 障碍物动作2 -> 箱子障碍物到箱子
  */
static void action_xiangzi_first(void)
{
    /* 豆子到箱子障碍物动作一 */
    (void)Motor_XYZ(0, 300, 20, 0,       /* X: 不动 */
                    0, 100, 20, 100.0f,  /* Y: dir=0(后), 100rpm, acc=20, 100cm */
                    0, 0.0f);             /* Z: 不动 */
    osDelay(6000);

    /* 豆子到箱子障碍物动作2 */
    (void)Motor_XYZ(1, 300, 20, 65.0f,   /* X: dir=1(右), 300rpm, acc=20, 65cm */
                    0, 50, 20, 160.0f,   /* Y: dir=0(后), 50rpm, acc=20, 160cm */
                    0, 0.0f);             /* Z: 不动 */
    osDelay(6000);

    k230_write(2);      // 开启看中间数字
    osDelay(1000);      // 等待k230返回结果
    k230_read(&huart2); // 解析k230返回结果
    osDelay(500);       // 等待k230解析完成
    k230_write(6);      // 给k230发送关闭摄像头命令

    /* 箱子障碍物到箱子 */
    (void)Motor_XYZ(1, 300, 20, 0,       /* X: 不动 */
                    0, 100, 20, 55.0f,   /* Y: dir=0(后), 100rpm, acc=20, 55cm */
                    0, 0.0f);             /* Z: 不动 */
    osDelay(8000);


    k230_write(3);      // 开启看侧面数字
    osDelay(1000);      // 等待k230返回结果
    k230_read(&huart2); // 解析k230返回结果
    osDelay(500);       // 等待k230解析完成
    k230_write(6);      // 给k230发送关闭摄像头命令

}
