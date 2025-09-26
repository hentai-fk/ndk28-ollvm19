# ndk28-ollvm19
这是给Windows系统的ndk 28.2.13676358添加混淆功能的使用方法和编译教程
## 使用方法
- -mllvm -irobf # 强制开启混淆，用在希望给函数添加混淆注释的时候，如果开启了其它混淆那么这个指令就相当于多余的
- -mllvm -irobf-indbr # 开启间接跳转混淆并加密跳转目标
- -mllvm -level-indbr # 间接跳转混淆的加密层级，范围是0~3，0级表示不加密跳转目标
- -mllvm -irobf-icall # 开启间接函数调用混淆并加密目标函数地址
- -mllvm -level-icall # 间接函数调用混淆的加密层级，范围是0~3，0级表示不加密目标函数地址
- -mllvm -irobf-indgv # 开启间接全局变量混淆并加密变量地址
- -mllvm -level-indgv # 间接全局变量混淆的加密层级，范围是0~3，0级表示不加密变量地址
- -mllvm -irobf-fla # 开启控制流平坦化混淆
- -mllvm -irobf-sub # 开启指令替换混淆
- -mllvm -level-sub # 指令替换次数，范围是0~无限，0级表示替换1次
- -mllvm -irobf-bcf # 开启虚假控制流混淆
- -mllvm -level-bcf # 虚假控制流混淆概率，默认是80%，范围是0~100
- -mllvm -irobf-cse # 开启字符串混淆
- -mllvm -irobf-cie # 开启整数常量混淆并加密常量地址
- -mllvm -level-cie # 整数常量混淆的加密层级，范围是0~3，0级表示不加密常量地址
- -mllvm -irobf-cfe # 开启浮点数常量混淆并加密常量地址
- -mllvm -level-cfe # 浮点常量混淆的加密层级，范围是0~3，0级表示不加密常量地址
- -mllvm -irobf-config # 通过配置文件开启混淆，配置文件格式为json
## 准备
### 1. 安装Windows系统的c和c++的编译工具
此教程使用Visual Studio 2022并安装c和c++编译工具MSVC
### 2. 安装Cmake和Ninja工具
这两个工具通过Android studio的sdk管理工具下载cmake工具就行了，在sdk的下载目录的cmake目录里就会包含cmake.exe和ninja.exe
## 整合源码
### 下载LLVM源码
在ndk 28.2.13676358的toolchains\llvm\prebuilt\windows-x86_64\bin目录执行clang --version会得到下面的信息
```
Android (13624864, based on r530567e) clang version 19.0.1 (https://android.googlesource.com/toolchain/llvm-project 97a699bf4812a18fb657c2779f5296a4ab2694d2)
Target: x86_64-w64-windows-gnu
Thread model: posix
InstalledDir: D:/Android/Sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin
```
所以去[llvm官网](https://releases.llvm.org/)下载接近19.0.1版本的release版本源码就行了，我下载了19.1.1版本
把llvm源码解压到任意一个目录里就行了
### 整合源码
把这里的源码下载下来，复制到llvm源码的根目录就行了，然后修改下面几个文件的内容
#### 1. llvm\lib\Transforms\CMakeLists.txt 文件末尾添加以下内容
```
add_subdirectory(Obfuscation)
```
#### 2. llvm\lib\Transforms\IPO\CMakeLists.txt 文件的add_llvm_component_library末尾添加以下内容
```
Obfuscation
```
#### 3. llvm\lib\Passes\PassBuilder.cpp 文件的头文件和构造函数末尾添加以下内容
```
#include "llvm/Transforms/Obfuscation/ObfuscationPassManager.h"
......
PassBuilder::PassBuilder() {
  registerOptimizerLastEPCallback([](llvm::ModulePassManager &MPM, llvm::OptimizationLevel Level) {
    MPM.addPass(ObfuscationPassManagerPass());
  });
}
```
#### 4. llvm\include\llvm\LinkAllPasses.h 文件添加以下内容
```
namespace llvm {
class ModulePass;
ModulePass *createObfuscationPassManager();
}

namespace {
  struct ForcePassLinking {
    ForcePassLinking() {
      (void)llvm::createSelectOptimizePass();

      (void)llvm::createObfuscationPassManager();
    }
  }
}
```
### 编译源码
修改build-msvc.cmd文件的内容，注意把MSVC目录修改成你的安装的目录，然后双击运行等待编译完成
### 整合编译结果
将输出目录build里的bin、include、lib、libexec目录复制到ndk目录的toolchains\llvm\prebuilt\windows-x86_64目录就行了
## 使用示例
### 1. CMakeLists.txt文件
```
add_compile_options("SHELL:-mllvm -irobf-indbr")
add_compile_options("SHELL:-mllvm -level-indbr=2")
add_compile_options("SHELL:-mllvm -irobf-icall")
add_compile_options("SHELL:-mllvm -level-icall=2")
add_compile_options("SHELL:-mllvm -irobf-indgv")
add_compile_options("SHELL:-mllvm -level-indgv=2")
add_compile_options("SHELL:-mllvm -irobf-bcf")
add_compile_options("SHELL:-mllvm -level-bcf=40")
add_compile_options("SHELL:-mllvm -irobf-cse")
```
### 2. Android.mk文件
```
LOCAL_CFLAGS += -mllvm -irobf-indbr -mllvm -level-indbr=2 -mllvm -irobf-icall -mllvm -level-icall=2
```
### 3. 配置文件
使用`-mllvm -irobf-config=path\to\config.json`参数设置混淆配置文件，文件内容如下
```
{
  "indbr": {
    "enable": true,
    "level": 3
  },
  "icall": {
    "enable": true,
    "level": 3
  },
  "indgv": {
    "enable": true,
    "level": 3
  },
}
```
### 4. 函数注释
`+indbr`表示开启混淆
`-indbr`表示关闭混淆
`^indbr=3`表示混淆层级设置为3
```
__attribute__((annotate("+indbr ^indbr=3 -icall ^indgv=2")))
int main() {
    std::cout << "HelloWorld" << std::endl;
    return 0;
}
```