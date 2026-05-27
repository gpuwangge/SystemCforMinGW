## Build Instruction
```cmake
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
make -j
```

## Tutorial
https://github.com/gpuwangge/SystemCforMinGW/blob/main/Tutorial.md  

## Install Instruction
平台:windows  
编译器:gcc (minGW)  

### Download SystemC
https://www.accellera.org/downloads/standards/systemc  
下载这个(或对应的最新版本):
```
SystemC 3.0.2 (Includes TLM) -> Core SystemC Language and Examples (.zip)  2025-10-31  
```
解压缩，项目目录会有CMakeLists.txt  
建立一个SystemC安装文件夹，比如C:/systemc/3.0.2-mingw  

### 编译SystemC Lib 
进入CMakeLists.txt的文件夹，执行如下步骤  
1 配置Configue: CMake 本质上不是编译器，它是“生成构建脚本的工具”。这一步通常不会真正把库编出来，它主要是“准备好怎么编”。  
```
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=C:/systemc/3.0.2-mingw
```
2 构建Build:会调用第 1 步生成的构建系统去实际编译代码。编译完成后，产物一般会留在 build/ 里面(还没“安装”到最终目录)。如果编译器不支持posix这一步会出问题，需要调整使用支持posix的编译器。  
```
cmake --build build -j
```
3 安装Install: 把第 2 步编出来的东西，按项目定义的安装规则复制到你在第 1 步设定的 CMAKE_INSTALL_PREFIX 目录。  
对 SystemC 来说，通常包括：头文件(include/systemc 等)，库文件(lib 里的一些 .a / .dll.a 等)，可能还有 DLL(bin/)，可能还有 cmake 配置文件(方便其他工程 find_package 用)。  
```
cmake --install build
```
安装完成后会有：  
```
C:\systemc\3.0.2-mingw\include  
C:\systemc\3.0.2-mingw\lib(里面有 libsystemc.a)  
C:\systemc\3.0.2-mingw\share\doc  
```


