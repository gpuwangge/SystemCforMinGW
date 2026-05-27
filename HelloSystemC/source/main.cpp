#include "main.h"
#include "common.h"
#include <systemc.h>

using namespace sc_core;

SC_MODULE(Hello) {
    SC_CTOR(Hello) {
        SC_THREAD(run);
    }

    void run() {
        std::cout << "Call Hello SystemC @ " << sc_time_stamp() << "\n";
        wait(10, SC_NS);
        std::cout << "After 10 ns @ " << sc_time_stamp() << "\n";
        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "Hello, World!" << std::endl;
    PrintCommon();
    PrintMain();

    Hello h("h");
    sc_start();

    return 0;
}


