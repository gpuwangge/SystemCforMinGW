#include "main.h"
#include "common.h"
#include <systemc.h>

using namespace sc_core;

/*
你运行时应该看到什么

rst_n=0 时：
    Producer/Consumer 都处于复位状态

en=1 后：
    Producer 开始产生数据
    当 valid=1 且 ready=1 时发生真正传输

Consumer 会故意制造 backpressure：
    每隔几拍 ready=0

因此你会看到：
    Producer stall (ready=0)
    Consumer got: X

这就是经典 valid/ready 握手
*/

// ------------------ Producer ------------------
SC_MODULE(Producer) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> en;

    // ready 来自 consumer
    sc_in<bool> ready;

    // 输出
    sc_out<bool> valid;
    sc_out<sc_uint<8>> data;

    sc_uint<8> cnt;

    void run() {
        cnt = 0;

        valid.write(false);
        data.write(0);

        wait(clk.posedge_event());

        while (true) {
            wait(clk.posedge_event());

            if (!rst_n.read()) {
                cnt = 0;

                valid.write(false);
                data.write(0);
            }
            else if (en.read()) {

                // 当前没有 pending 数据
                if (!valid.read()) {
                    cnt = cnt + 1;

                    data.write(cnt);
                    valid.write(true);

                    std::cout << sc_time_stamp()
                              << " Producer send request: "
                              << cnt << std::endl;
                }

                // valid && ready -> handshake success
                else if (valid.read() && ready.read()) {

                    std::cout << sc_time_stamp()
                              << " Producer handshake success: "
                              << data.read()
                              << std::endl;

                    valid.write(false);
                }

                // valid=1 但 ready=0
                else if (valid.read() && !ready.read()) {

                    std::cout << sc_time_stamp()
                              << " Producer stall (ready=0): "
                              << data.read()
                              << std::endl;
                }
            }
        }
    }

    SC_CTOR(Producer) {
        SC_THREAD(run);
    }
};

// ------------------ Consumer ------------------
SC_MODULE(Consumer) {
    sc_in<bool> clk;
    sc_in<bool> rst_n;

    sc_in<bool> valid;
    sc_in<sc_uint<8>> data;

    sc_out<bool> ready;

    int cycle;

    void run() {

        cycle = 0;

        ready.write(false);

        wait(clk.posedge_event());

        while (true) {
            wait(clk.posedge_event());

            cycle++;

            if (!rst_n.read()) {
                ready.write(false);
            }
            else {

                // 制造 backpressure:
                // 每3拍暂停1拍
                if ((cycle % 4) == 0) {
                    ready.write(false);
                } else {
                    ready.write(true);
                }

                // 真正接收数据
                if (valid.read() && ready.read()) {

                    std::cout << sc_time_stamp()
                              << " Consumer got: "
                              << data.read()
                              << std::endl;
                }
            }
        }
    }

    SC_CTOR(Consumer) {
        SC_THREAD(run);
    }
};

// ------------------ Top ------------------
SC_MODULE(Top) {

    sc_clock clk{"clk", 10, SC_NS};

    sc_signal<bool> rst_n{"rst_n"};
    sc_signal<bool> en{"en"};

    // valid-ready channel
    sc_signal<bool> valid{"valid"};
    sc_signal<bool> ready{"ready"};

    sc_signal<sc_uint<8>> data{"data"};

    Producer prod{"prod"};
    Consumer cons{"cons"};

    void stim() {

        rst_n = false;
        en = false;

        // reset 2 cycles
        wait(clk.posedge_event());
        wait(clk.posedge_event());

        rst_n = true;

        // 开始发送
        en = true;

        for (int i = 0; i < 20; i++) {
            wait(clk.posedge_event());
        }

        en = false;

        for (int i = 0; i < 5; i++) {
            wait(clk.posedge_event());
        }

        sc_stop();
    }

    void monitor() {

        while (true) {

            wait(clk.posedge_event());

            std::cout
                << sc_time_stamp()
                << " rst_n=" << rst_n.read()
                << " en=" << en.read()
                << " valid=" << valid.read()
                << " ready=" << ready.read()
                << " data=" << data.read()
                << std::endl;
        }
    }

    SC_CTOR(Top) {

        // Producer
        prod.clk(clk);
        prod.rst_n(rst_n);
        prod.en(en);

        prod.ready(ready);

        prod.valid(valid);
        prod.data(data);

        // Consumer
        cons.clk(clk);
        cons.rst_n(rst_n);

        cons.valid(valid);
        cons.data(data);

        cons.ready(ready);

        SC_THREAD(stim);
        SC_THREAD(monitor);
    }
};

int sc_main(int argc, char* argv[]) {

    Top top{"top"};

    sc_start();

    return 0;
}