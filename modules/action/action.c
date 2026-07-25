/**
  ******************************************************************************
  * @file    action.c
  * @brief   动作序列: 抓豆子 -> 放箱子 (Modules层)
  ******************************************************************************
  * action_1 任务: 上电后自动跑预设动作序列, 不依赖串口命令。
  * 分两段:
  *   - action_douzi_first:   抓豆子(起始->抓左边豆子->抓中间->准备去放箱子)
  *   - action_xiangzi_first: 放箱子(障碍物避让->到箱子)
  * Motor_XYZ 为非阻塞(发完命令即返回), 靠 osDelay 等电机走完。
  ******************************************************************************
  */

#include "action.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "motor.h"       /* Motor_XYZ */
#include "K230.h"        /* k230_write, k230_read */

/* ---- 任务参数 ---- */
#define ACTION_TASK_STACK_SIZE   1024               /* 堆栈(word) */
#define ACTION_TASK_PRIORITY     osPriorityNormal   /* 与 MotorAutoTask 同级 */
#define ACTION_STARTUP_DELAY     1000               /* 等电机使能+M2006反馈稳定(ms) */

/* ---- 内部函数 ---- */
static void action_1(void *argument);
static void action_douzi_first(void);
static void action_xiangzi_first(void);



//备注一下后面的左边指有电池那边，右边指没电池那边



/**
  * @brief  初始化动作序列: 创建 action_1 任务
  */
void Action_Init(void)
{
    const osThreadAttr_t attr = {
        .name = "ActionTask",
        .stack_size = ACTION_TASK_STACK_SIZE * 4,
        .priority = (osPriority_t)ACTION_TASK_PRIORITY,
    };
    osThreadNew(action_1, NULL, &attr);
}

/**
 * @brief  上电自动动作任务: 依次跑抓豆子 + 放箱子序列, 完成后挂起
 */
static void action_1(void *argument)
{
    (void)argument;
    osDelay(ACTION_STARTUP_DELAY); /* 等电机使能 + M2006 反馈稳定 */



    /////////////////////////////////////////////////*
    //我只写了一套动作的xy的动作，然后这个动作里面你的视觉没有作用,我写那两个函数是为了方便让你检查单片机对你数据的收发是否正常
    /////////////////////////////////////////



    k230_write(1);//给k230发送看豆命令
    osDelay(1000);//等待k230返回结果
    k230_read(&huart2);//解析k230返回结果
    osDelay(500);//等待k230解析完成
    k230_write(6);//给k230发送关闭摄像头命令


    action_douzi_first();   /* 抓豆子 ，动作截止到抓完豆子已经完成升降结构移到右边*/

    // 数字识别我放在action_xiangzi_first里面了
    action_xiangzi_first(); /* 放箱子 */

    for (;;)
    {
        osDelay(1000);
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
                    1, 100, 20, 185,    /* Y: dir=1(前), 100rpm, acc=20, 185cm */
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
                    0, 100, 20, 58,      /* Y: dir=0(后), 100rpm, acc=20, 58cm */
                    0, 0.0f);             /* Z: 不动 */
    osDelay(8000);


    k230_write(3);      // 开启看侧面数字
    osDelay(1000);      // 等待k230返回结果
    k230_read(&huart2); // 解析k230返回结果
    osDelay(500);       // 等待k230解析完成
    k230_write(6);      // 给k230发送关闭摄像头命令



}
