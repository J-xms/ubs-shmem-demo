/*
 * UBS Shared Memory 映射与读写示例
 * ==============================================================
 *
 * 引用自: atomgit.com/openeuler/ubs-mem/doc/zh/toctopics/使用示例.md
 *
 * 功能: 演示UBS内存导入端映射共享内存并进行读写访问
 *
 * 使用:
 *   gcc -o shm_test shm_test.c -I<ubs-mem源码>/src/app_lib/include \
 *        -L<ubs-mem构建目录>/lib -lubsm -lpthread -lm -ldl
 *   ./shm_test [共享内存名] [迭代次数]
 *
 * ==============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include "ubs_mem.h"

#define DEFAULT_SHM_NAME "demo_shm"
#define DEFAULT_REGION   "default"
#define DEFAULT_ITERS    100000

/* 错误处理 */
#define CHECK_RET(ret, msg) do { \
    if ((ret) != UBSM_OK) { \
        fprintf(stderr, "[错误] %s: ret=%d\n", msg, ret); \
        return -1; \
    } \
} while (0)

/* 获取时间(毫秒) */
static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

int main(int argc, char *argv[])
{
    int ret;
    void *addr = NULL;
    size_t length = 4096;  /* 4KB页面 */
    const char *shm_name;
    int iterations;

    /* ---- 参数解析 ---- */
    shm_name = (argc > 1) ? argv[1] : DEFAULT_SHM_NAME;
    iterations = (argc > 2) ? atoi(argv[2]) : DEFAULT_ITERS;

    printf("============================================================\n");
    printf("       UBS Shared Memory 映射读写示例\n");
    printf("============================================================\n");
    printf("共享内存名: %s\n", shm_name);
    printf("迭代次数:   %d\n\n", iterations);

    /* ---- 初始化 ---- */
    printf("[Step 1] 初始化UBS库...\n");
    ubsmem_options_t opts;
    CHECK_RET(ubsmem_init_attributes(&opts), "init_attributes");
    CHECK_RET(ubsmem_initialize(&opts),      "initialize");
    printf("  OK\n\n");

    /* ---- 分配共享内存 ---- */
    printf("[Step 2] 分配共享内存...\n");
    /* 先尝试删除已存在的(幂等操作) */
    ubsmem_shmem_deallocate(shm_name);

    uint64_t flags = UBSM_FLAG_CACHE;  /* 缓存模式 */
    CHECK_RET(ubsmem_shmem_allocate(DEFAULT_REGION, shm_name, length, 0600, flags),
             "shmem_allocate");
    printf("  OK: 已分配 %zu bytes\n\n", length);

    /* ---- 映射共享内存 (官方示例标准写法) ---- */
    printf("[Step 3] 映射共享内存到本地...\n");
    /*
     * 官方示例的直接写法:
     *   ubsmem_shmem_map(地址, 长度, 权限, MAP_SHARED, 名称, 偏移, &指针)
     *
     * 权限直接给 PROT_WRITE | PROT_READ，一步到位
     */
    ret = ubsmem_shmem_map(NULL,                    /* addr: NULL由内核选择 */
                           length,                  /* 长度 */
                           PROT_WRITE | PROT_READ,  /* 读写权限 */
                           MAP_SHARED,             /* 共享映射 */
                           shm_name,               /* 共享内存名 */
                           0,                      /* 偏移 */
                           &addr);                 /* out: 本地指针 */
    CHECK_RET(ret, "shmem_map");
    printf("  OK: 映射地址 = %p\n\n", addr);

    /* ---- 读写测试 ---- */
    printf("[Step 4] 执行读写测试...\n");
    printf("------------------------------------------------------------\n");

    double t0, t1;
    int64_t sum = 0;
    int64_t expected = (int64_t)iterations * (iterations - 1) / 2;

    /* 写入 */
    t0 = now_ms();
    for (int i = 0; i < iterations; i++) {
        /* ★ 核心写操作: 直接通过指针写入，与本地内存无异 */
        *(int64_t *)addr = i;
    }
    t1 = now_ms();
    printf("  写入 %d 次: %.3f ms (%.2f 万次/秒)\n",
           iterations, t1 - t0, iterations / (t1 - t0) / 10.0);

    /* 读取 */
    sum = 0;
    t0 = now_ms();
    for (int i = 0; i < iterations; i++) {
        /* ★ 核心读操作: 直接通过指针读取 */
        sum += *(int64_t *)addr;
    }
    t1 = now_ms();
    printf("  读取 %d 次: %.3f ms (%.2f 万次/秒)\n",
           iterations, t1 - t0, iterations / (t1 - t0) / 10.0);
    printf("  校验和: %ld vs 期望: %ld %s\n\n",
           (long)sum, (long)expected,
           (sum == expected) ? "✓" : "✗");

    /* ---- 关键: 缓存一致性问题 (官方示例重点!) ---- */
    printf("[Step 5] 刷新缓存确保一致性...\n");
    /*
     * ★★★ 官方示例强调的缓存一致性问题 ★★★
     *
     * 对于缓存模式(UBSM_FLAG_CACHE)的共享内存:
     * 在完成读写访问后，必须调用 set_ownership(..., PROT_NONE)
     * 来刷除和失效数据缓存，保证多节点间的数据一致性。
     *
     * 对于非缓存模式(UBSM_FLAG_NONCACHE)，此步骤可省略。
     */
    ret = ubsmem_shmem_set_ownership(shm_name, addr, length, PROT_NONE);
    CHECK_RET(ret, "set_ownership (flush)");
    printf("  OK: 缓存已刷新\n\n");

    /* ---- 清理 ---- */
    printf("[Step 6] 清理资源...\n");
    CHECK_RET(ubsmem_shmem_unmap(addr, length),  "shmem_unmap");
    CHECK_RET(ubsmem_shmem_deallocate(shm_name),  "shmem_deallocate");
    CHECK_RET(ubsmem_finalize(),                  "finalize");
    printf("  OK\n");

    printf("============================================================\n");
    printf("                       完成\n");
    printf("============================================================\n");
    return 0;
}
