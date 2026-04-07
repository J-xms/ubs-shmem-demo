# UBS Shared Memory 读写测试 Demo

## 概述

本项目提供基于 **openEuler UBS (Unified Bus Service Memory)** 的共享内存读写性能测试Demo。

UBS 是 openEuler 超节点上基于 UB 硬件能力实现的内存高阶服务，提供跨节点的内存共享、借用、缓存等能力。本Demo演示在**内存导入端**（消费方）如何对共享内存进行读写操作。

## 特性

- ✅ 展示UBS共享内存的完整使用流程
- ✅ 演示内存导入端的核心读写模式
- ✅ 提供10万次读写性能测试
- ✅ 支持命令行参数灵活配置
- ✅ 包含详细的代码注释和文档

## 依赖环境

### 硬件要求
- 支持 UB 硬件能力的 openEuler 超节点环境

### 软件要求
- **操作系统**: openEuler 24.03 LTS SP3 或更高版本
- **UBS 服务**: ubs-engine + ubs-mem 已安装并运行

### 系统准备

```bash
# 1. 启动 UBS 守护进程 (需要 root 权限)
systemctl start ubsmd

# 2. 确认服务运行状态
systemctl status ubsmd
```

## 编译说明

### 方式一: 使用已安装的RPM包

如果 ubs-mem 已通过 RPM 安装到系统:

```bash
# 查找库文件位置
find /usr -name "libubsm*" 2>/dev/null

# 查找头文件位置
find /usr -name "ubs_mem.h" 2>/dev/null

# 编译 (根据实际路径调整)
gcc -o shm_test shm_test.c \
    -I/usr/include/ubs-mem \
    -L/usr/lib \
    -lubsm -lpthread -lm -ldl -lnuma
```

### 方式二: 从源码编译 ubs-mem

```bash
# 1. 克隆 ubs-mem 源码
git clone https://atomgit.com/openeuler/ubs-mem.git
cd ubs-mem

# 2. 构建 (需要 gcc, cmake, ninja-build, numactl-devel 等)
sh build.sh -t release

# 3. 安装 RPM 包
rpm -ivh build/release/output/ubs-mem-*.rpm

# 4. 回到本项目目录编译
cd /path/to/shmTest
gcc -o shm_test shm_test.c \
    -I<ubs-mem源码目录>/src/app_lib/include \
    -L<ubs-mem构建目录>/lib \
    -lubsm -lpthread -lm -ldl -lnuma
```

### 编译参数说明

| 参数 | 说明 |
|------|------|
| `-I<path>` | ubs_mem.h 头文件目录 |
| `-L<path>` | ubsm 库文件目录 |
| `-lubsm` | UBS 内存库 |
| `-lpthread` | 线程库 |
| `-lm` | 数学库 |
| `-ldl` | 动态链接库 |
| `-lnuma` | NUMA 库 |

## 使用说明

### 基本用法

```bash
# 默认10万次迭代
./shm_test <共享内存名>

# 指定迭代次数
./shm_test <共享内存名> <迭代次数>
```

### 示例

```bash
# 示例1: 使用默认配置测试
./shm_test my_shm_001

# 示例2: 测试5万次迭代
./shm_test test_shm_abc 50000

# 示例3: 测试100万次迭代
./shm_test large_test 1000000
```

### 运行输出示例

```
===========================================================
       UBS Shared Memory 读写性能测试 (v1.0)
===========================================================

[测试配置]
  共享内存名:    my_shm_001
  Region:        default
  映射大小:       4096 bytes
  系统页大小:     4096 bytes
  迭代次数:       100000 次
  UBS库版本:      openEuler UBS-Memory

[Step 1] 初始化UBS库...
  ✓ UBS库初始化成功

[Step 2] 分配共享内存 'my_shm_001'...
  ✓ 共享内存分配成功 (size=4096)

[Step 3] 将共享内存映射到本地虚拟地址空间...
  ✓ 映射成功, 本地访问地址: 0x7f...

[Step 4] 设置共享内存读写权限...
  ✓ 权限设置成功 (PROT_READ | PROT_WRITE)

[Step 5] 开始读写性能测试...
-----------------------------------------------------------

[测试1] 纯写入 100000 次
  写入完成! 耗时: 12.345 ms

[测试2] 纯读取 100000 次 + 求和校验
  读取完成! 耗时: 10.234 ms
  校验: 求和=7049827042952256, 期望=7049827042952256, ✓ 通过

[测试3] 混合读写(写+读) 100000 次
  混合操作完成! 耗时: 25.678 ms

-----------------------------------------------------------

===========================================================
                     测试结果汇总
===========================================================
  总迭代次数:           100000 次
  数据类型:             int64_t (8 bytes)

  [纯写入] 耗时:        12.345 ms
         吞吐量:        8103.54 万次/秒

  [纯读取] 耗时:        10.234 ms
         吞吐量:        9771.38 万次/秒

  [混合读写] 耗时:      25.678 ms (写+读各100000次)
         吞吐量:        7788.54 万次/秒

  [校验] 读取值求和:    7049827042952256
         期望求和:      7049827042952256
         校验结果:      ✓ 通过
===========================================================
```

## 核心API使用模式

### 完整流程

```c
// 1. 初始化
ubsmem_options_t opts;
ubsmem_init_attributes(&opts);
ubsmem_initialize(&opts);

// 2. 分配共享内存
ubsmem_shmem_allocate("default", "shm_name", size, mode, UBSM_FLAG_CACHE);

// 3. 映射到本地 - 关键步骤,获取可直接访问的指针
void *ptr;
ubsmem_shmem_map(NULL, size, PROT_NONE, MAP_SHARED, "shm_name", 0, &ptr);

// 4. 设置权限
ubsmem_shmem_set_ownership("shm_name", ptr, size, PROT_READ | PROT_WRITE);

// 5. 直接读写 (如同本地内存)
*(int64_t*)ptr = 123;      // 写
int64_t val = *(int64_t*)ptr;  // 读

// 6. 解映射
ubsmem_shmem_unmap(ptr, size);

// 7. 释放
ubsmem_shmem_deallocate("shm_name");

// 8. 反初始化
ubsmem_finalize();
```

### 关键点说明

| API | 作用 |
|-----|------|
| `ubsmem_initialize()` | 初始化UBS库,必须第一个调用 |
| `ubsmem_shmem_allocate()` | 在指定的Region上分配共享内存 |
| **`ubsmem_shmem_map()`** | **映射到本地,返回可直接读写的指针 `local_ptr`** |
| `ubsmem_shmem_set_ownership()` | 设置内存区域的访问权限 |
| `ubsmem_shmem_write_lock()` | 加写锁(多进程场景) |
| `ubsmem_shmem_read_lock()` | 加读锁(多进程场景) |
| `ubsmem_shmem_unlock()` | 解锁 |
| `ubsmem_shmem_unmap()` | 取消映射 |
| `ubsmem_shmem_deallocate()` | 释放共享内存 |
| `ubsmem_finalize()` | 关闭UBS库 |

## UBS 与 POSIX 共享内存对比

| 特性 | UBS | POSIX shmget/shmat |
|------|-----|-------------------|
| **获取访问指针** | `ubsmem_shmem_map()` 返回 `local_ptr` | `shmat()` 返回指针 |
| **读写方式** | `*(type*)local_ptr` 直接访问 | `*(type*)ptr` 直接访问 |
| **跨节点能力** | ✅ 支持远程内存访问 | ❌ 仅本地 |
| **锁机制** | `write_lock/read_lock/unlock` | 需外部锁 |
| **权限设置** | `set_ownership()` | `shmctl()` |

## 项目结构

```
shmTest/
├── shm_test.c      # 主程序源码
├── README.md       # 本文档
└── LICENSE         # 许可证 (Mulan PSL v2)
```

## 限制与注意事项

1. **环境依赖**: 本程序必须在安装了UBS服务的openEuler环境运行
2. **权限要求**: 创建/删除共享内存可能需要适当权限
3. **名称限制**: 共享内存名最大48字符,只允许字母/数字/下划线
4. **服务状态**: 确保 `ubsmd` 服务处于运行状态
5. **资源清理**: 程序会自动清理创建的共享内存,异常退出时可能需要手动清理

## 故障排查

### 常见错误

| 错误信息 | 可能原因 | 解决方案 |
|---------|---------|---------|
| `ubsmem_initialize 失败` | UBS库未安装或服务未启动 | 检查 `systemctl status ubsmd` |
| `ubsmem_shmem_allocate 失败` | Region不存在或权限不足 | 检查 `ubsmd` 状态和日志 |
| `ubsmem_shmem_map 失败` | 共享内存不存在 | 确认先调用allocate |
| `权限设置失败` | 参数错误或内存已被占用 | 检查参数是否正确 |

### 调试方法

```bash
# 查看 UBS 服务日志
journalctl -u ubsmd -f

# 检查 UBS 服务状态
systemctl status ubsmd

# 查看共享内存列表 (如果有命令)
# ubsmem_shmem_list_lookup 或类似命令
```

## License

本项目代码基于 Mulan PSL v2 许可证开源。
See [LICENSE](LICENSE) for details.

## 参考资料

- [openEuler UBS 官方文档](https://www.openeuler.org/)
- [ubs-mem 源码仓库](https://atomgit.com/openeuler/ubs-mem)
- [ubs-engine 源码仓库](https://atomgit.com/openeuler/ubs-engine)
