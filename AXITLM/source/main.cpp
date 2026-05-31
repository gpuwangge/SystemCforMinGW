#include "main.h"
#include "common.h"
#include <iomanip>

/*
这个例子演示：
1. 仍然使用 tlm::tlm_generic_payload 作为标准事务对象
2. 通过 payload extension 给事务附加 AXI 风格的边带信息
3. Producer 发送时设置 extension
4. Consumer 接收时读取 extension

你会看到：
- generic payload 负责通用字段：command / address / data / response
- AxiExtension 负责协议特有字段：axi_id / burst_len / burst_size / prot

Result:
20 ns Producer send transaction: data=1 addr=0x1004 axi_id=1 burst_len=0 burst_size=2 prot=2
20 ns Consumer got transaction: data=1 addr=0x1004 axi_id=1 burst_len=0 burst_size=2 prot=2 is_write=1
50 ns Producer transaction done
50 ns Producer send transaction: data=2 addr=0x1008 axi_id=2 burst_len=0 burst_size=2 prot=2
50 ns Consumer got transaction: data=2 addr=0x1008 axi_id=2 burst_len=0 burst_size=2 prot=2 is_write=1
80 ns Producer transaction done
*/


// ==========================
// 自定义 AXI 扩展
// ==========================
// TLM payload extension 必须继承 tlm_extension<T>
// 并实现 clone() 和 copy_from()
struct AxiExtension : tlm::tlm_extension<AxiExtension>
{
    uint32_t axi_id;
    uint32_t burst_len;   // AXI len，表示 burst 长度字段
    uint32_t burst_size;  // 每 beat 字节数的 log2 或简化值
    uint32_t prot;        // 保护属性，示例里仅做演示
    bool     is_write;

    AxiExtension()
    : axi_id(0)
    , burst_len(0)
    , burst_size(0)
    , prot(0)
    , is_write(true)
    {}

    virtual tlm::tlm_extension_base* clone() const override
    {
        AxiExtension* ext = new AxiExtension;
        ext->axi_id     = axi_id;
        ext->burst_len  = burst_len;
        ext->burst_size = burst_size;
        ext->prot       = prot;
        ext->is_write   = is_write;
        return ext;
    }

    virtual void copy_from(const tlm::tlm_extension_base& other) override
    {
        const AxiExtension& ext = static_cast<const AxiExtension&>(other);
        axi_id     = ext.axi_id;
        burst_len  = ext.burst_len;
        burst_size = ext.burst_size;
        prot       = ext.prot;
        is_write   = ext.is_write;
    }
};


SC_MODULE(Producer)
{
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> en;

    tlm_utils::simple_initiator_socket<Producer> socket;

    int cnt;

    void run(){
        cnt = 0;
        wait(clk.posedge_event());

        while (true){
            wait(clk.posedge_event());

            if (!rst_n.read()) {
                cnt = 0;
            }
            else if (en.read()){
                cnt++;

                uint32_t data = cnt;
                tlm::tlm_generic_payload trans;
                sc_time delay = SC_ZERO_TIME;

                // --------------------------
                // generic payload 标准字段
                // --------------------------
                trans.set_command(tlm::TLM_WRITE_COMMAND);
                trans.set_address(0x1000 + cnt * 4);
                trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
                trans.set_data_length(sizeof(data));
                trans.set_streaming_width(sizeof(data));
                trans.set_byte_enable_ptr(0);
                trans.set_dmi_allowed(false);
                trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

                // --------------------------
                // 挂上 AXI 扩展
                // --------------------------
                AxiExtension* axi_ext = new AxiExtension;
                axi_ext->axi_id     = cnt & 0x3;
                axi_ext->burst_len  = 0;     // 单拍，演示用
                axi_ext->burst_size = 2;     // 2^2 = 4 bytes
                axi_ext->prot       = 0x2;   // 示例值
                axi_ext->is_write   = true;

                trans.set_extension(axi_ext);

                std::cout << sc_time_stamp()
                          << " Producer send transaction:"
                          << " data=" << data
                          << " addr=0x" << std::hex << trans.get_address() << std::dec
                          << " axi_id=" << axi_ext->axi_id
                          << " burst_len=" << axi_ext->burst_len
                          << " burst_size=" << axi_ext->burst_size
                          << " prot=" << axi_ext->prot
                          << std::endl;

                socket->b_transport(trans, delay);

                wait(delay);

                if (trans.is_response_error()){
                    std::cout << sc_time_stamp()
                              << " Producer response error: "
                              << trans.get_response_string()
                              << std::endl;
                }
                else{
                    std::cout << sc_time_stamp()
                              << " Producer transaction done"
                              << std::endl;
                }

                // simple payload 的 extension 指针默认不会自动 delete
                // 这个例子里事务只活一拍，发送完手动清理即可
                trans.clear_extension(axi_ext);
                delete axi_ext;
            }
        }
    }

    SC_CTOR(Producer) { SC_THREAD(run); }
};


SC_MODULE(Consumer)
{
    tlm_utils::simple_target_socket<Consumer> socket;

    SC_CTOR(Consumer){
        socket.register_b_transport(this, &Consumer::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay){
        uint32_t data = *reinterpret_cast<uint32_t*>(trans.get_data_ptr());

        // 读取 AXI 扩展
        AxiExtension* axi_ext = nullptr;
        trans.get_extension(axi_ext);

        std::cout << sc_time_stamp()
                  << " Consumer got transaction:"
                  << " data=" << data
                  << " addr=0x" << std::hex << trans.get_address() << std::dec;

        if (axi_ext){
            std::cout << " axi_id=" << axi_ext->axi_id
                      << " burst_len=" << axi_ext->burst_len
                      << " burst_size=" << axi_ext->burst_size
                      << " prot=" << axi_ext->prot
                      << " is_write=" << axi_ext->is_write;
        }
        else{
            std::cout << " [no axi extension]";
        }

        std::cout << std::endl;

        // 模拟 target 处理延迟
        delay += sc_time(30, SC_NS);

        // 简单做一点 AXI 风格检查：这里只允许写事务
        if (axi_ext && !axi_ext->is_write){
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        }
        else{
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        }
    }
};


SC_MODULE(Top)
{
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

        for (int i = 0; i < 4; i++) wait(clk.posedge_event());

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


int sc_main(int argc, char* argv[])
{
    Top top("top");
    sc_start();
    return 0;
}