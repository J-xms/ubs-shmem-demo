# UBS Shared Memory 映射读写示例

## 简介

本项目演示 openEuler UBS (Unified Bus Service Memory) 共享内存的**映射与读写方法**，代码引用自官方文档。

> 原文档: https://atomgit.com/openeuler/ubs-mem/blob/master/doc/zh/toctopics/使用示例.md

## 依赖

- openEuler 24.03 LTS SP3+
- ubs-engine + ubs-mem 已安装
- `systemctl start ubsmd` 已运行

## 编译

```bash
# 设置 ubs-mem 源码路径
export UBS_HOME=/path/to/ubs-mem

gcc -o shm_test shm_test.c \
    -I${UBS_HOME}/src/app_lib/include \
    -L${UBS_HOME}/build/lib \
    -lubsm -lpthread -lm -ldl
```

或使用 Makefile:

```bash
make UBS_HOME=/path/to/ubs-mem
```

## 运行

```bash
# 默认参数
./shm_test

# 自定义共享内存名和迭代次数
./shm_test my_shm_001 100000
```

## 核心代码模式

```c
// 1. 初始化
ubsmem_init_attributes(&opts);
ubsmem_initialize(&opts);

// 2. 分配共享内存
ubsmem_shmem_allocate("default", "shm_name", size, 0600, UBSM_FLAG_CACHE);

// 3. 映射到本地 (一步到位，直接给读写权限)
ubsmem_shmem_map(NULL, length, PROT_WRITE | PROT_READ,
                 MAP_SHARED, "shm_name", 0, &addr);

// 4. ★ 直接读写 (与本地内存完全一样) ★
*(int64_t*)addr = 123;           // 写
int64_t val = *(int64_t*)addr;   // 读

// 5. 【关键】刷新缓存保证一致性 (仅缓存模式需要)
ubsmem_shmem_set_ownership("shm_name", addr, length, PROT_NONE);

// 6. 清理
ubsmem_shmem_unmap(addr, length);
ubsmem_shmem_deallocate("shm_name");
ubsmem_finalize();
```

## 注意事项

- **缓存一致性**: 使用 `UBSM_FLAG_CACHE` 时，读写后必须调用 `set_ownership(..., PROT_NONE)` 刷新缓存
- **非缓存模式**: 使用 `UBSM_FLAG_NONCACHE` 时可跳过缓存刷新步骤
- **跨节点**: UBS 支持跨节点共享，需配合 `ubsmem_create_region` 创建Region

## License

Mulan PSL v2
