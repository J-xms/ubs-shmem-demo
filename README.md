# UBS Shared Memory 映射读写示例

演示 openEuler UBS 共享内存的**映射与读写方法**，引用自官方文档。

## 依赖

- openEuler 24.03 LTS SP3+
- UBS 已安装 (`rpm -qi ubs-mem`)
- `systemctl start ubsmd` 已运行

## 编译

```bash
make
```

或手动:

```bash
gcc -o shm_test shm_test.c -lubsm -lpthread -lm -ldl
```

## 运行

```bash
./shm_test [共享内存名] [迭代次数]

# 示例
./shm_test my_shm 100000
```

## 输出示例

```
============================================================
       UBS Shared Memory 映射读写示例
============================================================
共享内存名: my_shm
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
  │  虚拟地址 (VA):     0x7f9...                   │
  │  本地物理地址 (PA): 0x2a2...                  │  ← UB控制器地址
  │  页帧号 (PFN):     170...                        │
  │  页大小:           4096 bytes                    │
  │  映射长度:         4096 bytes                    │
  │  页内偏移:         0                               │
  └─────────────────────────────────────────────────────┘

[Step 4] 执行写入测试...
------------------------------------------------------------
写入虚拟地址: 0x7f...
写入值: 99999 (最后写入的值)
写入 100000 次: 12.345 ms (8103.54 万次/秒)

[Step 5] 执行读取测试...
------------------------------------------------------------
读取值: 99999
读取 100000 次: 10.234 ms (9771.38 万次/秒)
校验和: 7049827042952256 vs 期望: 7049827042952256 ✓ 通过

[Step 6] 刷新缓存确保一致性...
------------------------------------------------------------
刷新后:
  状态: PROT_NONE (缓存已刷除并失效)
============================================================
```

## 核心代码

```c
// 1. 初始化
ubsmem_init_attributes(&opts);
ubsmem_initialize(&opts);

// 2. 分配 (缓存模式)
ubsmem_shmem_allocate("default", "shm_name", size, 0600, UBSM_FLAG_CACHE);

// 3. 映射 (直接给读写权限)
ubsmem_shmem_map(NULL, length, PROT_WRITE | PROT_READ,
                 MAP_SHARED, "shm_name", 0, &addr);

// 4. 直接读写
*(int64_t*)addr = 123;
int64_t val = *(int64_t*)addr;

// 5. 刷新缓存 (缓存模式必须)
//   - 刷除CPU缓存中的数据
//   - 失效CPU缓存,确保后续读取从真实内存获取
ubsmem_shmem_set_ownership("shm_name", addr, length, PROT_NONE);

// 6. 清理
ubsmem_shmem_unmap(addr, length);
ubsmem_shmem_deallocate("shm_name");
ubsmem_finalize();
```

## 地址信息说明

| 字段 | 说明 |
|------|------|
| VA (虚拟地址) | 进程看到的地址空间 |
| PA (本地物理地址) | **UB控制器**用来寻址远程内存的地址，**不是**远程物理内存的实际地址 |
| PFN (页帧号) | 物理页的编号 |

## 注意

- **UBSM_FLAG_CACHE**: 读写后必须调用 `set_ownership(..., PROT_NONE)` 刷新缓存
- **UBSM_FLAG_NONCACHE**: 可跳过刷新步骤
- 共享内存名最大48字符，只允许字母/数字/下划线

## License

Mulan PSL v2
