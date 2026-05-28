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
这种按名字查对象
## 6 sc_main 是什么？

sc_main 是 SystemC 仿真程序的约定入口函数(simulation entry point)。它的标准签名是：

int sc_main(int argc, char* argv[]);

你在 sc_main 里通常做三件事：

实例化模块(SC_MODULE 的对象)

连接接口/信号、设置 trace、配置参数等

调用 sc_start(...) 启动仿真

由 SystemC 库提供胶水 main()，去调用用户的 sc_main()

SystemC 库中(或某个附加对象文件中)提供真正的 main()

main() 会初始化 SystemC 运行环境，然后调用你的 sc_main()

你也可以自己写一个 main()，在里面调用 sc_main()

## 7 sc_signal和sc_in/sc_out

sc_signal<T>：是 SystemC 里最常用的“信号/连线”(channel)，用来保存一个值并在值变化时通知事件(触发敏感进程)。你可以把它理解成硬件里的 wire/reg(在 SystemC 抽象下的信号线)。

sc_in<T>：则是模块的输入端口(port)。端口本身不存值，它只是一个“插口”，要绑定到某个真正承载数据的东西(最常见就是 sc_signal<T>)。

sc_out<T>：跟以上类似，只不过是输出端口。

port：指的就是 SystemC 模块里的端口对象，也就是那些 sc_in<> / sc_out<> / sc_inout<> 成员。

关系

sc_signal<T>:数据在哪里“存/传播”，像电线，可被读写

sc_in<T>:模块从哪里“读进来”，像针脚

两者通过 bind/连接(port(signal)) 连起来

举例：

SC_MODULE(consumer) {

sc_in<int> a; // 这就是 port(端口对象)

...

};

sc_signal<int> s; // 定义一个 int 类型的 signal

consumer u("u"); // 定义一个 consumer 类型的对象，对象名字叫 u，再给一个层级结构的名字 "u"。consumer 里面定义了一个 port，名字叫 a。

u.a(s); // 由以上定义可知，这一步就是把 u 里面的 a port(sc_in<int>) bind 到 s 上。

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

SC_THREAD(proc);

sensitive << clk.pos(); // 或 sensitive << sig1 << sig2 ...

也可以完全不写 sensitive，然后在线程里 wait(event) / wait(time)。

SC_CTHREAD

必须指定一个时钟沿作为驱动：

SC_CTHREAD(proc, clk.pos());

线程每次 wait() 默认就等 一个时钟周期(下一次指定边沿)

b.reset 支持方式不同

SC_THREAD

reset 要你自己写逻辑(例如检测 rst 信号并处理)，或用 async_reset_signal_is/reset_signal_is(SystemC 2.3+ 支持对 thread 也做 reset 约束，但风格上还是更自由)。

典型写法是线程里显式处理 reset。

SC_CTHREAD

有配套的 reset 声明方式，风格接近硬件寄存器复位：

SC_CTHREAD(proc, clk.pos());

async_reset_signal_is(rst_n, false); // 或 reset_signal_is(...)

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

## 9 sc_signal

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










