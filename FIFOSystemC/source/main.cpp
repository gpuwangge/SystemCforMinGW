#include "main.h"
#include "common.h"
#include <systemc.h>
using namespace sc_core;

/*
你运行时应该看到什么
rst_n=0⁠ 时 Consumer 不读，fifo_used 通常为 0
⁠en=1⁠ 后 Producer 开始写数据，Consumer 同时读数据并打印 ⁠Consumer got: X⁠
FIFO 深度小 (4)，如果 Consumer 读得慢/Producer 写得快，你会看到 ⁠fifo_used⁠ 上升到 4 后不再增长（因为 ⁠nb_write⁠ 失败，被丢弃）
*/

// ------------------ Producer: counter -> fifo ------------------
SC_MODULE(Producer) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> en;

    sc_fifo_out<sc_uint<8>> out;

    sc_uint<8> cnt;

    void run() {
        cnt = 0;
        wait(clk.posedge_event());

        while (true) {
            wait(clk.posedge_event());

            if (!rst_n.read()) {
            cnt = 0;
            } else if (en.read()) {
                // 递增产生数据
                cnt = cnt + 1;

                // 非阻塞写：fifo满则本周期写不进去
                if (out.nb_write(cnt)) {
                // 写成功
                } else {
                // 写失败（FIFO满），丢弃本次数据或下次再试（这里选择丢弃以简单演示）
                    std::cout << sc_time_stamp() << " Producer dropped(FIFO Full): " << cnt << std::endl;
                }
            }
        }
    }

    SC_CTOR(Producer) { SC_THREAD(run);}
};

// ------------------ Consumer: fifo -> print ------------------
SC_MODULE(Consumer) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;

    sc_fifo_in<sc_uint<8>> in;

    void run() {
        wait(clk.posedge_event());

        while (true) {
            wait(clk.posedge_event());

            if (!rst_n.read()) {
            // 复位期间不读
            } else {
                sc_uint<8> v;
                // 非阻塞读：fifo空则读不到
                if (in.nb_read(v)) {
                    std::cout << sc_time_stamp() << " Consumer got: " << v << std::endl;
                }
            }
        }
    }

    SC_CTOR(Consumer) { SC_THREAD(run);}
};

// ------------------ TB top ------------------
SC_MODULE(Top) {
    sc_clock clk{"clk", 10, SC_NS};
    sc_signal<bool> rst_n{"rst_n"};
    sc_signal<bool> en{"en"};

    // FIFO 深度默认=4 (你可以改成 1/2/8 看效果)
    sc_fifo<sc_uint<8>> fifo{"fifo", 1};

    Producer prod{"prod"};
    Consumer cons{"cons"};

    void stim() {
        rst_n = false;
        en = false;

        // 复位保持2拍
        wait(clk.posedge_event());
        wait(clk.posedge_event());

        rst_n = true;

        // 开使能：开始生产数据
        en = true;
        // 跑 12 拍
        for (int i = 0; i < 12; i++) wait(clk.posedge_event());

        // 暂停生产 5 拍
        en = false;
        for (int i = 0; i < 5; i++) wait(clk.posedge_event());

        // 再生产 6 拍
        en = true;
        for (int i = 0; i < 6; i++) wait(clk.posedge_event());

        sc_stop();
    }

    // 可选：监控 FIFO 当前占用 (num_available) 便于理解
    void monitor_fifo_level() {
        while (true) {
            wait(clk.posedge_event());
            std::cout << sc_time_stamp()
                << " rst_n=" << rst_n.read()
                << " en=" << en.read()
                << " fifo_used=" << fifo.num_available()
                << std::endl;
        }
    }

    SC_CTOR(Top) {
        prod.clk(clk);
        prod.rst_n(rst_n);
        prod.en(en);
        prod.out(fifo);

        cons.clk(clk);
        cons.rst_n(rst_n);
        cons.in(fifo);

        SC_THREAD(stim);
        SC_THREAD(monitor_fifo_level);
    }
};

int sc_main(int argc, char* argv[]) {
    Top top{"top"};
    sc_start();
    return 0;
}


