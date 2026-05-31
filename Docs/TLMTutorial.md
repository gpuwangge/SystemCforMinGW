# TLM Tutorial
## 1 TLM 阻塞传输流程
```
┌─────────────────────────────────────────────────────────────┐
│                    TLM 阻塞传输流程                          │
└─────────────────────────────────────────────────────────────┘

Producer (Initiator)                        Consumer (Target)
     │                                            │
     │  1. 设置事务参数                            │
     │     (command, address, data)               │
     │                                            │
     │  2. socket->b_transport(trans, delay) ────>│
     │     (阻塞等待，直到目标返回)                 │
     │                                            │ 3. 自动触发回调
     │                                            │    Consumer::b_transport()
     │                                            │
     │                                            │ 4. 处理事务
     │                                            │    (读/写数据)
     │                                            │
     │                                            │ 5. trans.set_response_status(TLM_OK_RESPONSE)
     │                                            │
     │  6. 返回，检查响应状态                      │
     │     if (trans.get_response_status() == OK) │
     └────────────────────────────────────────────┘
```

## 2 TLM Socket和SystemC in/out/signal/fifo的区别
| 维度   | TLM Socket (simple_initiator/target_socket) | sc_in/sc_out + sc_signal | sc_fifo     |
| ---- | ------------------------------------------- | ------------------------ | ----------- |
| 抽象层级 | 事务级（Transaction Level）                      | 寄存器传输级（RTL）              | 寄存器传输级（RTL） |
| 通信内容 | 事务对象（地址 + 命令 + 数据）                          | 单个信号值（bit/logic/int）     | 数据包（C++ 对象） |
| 时序模型 | 松散时序（Loosely Timed）                         | 精确时钟周期（周期精确）             | 周期近似        |
| 仿真速度 | 快（秒级启动 OS）                                  | 慢（周期级仿真）                 | 中等          |
| 方向性  | 发起端（initiator）/目标端（target）                  | 输入（in）/输出（out）           | 生产者/消费者     |
| 主要用途 | SoC 系统级建模、软件早期开发                            | RTL 级硬件验证、门级仿真           | 线程间 FIFO 通信 |

TLM Socket：SoC 系统级建模，像 CPU 访问内存（地址 + 数据）  
sc_in/sc_out：RTL 级硬件验证，像 Verilog 的 wire（时钟边上跳变）  
sc_fifo：线程间 FIFO 队列，像硬件 FIFO 缓冲（先进先出）  

再简单点说：  
TLM = 寄快递（一次寄一个包裹）  
特点：一次送一个完整的包裹，不用关心路上几步、经过几个红绿灯，只要知道"寄到了"就行。所以很快，适合画大地图（SoC 系统建模）。  

sc_in/sc_out = 电线（电线上只有 1 或 0，等时钟敲一下才变）  
特点：超级精确，每一步都算得清清楚楚，但很慢，适合做电路设计（RTL 验证）。  

sc_fifo = 排队（先来的先处理）  
特点：中间有个小箱子先存着，不用等对方马上处理，适合流水线工作。  

