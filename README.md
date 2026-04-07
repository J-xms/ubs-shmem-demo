# UBS Shared Memory 映射读写示例

## 简介

演示 openEuler UBS 共享内存的**映射与读写方法**。

代码引用自官方文档: https://atomgit.com/openeuler/ubs-mem/blob/master/doc/zh/toctopics/使用示例.md

## 依赖

- openEuler 24.03 LTS SP3+
- UBS 已安装 (`rpm -qi ubs-mem`)
- `systemctl start ubsmd` 已运行

## 编译

UBS已装好? 直接:

```bash
make
```

或者手动指定(仅UBS不在标准路径时):

```bash
gcc -o shm_test shm_test.c -lubsm -lpthread -lm -ldl
```

## 运行

```bash
# 使用默认参数
./shm_test

# 自定义参数
./shm_test my_shm_001 100000
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

// 4. 直接读写
*(int64_t*)addr = 123;
int64_t val = *(int64_t*)addr;

// 5. 刷新缓存 (缓存模式必须)
ubsmem_shmem_set_ownership("shm_name", addr, length, PROT_NONE);

// 6. 清理
ubsmem_shmem_unmap(addr, length);
ubsmem_shmem_deallocate("shm_name");
ubsmem_finalize();
```

## 注意

- **UBSM_FLAG_CACHE**: 读写后需调用 `set_ownership(..., PROT_NONE)` 刷新缓存
- **UBSM_FLAG_NONCACHE**: 可跳过刷新步骤

## License

Mulan PSL v2
