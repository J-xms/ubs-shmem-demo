# UBS Shared Memory 映射读写示例

演示 openEuler UBS 共享内存的**映射与读写方法**，引用自官方文档。

## 依赖

- openEuler 24.03 LTS SP3+
- UBS 已安装 (`rpm -qi ubs-mem`)
- `systemctl start ubsmd` 已运行

## 编译

UBS装好了? 直接:

```bash
make
```

或者手动编译:

```bash
gcc -o shm_test shm_test.c -lubsm -lpthread -lm -ldl
```

## 运行

```bash
# 默认参数
./shm_test

# 自定义参数
./shm_test my_shm_001 100000
```

## 输出示例

```
============================================================
       UBS Shared Memory 映射读写示例
============================================================
共享内存名: demo_shm
迭代次数:   100000

[Step 1] 初始化UBS库...
  OK

[Step 2] 分配共享内存...
  OK: 已分配 4096 bytes

[Step 3] 映射共享内存到本地...
  OK: 映射地址 = 0x7f...

  ┌─────────────────────────────────────────────────────┐
  │              地 址 信 息                             │
  ├─────────────────────────────────────────────────────┤
  │  虚拟地址 (VA):     0x7f...                   │
  │  物理地址 (PA):     0x2a2...                  │
  │  页帧号 (PFN):     ...                             │
  │  页大小:           4096 bytes                        │
  │  映射长度:         4096 bytes                        │
  └─────────────────────────────────────────────────────┘

[Step 4] 执行写入测试...
------------------------------------------------------------
写入虚拟地址: 0x7f...
写入物理地址: 0x2a2...
写入耗时: 12.345 ms (8103.54 万次/秒)

[Step 5] 执行读取测试...
------------------------------------------------------------
读取虚拟地址: 0x7f...
读取物理地址: 0x2a2...
读取耗时: 10.234 ms (9771.38 万次/秒)
校验和: 7049827042952256 vs 期望: 7049827042952256 ✓ 通过

[Step 6] 刷新缓存确保一致性...
------------------------------------------------------------
刷新后:
  虚拟地址: 0x7f...
  物理地址: 0x2a2...
  状态: PROT_NONE (CPU缓存已失效)

[Step 7] 清理资源...
  OK
============================================================
                       完成
============================================================
```

## 核心代码

```c
// 1. 初始化
ubsmem_init_attributes(&opts);
ubsmem_initialize(&opts);

// 2. 分配
ubsmem_shmem_allocate("default", "shm_name", size, 0600, UBSM_FLAG_CACHE);

// 3. 映射 (直接给读写权限)
ubsmem_shmem_map(NULL, length, PROT_WRITE | PROT_READ,
                 MAP_SHARED, "shm_name", 0, &addr);

// 4. 直接读写 - 虚拟地址和物理地址会同时打印
*(int64_t*)addr = 123;
int64_t val = *(int64_t*)addr;

// 5. 刷新缓存 (缓存模式必须)
ubsmem_shmem_set_ownership("shm_name", addr, length, PROT_NONE);

// 6. 清理
ubsmem_shmem_unmap(addr, length);
ubsmem_shmem_deallocate("shm_name");
ubsmem_finalize();
```

## 地址信息说明

| 字段 | 说明 |
|------|------|
| VA (虚拟地址) | 进程看到的地址空间，由 mmap 分配 |
| PA (物理地址) | 实际硬件物理内存地址，从 /proc/self/pagemap 获取 |
| PFN (页帧号) | 物理页的编号，PA = PFN × 页大小 |

## License

Mulan PSL v2
