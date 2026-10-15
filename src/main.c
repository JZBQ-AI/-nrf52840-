#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define touch_NODE DT_NODELABEL(touch)

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

#define LED_COUNT 4
#define FLOW_DELAY_MS 200

static const struct gpio_dt_spec touch =
    GPIO_DT_SPEC_GET(touch_NODE, gpios);

static const struct gpio_dt_spec leds[LED_COUNT] = {
    GPIO_DT_SPEC_GET(LED0_NODE, gpios),
    GPIO_DT_SPEC_GET(LED1_NODE, gpios),
    GPIO_DT_SPEC_GET(LED2_NODE, gpios),
    GPIO_DT_SPEC_GET(LED3_NODE, gpios),
};

static struct gpio_callback touch_cb;
static atomic_t flow_started;
K_SEM_DEFINE(start_sem, 0, 1);

/* GPIO 中断回调函数 */
static void button_callback(const struct device *port,
                            struct gpio_callback *cb,
                            gpio_port_pins_t pins)
{
    ARG_UNUSED(port);
    ARG_UNUSED(cb);

    if ((pins & BIT(touch.pin)) != 0U)
    {

        if (atomic_cas(&flow_started, 0, 1)) {
            k_sem_give(&start_sem);
        }
    }
}

int main(void)
{
    int ret;

    /* 检查并配置4个LED */
    for (int i = 0; i < LED_COUNT; i++) {
        if (!gpio_is_ready_dt(&leds[i])) {
            printk("LED%d is not ready\n", i);
            return 0;
        }

        ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            printk("LED%d configure failed: %d\n", i, ret);
            return 0;
        }
    }

    /* 检查并配置按键 */
    if (!gpio_is_ready_dt(&touch)) {
        printk("Button is not ready\n");
        return 0;
    }

    ret = gpio_pin_configure_dt(&touch, GPIO_INPUT);
    if (ret < 0) {
        printk("Button configure failed: %d\n", ret);
        return 0;
    }

    /* 按键从未按下变为按下时触发中断 */
    ret = gpio_pin_interrupt_configure_dt(
        &touch, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("Interrupt configure failed: %d\n", ret);
        return 0;
    }

    gpio_init_callback(&touch_cb,
                       button_callback,
                       BIT(touch.pin));

    ret = gpio_add_callback(touch.port, &touch_cb);
    if (ret < 0) {
        printk("Callback registration failed: %d\n", ret);
        return 0;
    }

    printk("Press touch to start flow LEDs\n");

    /* 等待按键中断发出启动信号 */
    k_sem_take(&start_sem, K_FOREVER);

    printk("Flow LEDs started\n");

    while (1) {
        for (int i = 0; i < LED_COUNT; i++) {
            /* 保证当前只有一个LED亮 */
            for (int j = 0; j < LED_COUNT; j++) {
                gpio_pin_set_dt(&leds[j], j == i);
            }
            k_msleep(FLOW_DELAY_MS);
        }
    }
    return 0;
}
















/* #define touch_NODE DT_NODELABEL(touch)//定义别名
struct gpio_dt_spec touch_struct = GPIO_DT_SPEC_GET(touch_NODE, gpios);//定义GPIO属性
struct gpio_callback gpio_cb_struct={0};//定义回调函数结构体

    /**
     * @brief 回调函数
     *
     * @param port Device struct for the GPIO device.
     * @param cb Original struct gpio_callback owning this handler
     * @param pins Mask of pins that triggers the callback handler
    
     void gpio_callback (const struct device *port,
					struct gpio_callback *cb,
					gpio_port_pins_t pins)
                    
{
    if(pins==BIT(touch_struct.pin))//判断是否是按键引脚触发的中断
    {
        printk("Button pressed!\n");
    }
}

int main(void)
{
    // 检查按键硬件外设是否就绪
    if (gpio_is_ready_dt(&touch_struct) == false) 
    {
        printk("Button is not ready\n");
        // printk——zephyr内核的打印函数(库较小)，printf——C标准库的打印函数
        return 0; // 按键设备异常，直接退出main
    }
    // 配置为输入
    gpio_pin_configure_dt(&touch_struct, GPIO_INPUT);
    //配置为上升沿触发中断
    gpio_pin_interrupt_configure_dt(&touch_struct, GPIO_INT_EDGE_TO_ACTIVE);
    //初始化回调函数
    gpio_init_callback(&gpio_cb_struct, gpio_callback, BIT(touch_struct.pin));
    //注册回调函数
    gpio_add_callback(touch_struct.port, &gpio_cb_struct);
    // 读取一次按键当前电平（**只在程序启动瞬间读一次，不在循环内**）
    int res = gpio_pin_get_dt(&touch_struct);
    printk("Button state: %d\n", res);

    return 0;
}  */




/*LED0闪烁

//获取设备树里 touch 节点的GPIO信息
#define BUTTON_NODE DT_NODELABEL(touch)  
struct gpio_dt_spec button_struct=GPIO_DT_SPEC_GET(BUTTON_NODE,gpios);

// 获取设备树里 led0 节点的GPIO信息
#define LED_NODE DT_NODELABEL(led0)
struct gpio_dt_spec led_struct=GPIO_DT_SPEC_GET(LED_NODE,gpios);

int main(void)
{
    // 检查按键硬件外设是否就绪
    if (gpio_is_ready_dt(&button_struct)==false) 
    {
        printf("Button is not ready\n");
        return 0; // 按键设备异常，直接退出main
    }
    // 检查LED硬件外设是否就绪
    if (gpio_is_ready_dt(&led_struct)==false) 
    {
        printf("LED is not ready\n");
        return 0; // LED设备异常，直接退出main
    }

    // 配置按键：输入、低电平有效、上拉电阻
    gpio_pin_configure_dt(&button_struct,GPIO_INPUT|GPIO_ACTIVE_LOW|GPIO_PULL_UP);
    // 配置LED：输出、低电平有效
    gpio_pin_configure_dt(&led_struct,GPIO_OUTPUT|GPIO_ACTIVE_LOW);

    // 读取一次按键当前电平（**只在程序启动瞬间读一次，不在循环内**）
    int res=gpio_pin_get_dt(&button_struct);

    // 设置LED逻辑值=1
    // 因为配置了GPIO_ACTIVE_LOW，逻辑1 → 引脚输出低电平
    gpio_pin_set_dt(&led_struct,1);

    // 无限循环
    while (1)
    {
        gpio_pin_toggle_dt(&led_struct); // 翻转LED逻辑电平
        k_sleep(K_SECONDS(1));            // 线程休眠1秒，让出CPU
    }
    return 0;
}
 */