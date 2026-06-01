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

速度大概排序：
| 速度     | 机制                         |
| ------ | -------------------------- |
| 最快     | TLM socket / b_transport   |
| 次快     | tlm_fifo                   |
| 再慢     | sc_fifo                    |
| 最慢（通常） | sc_in / sc_out + sc_signal |

为什么 TLM 最快: TLM 传的是事务，不是一拍一拍的信号变化，所以一次调用就能描述“读/写一次地址”的完整动作。它通常只需要处理一次函数调用和少量延迟注解，不会像信号级模型那样频繁触发很多事件。  
为什么 tlm_fifo 比 sc_fifo 更适合 TLM: tlm_fifo 是专门为 TLM 事务缓冲设计的，和 TLM socket 的数据类型更匹配，通常不用再把事务“拆成普通对象再塞进通用 FIFO”。因为它更贴合事务流，少一些适配层，所以在 TLM 场景里通常比 sc_fifo 更顺手、更高效。  
为什么 sc_fifo 比 TLM 慢：sc_fifo 处理的是普通数据对象，每次读写都要经过 SystemC 的 FIFO 同步机制，涉及阻塞/唤醒和事件调度。它比 TLM 细，但没有 sc_signal 那么“逐拍”地反映硬件时序，所以一般性能居中。
为什么 sc_in/sc_out + sc_signal 通常最慢：sc_in/sc_out 本身只是端口，真正的传输靠 sc_signal，而 sc_signal 属于信号级建模，每次变化都要走事件和 delta cycle 机制。如果一个模块里有很多信号、很多进程、很多时钟沿，仿真器要处理的调度就会非常多，所以通常最慢。  




## 3 TLM Payload
Payload（通用载荷） 是 TLM-2.0 中标准化的事务对象，用来在 Initiator 和 Target 之间传递读写请求 。  
简单说：Payload = TLM 里的「快递单」，上面写满了这次「送货」的所有信息 。  
```
tlm::tlm_generic_payload trans;
```
这个事务对象包含以下核心字段 ：  

| 字段              | 类型                  | 说明           | 示例                                   |
| --------------- | ------------------- | ------------ | ------------------------------------ |
| command         | tlm_command         | 读还是写         | TLM_READ_COMMAND / TLM_WRITE_COMMAND |
| address         | unsigned long long  | 目标地址         | 0x1000                               |
| data_ptr        | unsigned char*      | 数据指针         | 指向数据缓冲区                              |
| data_length     | unsigned int        | 数据长度（字节）     | 4（4 字节）                              |
| byte_enable     | unsigned int*       | 字节使能（哪些字节有效） | 可选                                   |
| response_status | tlm_response_status | 响应状态         | TLM_OK_RESPONSE / TLM_ERROR_RESPONSE |
| streaming_width | unsigned int        | 流宽度（连续传输）    | 可选                                   |
| DMI hint        | -                   | 直接内存访问提示     | 可选                                   |
| extensions      | -                   | 用户自定义扩展      | 可添加额外信息 verificationacademy+1        |

tlm_generic_payload = TLM 里的「标准化快递单」，封装了地址、命令、数据、响应，Initiator 和 Target 用它通信 。  

## 4 TLM Payload扩展
Payload 扩展是什么？  
1. 通俗理解  
想象你要寄快递：  

| 快递单字段    | Payload 扩展类比                |
| -------- | --------------------------- |
| 地址（必填）   | trans.set_address(0x1000)   |
| 商品（必填）   | trans.set_data_ptr(data)    |
| 额外备注（可选） | extension->cache_hit = true |

Payload 扩展 = 在标准快递单上额外加一栏「备注」，用来存放你自己的信息 。  

2. 为什么要用扩展？  
TLM 标准字段太少，不够用  
tlm_generic_payload 只包含最基本的字段：  

| 标准字段            | 用途  | 局限             |
| --------------- | --- | -------------- |
| command         | 读/写 | 不知道是 AXI 哪个 ID |
| address         | 地址  | 不知道是哪个 CPU 来的  |
| data_ptr        | 数据  | 不知道缓存是否命中      |
| response_status | 响应  | 不知道性能开销        |

但现实系统中还有很多信息需要传递：  

| 场景       | 需要额外什么信息                            |
| -------- | ----------------------------------- |
| AXI 总线   | axi_id、burst_type、lock、protection   |
| 缓存系统     | cache_hit、cache_level、prefetch_hint |
| 多 CPU 系统 | cpu_id、thread_id、exception_level    |
| 安全系统     | is_secure、privileged_mode           |
| 性能分析     | timestamp、latency、cycle_count       |

总结

| 维度    | 回答                                     |
| ----- | -------------------------------------- |
| 是什么   | Payload 扩展 = 在标准事务对象上额外加一栏「自定义字段」      |
| 有什么用  | 传递标准字段装不下的信息（AXI ID、缓存信息、CPU ID、性能数据等） |
| 什么时候用 | 当标准字段不够用时，就加扩展 gitcode.csdn            |

标准 Payload = 快递单上的「地址 + 商品 + 收件人」（必填）  
Payload 扩展 = 快递单上的「备注栏」（可选，比如「冰箱里放冷冻」「不要放门口」）  

## 5 TLM FIFO
TLM FIFO 是一个先进先出（FIFO）的缓冲区，用于在 TLM 模块之间传递事务（Transaction），而不是直接点对点通信。它的主要作用是：  
- 解耦发送者和接收者：producer 和 consumer 不需要同时工作，FIFO 中间缓冲  
- 防止数据丢失：当 producer 比 consumer 快时，FIFO 可以暂存事务  
- 提供标准接口：统一的 put() / get() 接口，方便连接  

1. 创建 FIFO 通道
```
// FIFO 大小为 8，存储事务指针
tlm::tlm_fifo<tlm::tlm_generic_payload*> fifo("fifo", 8);
```

2. Producer 向 FIFO 写入（put）
```
// FIFO 端口声明
sc_port<tlm::tlm_fifo_put_if<tlm::tlm_generic_payload*>, 1> fifo_port;

// 写入事务
fifo_port->put(trans);  // FIFO 满时阻塞等待 [web:124]
```

3. Consumer 从 FIFO 读取（get）
```
// FIFO 端口声明
sc_port<tlm::tlm_fifo_get_if<tlm::tlm_generic_payload*>, 1> fifo_port;

// 读取事务
if (fifo_port->nb_peek(trans)) {  // 非阻塞查看是否有数据
    trans = fifo_port->get();      // 从 FIFO 取出事务
    // 处理事务
    delete trans;
}
```

4. 连接
```
producer.fifo_port(fifo);  // Producer 的 put 端口连 FIFO
consumer.fifo_port(fifo);  // Consumer 的 get 端口连 FIFO
```

典型应用场景:
| 场景         | 说明                                    |
| ---------- | ------------------------------------- |
| CPU 与内存通信  | 读写请求队列，解耦 CPU 和内存模型 csdn              |
| 多 Agent 通信 | 多个 Agent 之间通过 TLM FIFO 传递数据，协调测试 csdn |
| AXI 总线建模   | 读写响应 FIFO，处理乱序事务 wenku.csdn           |

TLM FIFO = 先进先出的事务缓冲区，Producer 用 put() 往里塞事务，Consumer 用 get() 往外取事务，中间自动缓冲，解耦发送和接收 。  


