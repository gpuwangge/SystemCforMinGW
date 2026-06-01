#include "main.h"
#include "common.h"

/*
演示 tlm_fifo 的用法：
- Producer 向 FIFO 里 put pointer to tlm_generic_payload
- Consumer 从 FIFO 里 get pointer
- 使用 tlm_fifo<tlm_generic_payload*> 作为 FIFO 通道

你运行时会看到：
20 ns Producer fifo put: 1
30 ns Producer fifo put: 2
30 ns Consumer fifo get: 2
40 ns Producer fifo put: 3
40 ns Consumer fifo get: 3
50 ns Producer fifo put: 4
50 ns Consumer fifo get: 4
60 ns Consumer fifo get: 4
*/


// ==========================
// Producer：向 FIFO 里 put 事务
// ==========================
SC_MODULE(Producer){
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> en;

    // 使用 tlm_fifo_put_if 接口
    //定义一个名为 fifo_port 的 SystemC 端口，用于向 TLM FIFO 里写数据（put）
    //sc_port = SystemC 端口（port）,用于把模块内部的接口与外部通道连接起来,类似 sc_in / sc_out，但这里是通用的接口端口
    //1 表示这个端口 最多可以连接 1 个接口（binding）。通常写 1 就是表示只连一个 FIFO 通道。你也可以写成 2、16，表示可以连接多个接口（多对一或一对多）
    sc_port<tlm::tlm_fifo_put_if<tlm::tlm_generic_payload*>, 1> fifo_port;

    SC_CTOR(Producer) { SC_THREAD(run); }

    void run(){
        int cnt = 0;

        wait(clk.posedge_event());
        while (true) {
            wait(clk.posedge_event());

            if (!rst_n.read()) cnt = 0;
            
            else if (en.read()) {
                cnt++;

                // 创建事务
                tlm::tlm_generic_payload* trans = new tlm::tlm_generic_payload;
                uint32_t data = cnt;

                trans->set_command(tlm::TLM_WRITE_COMMAND);
                trans->set_address(0x1000);
                trans->set_data_ptr(reinterpret_cast<unsigned char*>(&data));
                trans->set_data_length(sizeof(data));
                trans->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

                // 向 FIFO 里 put（阻塞：FIFO 满时等待）
                fifo_port->put(trans);

                std::cout << sc_time_stamp() << " Producer fifo put: " << data << std::endl;
            }
        }
    }
};


// ==========================
// Consumer：从 FIFO 里 get 事务
// ==========================
SC_MODULE(Consumer){
    sc_in<bool> clk;
    sc_in<bool> rst_n;

    // 使用 tlm_fifo_get_if 接口
    sc_port<tlm::tlm_fifo_get_if<tlm::tlm_generic_payload*>, 1> fifo_port;

    SC_CTOR(Consumer) { SC_THREAD(run); }

    void run(){
        wait(clk.posedge_event());
        while (true) {
            wait(clk.posedge_event());

            if (!rst_n.read()) continue;

            // 非阻塞尝试：看看有没有数据
            tlm::tlm_generic_payload* trans = nullptr;
            if (fifo_port->nb_peek(trans)) {
                // 有数据，从 FIFO 里 get
                trans = fifo_port->get();

                uint32_t data = *reinterpret_cast<uint32_t*>(trans->get_data_ptr());

                std::cout << sc_time_stamp() << " Consumer fifo get: " << data << std::endl;

                // 处理完后可以 delete
                delete trans;
            }
        }
    }
};


// ==========================
// Top 模块：绑定 FIFO
// ==========================
SC_MODULE(Top){
    sc_clock clk{"clk", 10, SC_NS};

    sc_signal<bool> rst_n{"rst_n"};
    sc_signal<bool> en{"en"};

    Producer prod{"prod"};
    Consumer cons{"cons"};

    // FIFO 队长
    tlm::tlm_fifo<tlm::tlm_generic_payload*> fifo{"fifo", 8};

    void stim(){
        rst_n = false;
        en = false;

        wait(clk.posedge_event());
        wait(clk.posedge_event());

        rst_n = true;
        en = true;

        // 发几个事务
        for (int i = 0; i < 4; i++) {
            wait(clk.posedge_event());
        }

        en = false;
        wait(100, SC_NS);

        sc_stop();
    }

    SC_CTOR(Top){
        prod.clk(clk);
        prod.rst_n(rst_n);
        prod.en(en);

        cons.clk(clk);
        cons.rst_n(rst_n);

        // 绑定 producer 的 put 端口到 FIFO
        prod.fifo_port(fifo);
        // 绑定 consumer 的 get 端口到 FIFO
        cons.fifo_port(fifo);

        SC_THREAD(stim);
    }
};


int sc_main(int argc, char* argv[]){
    Top top("top");
    sc_start();
    return 0;
}