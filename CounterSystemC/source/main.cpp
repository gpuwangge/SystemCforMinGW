#include "main.h"
#include "common.h"
#include <systemc.h>

using namespace sc_core;

//带时钟的计数器模块
// ------------------ DUT: Counter ------------------
SC_MODULE(Counter) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;  // active-low reset
    sc_in<bool> en;
    sc_out<sc_uint<8>> q;

    void run() {
        q.write(0);
        wait(); // 等待第一个时钟沿
        while (true) {
            if (!rst_n.read()) {
                q.write(0);
            } else if (en.read()) {
                q.write(q.read() + 1);
            }
            wait();
        }
    }
    SC_CTOR(Counter) {
        SC_CTHREAD(run, clk.pos());
        //reset_signal_is(...) 是 SystemC 标准库 (IEEE 1666 / Accellera SystemC) 里定义的标准 API, 不是用户自定义函数, 也不是某家厂商私有扩展
        reset_signal_is(rst_n, false);
    }
};

// ------------------ Testbench ------------------
SC_MODULE(TB) {
    sc_clock clk{"clk", 10, SC_NS};
    //rst: 一般表示 高有效 (rst=1 时复位)
    //rst_n 或 rst_b: 表示 低有效 (rst_n=0 时复位)
    sc_signal<bool> rst_n{"rst_n"}; //reset 的缩写，中文常叫复位。复位信号用来把电路/模块的内部状态强制回到一个已知初始状态。
    //en=1: 允许更新 (寄存器装载新值、计数器递增、状态机跳转)
    //en=0: 禁止更新 (寄存器保持原值、计数器停住、状态机不变)
    sc_signal<bool> en{"en"}; //en 通常是 enable 的缩写，中文常叫使能。
    //q 通常代表某个时序状态量的当前值，也就是“被寄存起来”的值
    sc_signal<sc_uint<8>> q{"q"}; //这里表示 DUT (Counter 模块) 的8位输出端口，输出当前计数结果。
    Counter dut{"dut"};
    // 等 N 个上升沿
    void wait_posedges(int n) {
        for (int i = 0; i < n; i++) wait(clk.posedge_event());
    }

    //stimulus (激励) 函数，作用是：在仿真过程中主动产生输入信号的变化，也就是“扮演外部环境/软件/上游模块”，去驱动 DUT (被测模块) 的输入端口，比如 rst_
    void stim() {
        // 初值：拉低复位，关闭使能
        rst_n.write(false);
        en.write(false);
        // 复位保持 2 个周期
        wait_posedges(2);
        // 释放复位
        rst_n.write(true);
        // 开使能数 5 个周期
        en.write(true);
        wait_posedges(5);
        // 关使能保持 3 个周期
        en.write(false);
        wait_posedges(3);
        // 再开使能 4 个周期
        en.write(true);
        wait_posedges(4);
        sc_stop();
    }

    void monitor() {
        while (true) {
            wait(clk.posedge_event());
            wait(SC_ZERO_TIME); // 让DUT先把q更新完
            std::cout << sc_time_stamp()
                << " rst_n=" << rst_n.read()
                << " en=" << en.read()
                << " q=" << q.read()
                << std::endl;
        }
    }
    SC_CTOR(TB) {
        dut.clk(clk);
        dut.rst_n(rst_n);
        dut.en(en);
        dut.q(q);
        SC_THREAD(stim);
        SC_THREAD(monitor);
    }
};

int sc_main(int argc, char* argv[]) {
    TB tb{"tb"};
    sc_start();
    return 0;
}

