#include "main.h"
#include "common.h"

/*
SystemC 是语言/仿真框架, TLM 是建立在 SystemC 上的一套建模方法和类库.
TLM 一定依赖 SystemC, 但 SystemC 不一定依赖 TLM. 你完全可以用 SystemC 来写 RTL 风格的代码, 就像 ValidReadySystemC 和 CounterSystemC 那样.
TLM(Transaction Level Modeling): 不要关心每一拍发生什么,只关心一次访问发生什么. 仿真速度提升.
不过 TLM 不是一种总线协议，而是一种建模抽象层次。它更像：RTL级建模。
SystemC 本质上是一个 C++ 库。提供：
    module
    process
    event
    clock
    signal
一句话总结：
SystemC 是仿真框架；TLM 是建立在 SystemC 上的高层建模方法。
SystemC 关注“信号和周期（cycle）”，TLM 关注“事务（transaction）和延迟（delay）”。
RTL 风格的 SystemC 用 sc_signal、clk、valid/ready；TLM 风格则用 socket、payload、b_transport() 来描述模块之间的通信。
*/

/* Results you should see when running this program:
20 ns Producer send transaction: 1
20 ns Consumer got transaction: 1
50 ns Producer transaction done
50 ns Producer send transaction: 2
50 ns Consumer got transaction: 2
80 ns Producer transaction done
80 ns Producer send transaction: 3
80 ns Consumer got transaction: 3
110 ns Producer transaction done
110 ns Producer send transaction: 4
110 ns Consumer got transaction: 4
140 ns Producer transaction done
*/

SC_MODULE(Producer)
{
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> en;

    //Producer 这个模块身上装了一个“出货口”，它负责把事务发出去；对面如果是 Consumer 或 Target，通常会有对应的 simple_target_socket 来接收。
    tlm_utils::simple_initiator_socket<Producer> socket;

    int cnt;

    void run(){
        cnt = 0;
        wait(clk.posedge_event());
        while (true){
            wait(clk.posedge_event());
            if (!rst_n.read()) cnt = 0;
            
            else if (en.read()){
                cnt++;
                uint32_t data = cnt;
                tlm::tlm_generic_payload trans;
                sc_time delay = SC_ZERO_TIME;

                trans.set_command(tlm::TLM_WRITE_COMMAND);
                trans.set_address(0x1000);
                trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
                trans.set_data_length(sizeof(data));

                std::cout << sc_time_stamp() << " Producer send transaction: " << data << std::endl;
                /*
                调用后阻塞：当前线程会一直等待，直到目标模块完成该事务并返回响应
                时序注解：delay 表示事务在 sc_time_stamp() + delay 时刻生效；目标可以修改 delay 累加延迟
                一次完成：在 loosely-timed 风格中，每个 b_transport 调用对应一个完整的事务（从发出到收到响应）
                */
                socket->b_transport(trans, delay);

                wait(delay);

                std::cout << sc_time_stamp() << " Producer transaction done"<< std::endl;
            }
        }
    }

    SC_CTOR(Producer) { SC_THREAD(run); }
    
};

SC_MODULE(Consumer){
    //这行代码和之前的 simple_initiator_socket<Producer> 正好是一对：一个发、一个收，完成 TLM 通信 。
    tlm_utils::simple_target_socket<Consumer> socket;

    SC_CTOR(Consumer){
        //在 target socket 上注册一个 b_transport 回调函数，当 initiator 通过 socket 调用 b_transport() 时，SystemC 会自动调用你指定的这个成员函数来处理事务 。
        socket.register_b_transport(this, &Consumer::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay){
        uint32_t data = *reinterpret_cast<uint32_t*>(trans.get_data_ptr());

        std::cout << sc_time_stamp() << " Consumer got transaction: " << data << std::endl;

        // 模拟处理时间
        delay += sc_time(30, SC_NS);

        //在事务处理完成后，设置响应状态为「成功」，告诉 initiator 这个事务已经正常完成。
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

SC_MODULE(Top){
    sc_clock clk{"clk", 10, SC_NS};

    sc_signal<bool> rst_n{"rst_n"};
    sc_signal<bool> en{"en"};

    Producer prod{"prod"};
    Consumer cons{"cons"};

    void stim(){
        rst_n = false;
        en = false;

        wait(clk.posedge_event());
        wait(clk.posedge_event());

        rst_n = true;
        en = true;

        for (int i = 0; i < 10; i++) wait(clk.posedge_event());
        
        en = false;
        wait(100, SC_NS);

        sc_stop();
    }

    SC_CTOR(Top){
        prod.clk(clk);
        prod.rst_n(rst_n);
        prod.en(en);

        prod.socket.bind(cons.socket);

        SC_THREAD(stim);
    }
};

int sc_main(int argc, char* argv[]){
    Top top("top");
    sc_start();

    return 0;
}
