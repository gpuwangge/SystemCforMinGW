# Tutorial
## 1 头文件与命名空间
```
#include <systemc>
using namespace sc_core;
```
## 2 定义一个 SystemC 模块（硬件模块的“类”）
```
SC_MODULE(Hello) {
SC_CTOR(Hello) { SC_THREAD(run); }
  ...
};
```
```
SC_MODULE(Hello)
```
这是一个宏，等价于定义一个继承自 sc_module 的 C++ 类，名字叫 Hello。
在 SystemC 里，模块 (module) 就是建模的基本单元，类似 Verilog 里的 module。
这行会展开成类似：
```
struct Hello : sc_core::sc_module { ... };
```
```
SC_CTOR(Hello)
```
这是模块构造函数的宏，用于注册进程、初始化内部成员等。模块实例化时会执行这里的代码。  
名字由来：  
ctor -> constructor(构造函数)  
dtor -> destructor(析构函数)  
你也可以不用 SC_CTOR，直接手写：  
```
SC_MODULE(Hello) {
  Hello(sc_core::sc_module_name name) : sc_core::sc_module(name) {
    SC_THREAD(run);
  }
  void run() {}
};
```
两种都可以。SC_CTOR 的优势是：  
更短、更符合 SystemC 社区的常见写法  
不容易写错构造函数签名（SystemC 对模块构造签名有约定）  
## 3 注册一个线程进程(process)
```
SC_CTOR(Hello) { SC_THREAD(run); }
```
```
SC_THREAD(run);
```
告诉 SystemC 内核：在仿真开始后，把成员函数 run() 当作一个“线程进程”来执行。  
SystemC 常见进程类型：  
SC_METHOD：不允许在函数里 wait()，一次触发跑至结束（适合组合逻辑/事件触发回调）。  
SC_THREAD：允许 wait()，可写成“会暂停/恢复”的行为（适合时序流程、协议、测试激励）。  
这里用 SC_THREAD 是因为 run() 里面要 wait(10, SC_NS)。  

systemc在实现的时候也是在一个进程里建立两个线程吗？比如：
```
SC_THREAD(stim);
SC_THREAD(monitor_fifo_level);
```
答案：不是。SystemC 里的 SC_THREAD 不是操作系统线程，实现上通常也不是“一个进程里开两个 pthread/std::thread”。它们是由 SystemC 内核管理的仿真进程（simulation process / coroutine），在同一个 OS 线程（甚至同一个进程）里通过调度器“轮流执行”。  
更准确地说：  
1) SC_THREAD/SC_METHOD 属于“仿真进程”，由内核调度  
你写两个 SC_THREAD(stim); SC_THREAD(monitor);  
内核会创建两个仿真进程对象  
每个仿真进程运行到 wait(...) 时主动让出控制权  
调度器在事件（如 clk.posedge_event()）触发时，把对应进程恢复执行  
这是一种协作式调度（cooperative）：进程不 wait，就会一直跑、阻塞整个仿真。  
2) 默认情况下：单 OS 线程、单进程、时间片不并行  
通常 SystemC 仿真是：  
一个操作系统线程跑 SystemC kernel 的主循环  
所有 SC_THREAD/SC_METHOD 都在这个 OS 线程里被“交错执行”   
所以它们是“仿真并发”，不是 CPU 并行  
（当然也在同一个 OS 进程里。）  
3) SC_METHOD 和 SC_THREAD 的实现机制区别  
SC_METHOD：每次触发就从函数开头执行到结尾，不能 wait，不需要保存执行上下文  
SC_THREAD：可以 wait，所以内核需要保存/恢复上下文（通常用协程/纤程一类机制实现）  
但两者都不是 OS 级线程。  
4) 什么时候才会用到多 OS 线程？  
标准 SystemC 语义本身不要求多线程。少数场景会引入多线程，例如：  
仿真器/厂商内核做并行加速（不改变语义的优化）  
你自己在 SystemC 外围用 std::thread 跑别的工作（日志、socket、GUI），但这要非常小心线程安全（SystemC 内核通常不是随便多线程调用的）

结论：在一个模块里写两个 SC_THREAD，实现上通常仍是同一个 OS 线程里由 SystemC 内核调度的两个“仿真线程/协程”，不是两个真正的系统线程。  
## 4 线程函数 run():打印时间、等待、再打印、停止
```
void run() {
  std::cout << "Hello SystemC @ " << sc_time_stamp() << "\n";
  wait(10, SC_NS);
  std::cout << "After 10ns @ " << sc_time_stamp() << "\n";
  sc_stop();
}
```
```
sc_time_stamp()
```
返回“当前仿真时间”，一开始通常是 0(例如 0 s)。  
```
wait(10, SC_NS);
```
让这个线程进程暂停 10 纳秒。注意这里不是让 Windows 睡眠 10ns，而是让仿真时间推进 10ns。  
再次打印，此时 sc_time_stamp() 应该是 10 ns。  
```
sc_stop();
```
请求停止仿真(结束 sc_start())。
## 5 sc_main:SystemC 程序入口
```
int sc_main(int, char*[]) {
Hello h("h");
  sc_start();
  return 0;
}
```
SystemC 的入口函数叫 sc_main(不是标准 C++ 的 main)。SystemC 库会提供 glue code 来从 main 进入 sc_main(或由链接方式决定)。  
```
Hello h("h");
```
实例化一个模块，名字叫 "h"。这个名字在 SystemC 的对象层次结构里很重要，用于层级命名/调试/trace。  
```
sc_start();
```
启动仿真内核。它会调度并运行你注册的进程(这里就是 run())。  
直到 sc_stop() 被调用或仿真结束条件达成，sc_start() 才会返回。  
```
Hello h("h");
```
左边的 h(不带引号)是 C++ 变量名:在 C++ 代码里你用它来引用这个对象。  
右边的 "h"(带引号)是传给 SystemC 的 实例名(instance name)，用于 SystemC 内部建立“对象层级/命名空间”。  
SystemC 会把这个实例登记为一个对象，名字叫 "h"。后续很多机制都用这个名字，例如：  
打印层级名称(h、top.h 这种)  
报错/警告信息里定位是哪一个模块实例  
VCD/FSDB 等波形 trace 的信号命名(通常会带层级名)  
```
sc_find_object("h")
```
这种按名字查对象。
## 6 sc_main 是什么
sc_main 是 SystemC 仿真程序的约定入口函数(simulation entry point)。它的标准签名是：
```
int sc_main(int argc, char* argv[]);
```
你在 sc_main 里通常做三件事：  
实例化模块(SC_MODULE 的对象)  
连接接口/信号、设置 trace、配置参数等  
调用 sc_start(...) 启动仿真  
由 SystemC 库提供胶水 main()，去调用用户的 sc_main()  
SystemC 库中(或某个附加对象文件中)提供真正的 main()  
main() 会初始化 SystemC 运行环境，然后调用你的 sc_main()  
你也可以自己写一个 main()，在里面调用 sc_main()  
## 7 sc_signal和sc_in/sc_out
```
sc_signal<T>
```
是 SystemC 里最常用的“信号/连线”(channel)，用来保存一个值并在值变化时通知事件(触发敏感进程)。你可以把它理解成硬件里的 wire/reg(在 SystemC 抽象下的信号线)。  
```
sc_in<T>
```
则是模块的输入端口(port)。端口本身不存值，它只是一个“插口”，要绑定到某个真正承载数据的东西(最常见就是 sc_signal<T>)。  
```
sc_out<T>
```
跟以上类似，只不过是输出端口。
```
port
```
指的就是 SystemC 模块里的端口对象，也就是那些 sc_in<> / sc_out<> / sc_inout<> 成员。  
关系  
sc_signal<T>:数据在哪里“存/传播”，像电线，可被读写  
sc_in<T>:模块从哪里“读进来”，像针脚  
两者通过 bind/连接(port(signal)) 连起来  
举例：  
```
SC_MODULE(consumer) {
  sc_in<int> a; // 这就是 port(端口对象)
  ...
};
```
```
sc_signal<int> s;
```
定义一个 int 类型的 signal
```
consumer u("u");
```
定义一个 consumer 类型的对象，对象名字叫 u，再给一个层级结构的名字 "u"。consumer 里面定义了一个 port，名字叫 a。  
```
u.a(s);
```
由以上定义可知，这一步就是把 u 里面的 a port(sc_in<int>) bind 到 s 上。  
## 8 SC_THREAD和SC_CTHREAD的区别
SC_THREAD 和 SC_CTHREAD 都是用来在 SystemC 里创建“线程进程”(会按时间推进，可以 wait())的，但定位不同：  
SC_THREAD:通用线程(最常用、最灵活)  
SC_CTHREAD:时钟驱动线程(专用于同步时序逻辑，更“硬件化”，更偏 RTL 风格/综合友好)  
SC_CTHREAD 里多出来的这个 C 通常理解为 Clocked(时钟驱动的) / Cycle(按周期推进的)。  
也就是说  
SC_THREAD:普通线程进程(general thread)  
SC_CTHREAD:clocked thread / cycle thread —— 专用来建模“同步时序逻辑”的线程  
必须指定时钟沿:SC_CTHREAD(proc, clk.pos());  
wait() 通常表示“等一个时钟周期/下一拍”  
更接近 RTL 写法(类似 always_ff @(posedge clk))  
它不是 C++ 的 “C”，而是 SystemC 里为了强调“这个线程是时钟同步的”而加的前缀。  
a.触发/调度方式不同  
SC_THREAD  
用敏感列表或内部 wait() 控制何时运行  
常见写法：  
```
SC_THREAD(proc);
sensitive << clk.pos(); // 或 sensitive << sig1 << sig2 ...
```
也可以完全不写 sensitive，然后在线程里 wait(event) / wait(time)。  
```
SC_CTHREAD
```
必须指定一个时钟沿作为驱动：  
```
SC_CTHREAD(proc, clk.pos());
```
线程每次 wait() 默认就等 一个时钟周期(下一次指定边沿)  
b.reset 支持方式不同
SC_THREAD  
reset 要你自己写逻辑(例如检测 rst 信号并处理)，或用 async_reset_signal_is/reset_signal_is(SystemC 2.3+ 支持对 thread 也做 reset 约束，但风格上还是更自由)。  
典型写法是线程里显式处理 reset。  
SC_CTHREAD  
有配套的 reset 声明方式，风格接近硬件寄存器复位：  
```
SC_CTHREAD(proc, clk.pos());
async_reset_signal_is(rst_n, false); // 或 reset_signal_is(...)
```
reset 触发时会把线程“拉回”到开头(从 reset 状态重新开始执行)，更符合时序电路建模。  
c.wait() 语义与使用习惯不同  
SC_THREAD  
wait() 可以：  
wait();:等敏感列表事件(如果有)  
wait(10, SC_NS);:等时间  
wait(ev);:等事件  
wait(ev1 | ev2);:等多个事件之一  
适合协议、软件式状态机、复杂握手、任意事件驱动。  
SC_CTHREAD  
主要用 wait(); 表示 等一个时钟(下一拍)  
不鼓励(通常也不适合)用“任意事件 wait”，它的核心就是“每拍推进一次”的同步流程。  
SC_CTHREAD 更接近 Verilog 的 always_ff @(posedge clk) + reset，更适合写可综合的时序逻辑(取决于你的综合工具是否支持 SystemC 综合)。  
SC_THREAD 偏通用仿真建模，写起来像并发程序，不一定综合友好。  
你在写 同步时序电路(寄存器、流水线、同步 FSM):优先 SC_CTHREAD  
你在写 复杂时序/协议/测试平台(testbench)、需要等待任意事件/时间:用 SC_THREAD  
## 9 sc_clock
sc_clock 在用法上很像信号(你可以把它连到 sc_in<bool> 上、也能放进 sensitive << clk.pos())，但严格说它不是 sc_signal。
更准确地讲：  
sc_signal<T>:通用“信号通道”(channel)，值由某个写者写入。  
sc_clock:一种专用通道/原语通道(primitive channel)，内部自己按照你设定的周期自动翻转，生成时钟波形。  
不同点(关键差异)  
a.谁来驱动  
sc_signal<bool>:需要你自己(通过 sc_out 或 write())去驱动翻转  
sc_clock:SystemC 内核自动驱动，按周期产生 0/1  
b.用途  
sc_signal:传递任意数据/控制信号  
sc_clock:专内建模时钟(period、duty cycle、起始相位等)  
c.类型与接口  
sc_clock 的值类型是 bool(本质是一个布尔时钟源)  
它实现的是时钟信号需要的事件(posedge/negedge)  
sc_clock 和 sc_signal 一样都是 channel，模块要用到它通常也是通过 port 绑定(bind)进去的。  
最常见:把 sc_clock 绑定到模块的 sc_in<bool> clk  
举例：
```
sc_clock clk{"clk", 10, SC_NS};
```
在 SystemC 里创建一个时钟信号（sc_clock）。这行用的是 sc_clock 的一个常用构造函数，参数含义如下：
| 参数      | 例子里的值 | 含义 |
|-----------|------------|------|
| name      | `"clk"`    | 时钟对象的名字（用于波形/打印/层级名） |
| period    | `10`       | 时钟周期的数值 |
| time_unit | `SC_NS`    | 上面 period 的时间单位（纳秒） |

> 所以这句的整体含义是：创建一个名为 `clk` 的时钟，周期为 `10 ns`。

推导出来的时钟特性
在未指定其它参数时（占空比、起始时间、初值等使用默认值）：  
- 周期 T = 10 ns  
- 默认占空比通常是 50%（实现上等价于高电平 5 ns、低电平 5 ns）  
- 频率 f = 1/T = 1 / 10ns = 100 MHz  

常见扩展写法（了解即可）  
如果你看到更长的构造形式，可能是这样：  
```
sc_clock clk("clk", 10, SC_NS, 0.5, 0, SC_NS, true);
```
含义依次是：  
- 名字  
- 周期  
- 周期单位  
- 占空比（0.5=50%）  
- 起始相位/延迟（这里是 0）  
- 延迟单位  
- 初始电平（true 表示一开始为 1）

之前的简写版就是用了其中最常用的前三个参数，其余用默认值。  
## 10 sc_fifo
sc_signal<T> 和 sc_fifo_out<T> 的核心区别是：一个是“硬件线网/寄存器式信号（单值）”，一个是“带队列语义的通道接口（多值、有缓存）”。  
数据语义:前者只有一个当前值（被覆盖）；后者是队列，可以存多个元素（有深度）。  
写入行为	write(x)： 前者把值更新为 x（旧值被覆盖）；后者write(x) / nb_write(x) 把元素入队（FIFO 满会阻塞或失败）。  
读取行为	read()： 前者读当前值（不会“消耗”）；后者read() / nb_read(x) 出队（FIFO 空会阻塞或失败）。  
同步/时序：前者常用来描述 RTL 信号，更新在 delta cycle 生效；常配合时钟进程；后者更像“消息/事务传输”，天然有生产者-消费者语义。  
连接关系：前者通常 1 个 driver（单写者）+ 多读者（可多个观察者）；FIFO 通常 1 写者 + 1 读者（典型用法；也可扩展但要小心仲裁）。  
适用场景：前者寄存器输出、组合信号、握手信号（valid/ready）、状态等；后者用于流式数据、任务队列、模块解耦、速率不匹配的缓冲。  
直观例子  
sc_signal：只保留“最后一次写”的值  
```
sc_signal<int> s;
s.write(1);
s.write(2);
// 你再 read，只会得到 2，1 被覆盖掉了
```
sc_fifo：会把每次写入都排队保存  
```
sc_fifo<int> f(4);
f.write(1);
f.write(2);
// 读两次会依次得到 1、2（先进先出）
```
sc_fifo_out<T> 不是 FIFO 本体，它是一个端口类型（port），表示“这个模块有一个 FIFO 输出口”。它必须连接到某个 sc_fifo<T>（或兼容 sc_fifo 接口的 channel）  
```
sc_fifo<int> fifo(4);

Producer p("p");
p.out(fifo);          // out 是 sc_fifo_out<int>

Consumer c("c");
c.in(fifo);           // in 是 sc_fifo_in<int>
```
你在写偏 RTL 的模块（寄存器、FSM、valid/ready 接口）：优先 sc_signal  
你想做模块间数据流、缓冲、解耦（生产快/消费慢）：用 sc_fifo + sc_fifo_in/out  
## 11 wait
在 SystemC 里，wait(...) 的作用是让当前这个进程挂起（阻塞），直到指定的事件/时间发生，发生后再从 wait 这一行的下一句继续执行。它相当于在写“时序逻辑”的感觉：代码不会一口气跑完，而是跟着仿真时间一步步走。
结合这段代码来看：  
```
wait(clk.posedge_event());
```
含义是：等待 clk 的上升沿事件发生（posedge，0→1 的那一刻），也就是等到下一个时钟上升沿才继续往下执行。  
为什么要这样做？
- 常见写法是让线程对齐到时钟边沿开始工作，避免在仿真时间 0 就“抢跑”执行一堆逻辑。  
- 等到第一个 posedge 后，逻辑行为更像硬件里的“从某个时钟开始工作”。

类似写法还有：wait(clk.negedge_event()) 等待下降沿。  
在while loop里面写wait相当于做一个节拍器。  

