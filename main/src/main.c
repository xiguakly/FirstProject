/*==================================笔记===================== 
1.LVGL使用方式
    1.1在所有与lvgl屏幕的相关代码开始之前，需要lv_init();
    1.2当所有配置初始化完成后，创建一块空画布，可在这个画布上添加控件，这块画布就是父容器
        1.2.1:定义控件指针 lv_obj_t *NewScr
        1.2.2:创建新画布 NewScr = lv_obj_create(NULL)，参数一定为NULL，NULL代表创建新的空画布
                lv_obj_create(lv_src_act())，参数为lv_src_act()，这代表默认屏幕，不用创建就存在的初始画布，是所有控件
                    的父容器。
        1.2.3:设置显示效果：lv_obj_set_style_bg_color(屏幕指针，颜色，传参3)
        1.2.4：屏幕对齐：这一步没有，自动铺满整个屏幕
        1.2.5：在这个屏幕内创建控件。
    注：跳转不同屏幕函数：lv_scr_load(屏幕指针)
2.创建控件步骤：
    2.1:定义控件指针 lv_obj_t *xxx
    2.2:调用对应创建API： lv_xxx_create(父容器)
        父容器：    1.当前屏幕 = lv_scr_act()
                    2.别的控件 = lv_obj_t *xxx里的xxx
                    3.null = 独立新屏幕
    2.3：设置显示效果：调用不同API设置长宽高，颜色，等各种显示效果
    2.4：设置位置对齐：对应在父容器的什么位置lv_obj_align(控件，父对象，对齐模数，x偏移，y偏移)
    2.5：配置专属功能：不同控件有不同的功能，调用不同API设置专属功能
=========================================================*/


// 解决MinGW下SDL、标准库函数隐式声明警告
#define _DEFAULT_SOURCE

// C标准库，LVGL内存分配、工具函数依赖
#include <stdlib.h>
// SDL2库：实现窗口、渲染、时钟、关闭事件，模拟LCD屏幕硬件
#include <SDL2/SDL.h>
// LVGL总头文件，包含全部控件、驱动、样式、系统API
#include "lvgl/lvgl.h"
// 官方示例控件，暂时不用注释关闭
//#include "lvgl/examples/lv_examples.h"

// 屏幕分辨率宏：宽800像素，高480像素
#define W 800
#define H 480

// 全局LVGL单帧显存缓存：存储一整屏像素数据，static全局常驻内存
static lv_color_t frame_buf1[W * H];
static lv_color_t frame_buf2[W * H];
//鼠标输入相关变量
static int16_t mouse_x = 0;       
static int16_t mouse_y = 0;
static bool mouse_down = false;

// SDL渲染全局对象，回调函数flush_cb需要直接访问，设为static全局
static SDL_Window* win = NULL;        // 电脑仿真窗口句柄
static SDL_Renderer* ren = NULL;      // SDL软件渲染器
static SDL_Texture* tex = NULL;       // 像素纹理，对接LVGL显存
static lv_obj_t* label=NULL;           //标签名称

//变量
static bool APP_Running = true;
//======================================自定义函数声明===========================================//
static void mouse_drv_init(void);
static Uint32 get_mouse_state(SDL_Event *e);
static void mouse_read_cb(lv_indev_drv_t *indev_drv,
                        lv_indev_data_t *data);
static void btn_click_event(lv_event_t *e);
//======================================自定义函数声明===========================================//

/**
 * @brief LVGL屏幕刷新回调函数（底层驱动核心）
 * LVGL完成画面绘制后自动调用，把内存缓存图像推送到SDL窗口
 * @param drv 显示驱动实例句柄
 * @param area 本次刷新区域（局部刷新使用，本工程全屏刷新未用到）
 * @param buf LVGL绘制完成的像素缓存数组
 */
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *buf)
{
    /* ren即SDL渲染器的句柄 */
    //设置渲染器的清屏底色
    SDL_SetRenderDrawColor(ren,0x88,0x88,0x88,255);
    //清屏
    SDL_RenderClear(ren);
    // 将LVGL像素数据更新到SDL纹理，NULL代表更新整张屏幕
    SDL_UpdateTexture(tex, NULL, buf, W * sizeof(lv_color_t));
    // 将纹理送入渲染队列缓存
    SDL_RenderCopy(ren, tex, NULL, NULL);
    // 把缓存画面真正渲染到窗口界面
    SDL_RenderPresent(ren);
    // 关键：通知LVGL屏幕刷新完成，允许下一帧绘制，不加会界面卡死
    lv_disp_flush_ready(drv);
}

/**
 * @brief 硬件抽象层初始化（HAL层）
 * 模拟单片机LCD初始化，完成SDL窗口创建 + LVGL显示驱动注册
 * 属于底层固定代码，界面业务修改无需改动此处
 */
static void simple_hal_init(void)
{
    // 初始化SDL视频子系统，仅启用窗口绘图功能
    SDL_Init(SDL_INIT_VIDEO);
    // 创建SDL窗口：标题、居中位置、宽高、无特殊属性
    win = SDL_CreateWindow("TFT Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
    // 创建软件渲染器，兼容所有显卡，纯CPU绘图
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    // 创建流式纹理，像素格式RGB565和LVGL匹配，支持实时更新像素
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STATIC, W, H);

    // LVGL绘制缓存结构体
    static lv_disp_draw_buf_t disp_buf;
    // 初始化绘图缓存：主缓存frame_buf，无副缓存（单缓冲），总像素数量W*H
    lv_disp_draw_buf_init(&disp_buf, frame_buf1, frame_buf2, W * H);

    // LVGL屏幕驱动配置结构体
    static lv_disp_drv_t disp_drv;
    // 填充驱动默认参数
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;      // 绑定绘图内存缓存
    disp_drv.flush_cb = flush_cb;       // 绑定屏幕刷新回调
    disp_drv.hor_res = W;               // 屏幕横向分辨率
    disp_drv.ver_res = H;               // 屏幕纵向分辨率
    disp_drv.full_refresh = true;
    lv_disp_drv_register(&disp_drv);    // 向LVGL注册屏幕驱动，使LVGL识别显示屏

    // 设置主屏幕背景灰色，区分前景文字控件
    // lv_scr_act()：获取当前激活的顶层屏幕（所有控件的父容器）
    // 0：样式作用于控件默认普通状态
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x888888), 0);
    //鼠标驱动初始化
    mouse_drv_init();
}

int main(int argc, char **argv)
{
    // 屏蔽命令行参数未使用警告
    (void)argc;
    (void)argv;

    // 1. LVGL库内核初始化，程序最优先执行
    lv_init();
    // 2. 初始化SDL仿真屏幕底层驱动
    simple_hal_init();
    //创建新屏幕
    //lv_obj_create(NULL);    
    // ==================== UI业务界面代码 ====================
    // 创建文字标签，父容器为主屏幕
    label = lv_label_create(lv_scr_act());
    // 设置标签显示文本
    lv_label_set_text(label, "LVGL Test Text OK");
    // 把控件在父容器居中
    lv_obj_center(label);
    // 设置文字颜色为黑色
    lv_obj_set_style_text_color(label, lv_color_black(), 0);

    // 创建按钮控件，父容器为主屏幕
    lv_obj_t *test_btn = lv_btn_create(lv_scr_act());
    // 设置按钮尺寸：宽140px，高60px
    lv_obj_set_size(test_btn,140,60);
    // 对齐：父容器底部居中，X偏移0，向上偏移15像素
    lv_obj_align(test_btn,LV_ALIGN_BOTTOM_MID,0,-15);

    // 【修复原代码错误】按钮内部创建文字标签，父容器是按钮test_btn
    lv_obj_t *btn_text = lv_label_create(test_btn);
    // 标签专属接口设置文字
    lv_label_set_text(btn_text,"button1");
    // 文字在按钮内部居中
    lv_obj_center(btn_text);
    // 按钮文字设置为白色
    lv_obj_set_style_text_color(btn_text,lv_color_white(),0);
    //给按钮绑定点击事件
    //参数1：按钮名称，参数2：回调函数,参数3：监听事件，参数4：自定义参数
    lv_obj_add_event_cb(test_btn, btn_click_event , LV_EVENT_CLICKED , NULL);

    // 强制立刻刷新一帧，程序启动直接渲染界面，无需等待循环调度
    //lv_refr_now(NULL);

    // ==================== 主循环变量定义 ====================
    SDL_Event e;
    // 记录上一次给LVGL提供心跳的时间戳
    uint32_t tick = SDL_GetTicks(); 

    // ==================== 嵌入式标准死循环 ====================
    while(APP_Running)
    {
        // 轮询SDL所有窗口事件：关闭窗口、鼠标点击、移动
        get_mouse_state(&e);
        uint32_t now = SDL_GetTicks();
        // 每5ms给LVGL送入一次计时节拍，驱动动画、长按、定时器
        if(now - tick >= 15){
            lv_tick_inc(15);
            tick = now;
        }
        // LVGL核心调度函数：刷新界面、执行动画、检测交互、调用flush_cb
        lv_timer_handler();
        // 短暂延时，降低CPU占用，不影响画面流畅度
        SDL_Delay(2); 
    } 
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}


//======================================自定义函数===========================================//
//鼠标驱动初始化
static void mouse_drv_init(void)
{
    static lv_indev_drv_t mouse_indev_drv;
    lv_indev_drv_init(&mouse_indev_drv);
    mouse_indev_drv.type = LV_INDEV_TYPE_POINTER;
    mouse_indev_drv.read_cb = mouse_read_cb;
    lv_indev_drv_register(&mouse_indev_drv);
}
static Uint32 get_mouse_state(SDL_Event *e)
{
    //轮询SDL所有窗口事件，关闭窗口，鼠标移动，鼠标左键按下，鼠标左键抬起；
    while(SDL_PollEvent(e))
    {
        if(e->type==SDL_QUIT)
        {
            APP_Running = false;
            return e->type;
        }
        //鼠标移动，实时更新x、y坐标
        if(e->type == SDL_MOUSEMOTION)
        {
            mouse_x=(int16_t)e->motion.x;
            mouse_y=(int16_t)e->motion.y;
        }
        if(e->type==SDL_MOUSEBUTTONDOWN&&
            e->button.button==SDL_BUTTON_LEFT)
        {
            mouse_down=true;
        }
        if(e->type==SDL_MOUSEBUTTONUP&&
            e->button.button==SDL_BUTTON_LEFT)
        {
            mouse_down=false;
        }
    }
    return 0;   
}
//鼠标输入驱动回调，LVGL需要读取鼠标坐标与按键状态
static void mouse_read_cb(lv_indev_drv_t *indev_drv,
                        lv_indev_data_t *data)
{
    //传递鼠标坐标
    data->point.x=mouse_x;
    data->point.y=mouse_y;
    if(mouse_down)
    {
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }

}
//按钮点击回调函数
static void btn_click_event(lv_event_t *e)
{
    //获取触发事件，就是按钮
    lv_obj_t *target = lv_event_get_target(e);
    const char *txt;
    //判断事件类型，点击释放
    if(lv_event_get_code(e)==LV_EVENT_CLICKED)
    {

        txt=lv_label_get_text(label);
        if(strcmp(txt,"button click")==0)
        {
            //修改顶部文字内容
            lv_label_set_text(label,"LVGL Test Text OK"); 
        }    
        else if(strcmp(txt,"LVGL Test Text OK")==0)
        {
            lv_label_set_text(label,"button click");
        }
        //lv_refr_now(NULL);
    }
}
//======================================自定义函数===========================================//