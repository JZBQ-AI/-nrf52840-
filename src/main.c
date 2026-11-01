#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

//定义栈大小
#define STACK_THREAD0_SIZE 512
#define STACK_THREAD1_SIZE 512
#define STACK_WORKER_SIZE 512

//定义线程优先级
#define THREAD0_PRIORITY 2
#define THREAD1_PRIORITY 3
#define WORKER_PRIORITY 4

K_THREAD_STACK_DEFINE(my_stack_area, STACK_THREAD0_SIZE);
struct k_work_q offload_work_q;//线程列队结构体
struct k_work work;//工作项结构体

void emulate_work(void)
{
    for (volatile int i = 0; i < 100000; i++);
}
void offload_function(struct k_work *work)
{
    uint64_t ts_start = k_uptime_get();
    emulate_work();
    int64_t work_run_time = k_uptime_delta(&ts_start);
    printk("[WORKER] work execute time: %lld ms\n", work_run_time);
}
void thread0(void)
{
        //初始化
    k_work_queue_start(&offload_work_q,my_stack_area,
                       STACK_WORKER_SIZE,
                       WORKER_PRIORITY,
                       NULL);
    k_work_init(&work, offload_function);
    uint64_t time_stap;
    int64_t delta_time;
    while (1)
    {
        time_stap = k_uptime_get();
        k_work_submit_to_queue(&offload_work_q, &work);
        delta_time = k_uptime_delta(&time_stap);
        printk("Thread 0 is running, delta time: %lld ms\n", delta_time);
        k_msleep(20);
    }
}
void thread1(void)
{
    uint64_t time_stap;
    int64_t delta_time;
    while (1)
    {
        time_stap = k_uptime_get();
        emulate_work();
        delta_time = k_uptime_delta(&time_stap);
        printk("Thread 1 is running, delta time: %lld ms\n", delta_time);
        k_msleep(20);
    }
}
K_THREAD_DEFINE(thread0_id, STACK_THREAD0_SIZE, thread0, NULL, NULL, NULL, THREAD0_PRIORITY, 0, 0);     
K_THREAD_DEFINE(thread1_id, STACK_THREAD1_SIZE, thread1, NULL, NULL, NULL, THREAD1_PRIORITY, 0, 0);




/* //多线程学习
#include <stdio.h>
#include "zephyr/kernel.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>
#define STACKSIZE 1024	   // 栈大小
#define THREAD0_PRIORITY 6 // 优先级高会打断优先级低
#define THREAD1_PRIORITY 7 // 线程1 优先级
#define LED_COUNT       2

static const struct gpio_dt_spec leds[LED_COUNT] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
};
void thread0()
{
    gpio_pin_configure_dt(&leds[0],GPIO_OUTPUT|GPIO_ACTIVE_LOW);
    gpio_pin_set_dt(&leds[0],1);
    gpio_pin_configure_dt(&leds[1],GPIO_OUTPUT|GPIO_ACTIVE_LOW);
    gpio_pin_set_dt(&leds[1],1);
	while (1)
	{
        
        gpio_pin_toggle_dt(&leds[0]); // 翻转LED逻辑电平
        gpio_pin_toggle_dt(&leds[1]);
		printk("hello i am thread0\n");
        k_msleep(1000); // 让出CPU，线程0进入就绪态
	}
}
 void thread1()
{
    gpio_pin_configure_dt(&leds[1],GPIO_OUTPUT|GPIO_ACTIVE_LOW);
    gpio_pin_set_dt(&leds[1],1);
	while (1)
	{
        gpio_pin_toggle_dt(&leds[1]); // 翻转LED逻辑电平
		printk("hello i am thread1\n");
        k_msleep(100); // 让出CPU，线程1进入就绪态
	}
} 
// 静态方式创建线程
K_THREAD_DEFINE(thread0_id, STACKSIZE, thread0, NULL, NULL, NULL, THREAD0_PRIORITY, 0, 0);
K_THREAD_DEFINE(thread1_id, STACKSIZE, thread1, NULL, NULL, NULL, THREAD1_PRIORITY, 0, 0);

 int main(void)
{
	k_sleep(K_SECONDS(1));
	return 0;
}  */



                    
/* //加速度传感器 LIS2DH12 测试程序
#include <errno.h>
#include <stdint.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define LIS2DH12_REG_WHO_AM_I       0x0F
#define LIS2DH12_REG_CTRL1          0x20
#define LIS2DH12_REG_CTRL4          0x23
#define LIS2DH12_REG_STATUS         0x27
#define LIS2DH12_REG_OUT_X_L        0x28
#define LIS2DH12_WHO_AM_I_VALUE     0x33
#define LIS2DH12_AUTO_INCREMENT     BIT(7)

//CTRL_REG1：100 Hz，X/Y/Z 启用，高分辨率模式。
#define LIS2DH12_CTRL1_ODR_100_HZ   (5U << 4)
#define LIS2DH12_CTRL1_XYZ_ENABLE   (BIT(0) | BIT(1) | BIT(2))
#define LIS2DH12_CTRL1_VALUE        \
	(LIS2DH12_CTRL1_ODR_100_HZ | LIS2DH12_CTRL1_XYZ_ENABLE)

//CTRL_REG4：块数据更新，高分辨率，量程为±2g
#define LIS2DH12_CTRL4_BDU          BIT(7)
#define LIS2DH12_CTRL4_FS_2G        (0U << 4)
#define LIS2DH12_CTRL4_HR           BIT(3)
#define LIS2DH12_CTRL4_VALUE        \
	(LIS2DH12_CTRL4_BDU | LIS2DH12_CTRL4_FS_2G | LIS2DH12_CTRL4_HR)

#define LIS2DH12_STATUS_DRDY_MASK   BIT_MASK(4)
#define LIS2DH12_ACCEL_BURST_SIZE   7U

struct lis2dh12_sample {
	uint8_t status;

	int16_t x_raw;
	int16_t y_raw;
	int16_t z_raw;

	int32_t x_mg;
	int32_t y_mg;
	int32_t z_mg;
};

static const struct i2c_dt_spec lis2dh12 =
	I2C_DT_SPEC_GET(DT_NODELABEL(myaccel));

static int lis2dh12_write_reg(uint8_t reg, uint8_t value)
{
	uint8_t buffer[2] = {reg, value};

	return i2c_write_dt(&lis2dh12, buffer, sizeof(buffer));
}

static int lis2dh12_init(void)
{
	uint8_t id = 0;
	int ret;

	if (!i2c_is_ready_dt(&lis2dh12)) {
		printk("LIS2DH12: I2C bus is not ready\n");
		return -ENODEV;
	}
	//The device needs a few milliseconds after power-on. 
	k_sleep(K_MSEC(10));

	ret = i2c_reg_read_byte_dt(&lis2dh12, LIS2DH12_REG_WHO_AM_I, &id);
	if (ret < 0) {
		printk("LIS2DH12: WHO_AM_I read failed: %d\n", ret);
		return ret;
	}
	if (id != LIS2DH12_WHO_AM_I_VALUE) {
		printk("LIS2DH12: unexpected WHO_AM_I 0x%02X\n", id);
		return -ENODEV;
	}
	// 在更改传感器配置时，请停止传感器
	ret = lis2dh12_write_reg(LIS2DH12_REG_CTRL1, 0);
	if (ret < 0) {
		return ret;
	}

	ret = lis2dh12_write_reg(LIS2DH12_REG_CTRL4, LIS2DH12_CTRL4_VALUE);
	if (ret < 0) {
		return ret;
	}

	ret = lis2dh12_write_reg(LIS2DH12_REG_CTRL1, LIS2DH12_CTRL1_VALUE);
	if (ret < 0) {
		return ret;
	}

	k_sleep(K_MSEC(20));
	printk("LIS2DH12: initialized, WHO_AM_I=0x%02X\n", id);

	return 0;
}
//在±2 g的高分辨率模式下，12位原始值的标度为
//每计数单位约1000/1024毫克。
static int32_t lis2dh12_raw_to_mg(int16_t raw)
{
	int32_t scaled = (int32_t)raw * 1000;

	return (scaled >= 0) ? ((scaled + 512) / 1024)
			     : ((scaled - 512) / 1024);
}

static void lis2dh12_get_axis(const uint8_t data[LIS2DH12_ACCEL_BURST_SIZE],
			      size_t offset, int16_t *raw, int32_t *mg)
{
	int16_t value;

	value = (int16_t)((uint16_t)data[offset] |
			  ((uint16_t)data[offset + 1] << 8));

	//LIS2DH 数据为左对齐；保留带符号的 12 位值。
	*raw = value >> 4;
	*mg = lis2dh12_raw_to_mg(*raw);
}

static int lis2dh12_read(struct lis2dh12_sample *sample)
{
	uint8_t data[LIS2DH12_ACCEL_BURST_SIZE];
	int ret;
	//
    //在一次自动递增的传输中读取 STATUS 以及随后的 X/Y/Z 的 LSB/MSB。
    //这样可以确保三个轴的数据均来自同一个采样点。
    //
	ret = i2c_burst_read_dt(
		&lis2dh12,
		LIS2DH12_REG_STATUS | LIS2DH12_AUTO_INCREMENT,
		data,
		sizeof(data));

	if (ret < 0) {
		return ret;
	}

	sample->status = data[0];
	if ((sample->status & LIS2DH12_STATUS_DRDY_MASK) == 0U) {
		return -EAGAIN;
	}

	lis2dh12_get_axis(data, 1, &sample->x_raw, &sample->x_mg);
	lis2dh12_get_axis(data, 3, &sample->y_raw, &sample->y_mg);
	lis2dh12_get_axis(data, 5, &sample->z_raw, &sample->z_mg);

	return 0;
}
int main(void)
{
	struct lis2dh12_sample sample = {0};
	int ret;

	ret = lis2dh12_init();
	if (ret < 0) {
		printk("LIS2DH12: initialization failed: %d\n", ret);
		while (1) {
			k_sleep(K_FOREVER);
		}
	}
	while (1) {
		ret = lis2dh12_read(&sample);
		if (ret == 0) {
			printk("raw X=%d Y=%d Z=%d | mg "
			       "X=%d Y=%d Z=%d\n",
			       sample.x_raw, sample.y_raw, sample.z_raw,
			       sample.x_mg, sample.y_mg, sample.z_mg);
		} else if (ret != -EAGAIN) {
			printk("LIS2DH12: read failed: %d\n", ret);
		}

		k_sleep(K_MSEC(100));
	}
} */




/*i2c通信测试
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
static struct i2c_dt_spec i2c0_struct = I2C_DT_SPEC_GET(DT_NODELABEL(myaccel));

int main(void)
{

    while (1) 
    {
        uint8_t who_am_i = 0x0f;
        uint8_t rx_buf[1] = {0};
        i2c_write_dt(&i2c0_struct,&who_am_i, 1);   
        i2c_read_dt(&i2c0_struct, rx_buf, 1);
        k_sleep(K_SECONDS(0.1));
    }
    return 0;
} */




/* //串口异步收发（调用中断）
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);//设置日志输出级别为DBG，低于该级别的日志不输出
#define UART_NODE DT_NODELABEL(uart0)
const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);
uint8_t rx_buf[10]={0};
void uart_cd(const struct device *dev, 
    struct uart_event *evt, void *user_data)
{
    switch(evt->type)
    {
        case UART_TX_DONE:
            LOG_INF("整个TX缓冲区已传输完毕。");
            break;
        case UART_TX_ABORTED:
            LOG_INF("由于超时或调用了uart_tx_abort，传输被中止");
            break;
        case UART_RX_RDY:
            LOG_INF("已接收的数据已准备好进行处理。");
            LOG_INF("接收到数据时：%s", evt->data.rx.buf);
            break;
        case UART_RX_BUF_REQUEST:
            LOG_INF("驱动程序请求下一个缓冲区以实现连续接收");
            break;
        case UART_RX_BUF_RELEASED:
            LOG_INF(" UART 驱动程序不再使用缓冲区");
            break;
        case  UART_RX_DISABLED: 
            uart_rx_enable(uart_dev,rx_buf,sizeof(rx_buf),100);
            LOG_INF(" RX 已被禁用，可以重新启用。");
            break;
        case  UART_RX_STOPPED:
            LOG_INF("RX 因外部事件而停止运行。");
            break;
        default:
            LOG_INF("Unknown event type: %d", evt->type);
            break;
    }

}
int main(void)
{
    struct uart_config cfg = {
        .baudrate=115200,
        .parity=UART_CFG_PARITY_NONE,//无奇偶校验
        .stop_bits=UART_CFG_STOP_BITS_1,
        .data_bits=UART_CFG_DATA_BITS_8,
        .flow_ctrl=UART_CFG_FLOW_CTRL_NONE//无流控
    };
    // 配置UART
    uart_configure(uart_dev, &cfg);
    //配置回调函数
    uart_callback_set(uart_dev, uart_cd, NULL);
    uart_rx_enable(uart_dev,rx_buf,sizeof(rx_buf),100);
    while (1) 
    {
        k_sleep(K_SECONDS(1));
    }
} */




/* //中断流水灯
//  * 1. 按键0触摸一次，4个LED流水点亮，按键0触摸第二次，3个LED流水点亮，按键0触摸第三次，2个LED流水点亮，按键0触摸第四次，LED0常亮，按键0触摸第五次，全部熄灭。
//  * 2. 按键0触摸时有消抖处理。
//  * 3. LED流水点亮的速度为200ms/盏。
//  * 4. LED0常亮时不闪烁。
 
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(main,LOG_LEVEL_ERR);//设置日志输出级别为ERR，低于该级别的日志不输出

#define button0_NODE DT_NODELABEL(button0)
#define LED_COUNT       4
#define FLOW_DELAY_MS   200
#define DEBOUNCE_MS     200

static const struct gpio_dt_spec button0 =
    GPIO_DT_SPEC_GET(button0_NODE, gpios);
static const struct gpio_dt_spec leds[LED_COUNT] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
};
static struct gpio_callback button0_cb;
// 0=全灭，1=四灯流水，2=三灯流水，3=两灯流水，4=单灯常亮 
static atomic_t mode;
//用于通知主线程模式发生变化 
K_SEM_DEFINE(mode_sem, 0, 1);
// 只在 GPIO 中断中访问 
static int64_t last_button0_ms;
static bool has_button0ed;
//触摸中断回调 
static void button0_callback(const struct device *port,
                           struct gpio_callback *cb,
                           gpio_port_pins_t pins)
{
    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    if ((pins & BIT(button0.pin)) == 0U) {
        return;
    }
    int64_t now = k_uptime_get();
    // 过滤短时间内重复产生的触摸中断 (消抖)
    if (has_button0ed && (now - last_button0_ms < DEBOUNCE_MS)) {
        return;
    }
    has_button0ed = true;
    last_button0_ms = now;
    int next_mode = (atomic_get(&mode) + 1) % 5;
    atomic_set(&mode, next_mode);
    k_sem_give(&mode_sem);
}
//index 对应的灯亮，其余灯灭；index=-1 时全灭
static int show_led(int index)
{
    int ret;
    // 先全部关闭，再开启目标灯
    for (int i = 0; i < LED_COUNT; i++) 
    {
        ret = gpio_pin_set_dt(&leds[i], 0);
        if (ret < 0) 
        {
            printk("LED%d off failed: %d\n", i, ret);
            return ret;
        }
    }
    if (index >= 0 && index < LED_COUNT) 
    {
        ret = gpio_pin_set_dt(&leds[index], 1);
        if (ret < 0) {
            printk("LED%d on failed: %d\n", index, ret);
            return ret;
        }
    }
    return 0;
}
int main(void)
{
    int ret;
    // 检查并配置4个LED
    for (int i = 0; i < LED_COUNT; i++) {
        if (!gpio_is_ready_dt(&leds[i])) {
            printk("LED%d is not ready\n", i);
            return 0;
        }
        ret = gpio_pin_configure_dt(
            &leds[i], GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            printk("LED%d configure failed: %d\n", i, ret);
            return 0;
        }
    }
    if (!gpio_is_ready_dt(&button0)) {
        printk("button0 is not ready\n");
        return 0;
    }
    ret = gpio_pin_configure_dt(&button0, GPIO_INPUT);
    if (ret < 0) {
        printk("button0 configure failed: %d\n", ret);
        return 0;
    }
    // 先注册回调，再启用中断
    gpio_init_callback(&button0_cb,
                       button0_callback,
                       BIT(button0.pin));
    ret = gpio_add_callback(button0.port, &button0_cb);
    if (ret < 0) {
        printk("Callback registration failed: %d\n", ret);
        return 0;
    }
    ret = gpio_pin_interrupt_configure_dt(
        &button0, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("Interrupt configure failed: %d\n", ret);
        return 0;
    }
    printk("Ready: button0 to change LED mode\n");

    int previous_mode = -1;
    int led_index = 0;
    while (1) {
        LOG_INF("HELLOWORLD");//cpu空闲时有输出
        LOG_ERR("HELLOWORLD");//输出与优先级设置有关
        // 读取当前模式
        int current_mode = atomic_get(&mode);
        // 切换模式时，从 LED0 重新开始
        if (current_mode != previous_mode) {
            previous_mode = current_mode;
            led_index = 0;

            printk("Mode: %d\n", current_mode);
        }
        if (current_mode == 0) {
            // 全灭，等待下一次触摸
            if (show_led(-1) < 0) {
                return 0;
            }
            k_sem_take(&mode_sem, K_FOREVER);
        } else if (current_mode == 4) {
            // LED0 常亮，等待下一次触摸
            if (show_led(0) < 0) {
                return 0;
            }
            k_sem_take(&mode_sem, K_FOREVER);
        } 
        else {
            // 模式1/2/3分别使用4/3/2个灯
            int active_led_count = 5 - current_mode;

            if (show_led(led_index) < 0){
                return 0;
            }
            // 等待200 ms后切换下一盏灯。若触摸产生信号，则提前结束等待,回到循环顶部处理新模式。
            ret = k_sem_take(&mode_sem, K_MSEC(FLOW_DELAY_MS));
            if (ret == -EAGAIN) 
            {
                led_index =
                    (led_index + 1) % active_led_count;
            }
        }
    }

    return 0;
}
 */




/* //zephyr GPIO中断回调,按键0触摸一次，打印“button0 pressed!”。
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>
#define button0_NODE DT_NODELABEL(button0)//定义别名
struct gpio_dt_spec button0_struct = GPIO_DT_SPEC_GET(button0_NODE, gpios);//定义GPIO属性
struct gpio_callback gpio_cb_struct={0};//定义回调函数结构体

    //  @brief 回调函数
    //  @param port Device struct for the GPIO device.
    //  @param cb Original struct gpio_callback owning this handler
    //  @param pi    ns Mask of pins that triggers the callback handler
     
    void gpio_callback (const struct device *port,
					struct gpio_callback *cb,
					gpio_port_pins_t pins)             
{
    if(pins==BIT(button0_struct.pin))//判断是否是按键引脚触发的中断
    {
        printk("button0 pressed!\n");
    }
}
int main(void)
{
    // 检查按键硬件外设是否就绪
    if (gpio_is_ready_dt(&button0_struct) == false) 
    {
        printk("button0 is not ready\n");
        // printk——zephyr内核的打印函数(库较小)，printf——C标准库的打印函数
        return 0; // 按键设备异常，直接退出main
    }
    // 配置为输入
    gpio_pin_configure_dt(&button0_struct, GPIO_INPUT);
    //配置为上升沿触发中断
    gpio_pin_interrupt_configure_dt(&button0_struct, GPIO_INT_EDGE_TO_ACTIVE);
    //初始化回调函数
    gpio_init_callback(&gpio_cb_struct, gpio_callback, BIT(button0_struct.pin));
    //注册回调函数
    gpio_add_callback(button0_struct.port, &gpio_cb_struct);
    // 读取一次按键当前电平
    int res = gpio_pin_get_dt(&button0_struct);
    printk("button0 state: %d\n", res);

    return 0;
}   */




/*  //LED0闪烁
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>
//获取设备树里 button0 节点的GPIO信息
#define button0_NODE DT_NODELABEL(button0)  
struct gpio_dt_spec button0_struct=GPIO_DT_SPEC_GET(button0_NODE,gpios);

// 获取设备树里 led0 节点的GPIO信息
#define LED_NODE DT_NODELABEL(led0)
struct gpio_dt_spec led_struct=GPIO_DT_SPEC_GET(LED_NODE,gpios);
int main(void)
{
    // 检查按键硬件外设是否就绪
    if (gpio_is_ready_dt(&button0_struct)==false) 
    {
        printf("button0 is not ready\n");
        return 0; // 按键设备异常，直接退出main
    }
    // 检查LED硬件外设是否就绪
    if (gpio_is_ready_dt(&led_struct)==false) 
    {
        printf("LED is not ready\n");
        return 0; // LED设备异常，直接退出main
    }

    // 配置按键：输入、低电平有效、上拉电阻
    gpio_pin_configure_dt(&button0_struct,GPIO_INPUT|GPIO_ACTIVE_LOW|GPIO_PULL_UP);
    // 配置LED：输出、低电平有效
    gpio_pin_configure_dt(&led_struct,GPIO_OUTPUT|GPIO_ACTIVE_LOW);
    // 读取一次按键当前电平（**只在程序启动瞬间读一次，不在循环内**）
    int res=gpio_pin_get_dt(&button0_struct);

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