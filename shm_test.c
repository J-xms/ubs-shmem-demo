/*
 * UBS Shared Memory 映射与读写示例
 * ==============================================================
 *
 * 引用自: atomgit.com/openeuler/ubs-mem/doc/zh/toctopics/使用示例.md
 *
 * 功能: 演示UBS内存导入端映射共享内存并进行读写访问
 *
 * 编译:
 *   gcc -o shm_test shm_test.c -lubsm -lpthread -lm -ldl
 *
 * 运行:
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
#include <fcntl.h>
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

/*
 * 获取虚拟地址对应的物理地址
 * 通过 /proc/self/pagemap 获取页帧号，再计算物理地址
 *
 * @param vaddr 虚拟地址
 * @return 物理地址 (uint64_t), 0表示获取失败
 */
static uint64_t get_physical_address(void *vaddr)
{
    int fd;
    uint64_t page_frame_number = 0;
    unsigned long vaddr_ul = (unsigned long)vaddr;
    long page_size = sysconf(_SC_PAGESIZE);
    uint64_t pagemap_entry;
    ssize_t ret;

    /* 打开pagemap文件 */
    fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        perror("open /proc/self/pagemap");
        return 0;
    }

    /*
     * pagemap中每个条目64位，条目索引 = 虚拟地址 / 页大小
     * 每个条目的结构:
     *   bits 0-54: 页帧号 (PFN)
     *   bit  63:   页存在标志
     */
    off_t offset = (vaddr_ul / page_size) * sizeof(uint64_t);
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        close(fd);
        return 0;
    }

    ret = read(fd, &pagemap_entry, sizeof(pagemap_entry));
    close(fd);

    if (ret != sizeof(pagemap_entry)) {
        return 0;
    }

    /* 检查页是否存在 (bit 63) */
    if ((pagemap_entry & (1ULL << 63)) == 0) {
        /* 页不在内存中或未分配 */
        return 0;
    }

    /* 提取页帧号 (bits 0-54) */
    page_frame_number = pagemap_entry & ((1ULL << 55) - 1);

    /* 计算物理地址 = 页帧号 * 页大小 + 页内偏移 */
    uint64_t offset_in_page = vaddr_ul % page_size;
    uint64_t phys_addr = (page_frame_number * page_size) + offset_in_page;

    return phys_addr;
}

/*
 * 获取页帧号
 */
static uint64_t get_page_frame_number(void *vaddr)
{
    int fd;
    unsigned long vaddr_ul = (unsigned long)vaddr;
    long page_size = sysconf(_SC_PAGESIZE);
    uint64_t pagemap_entry;
    ssize_t ret;

    fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    off_t offset = (vaddr_ul / page_size) * sizeof(uint64_t);
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        close(fd);
        return 0;
    }

    ret = read(fd, &pagemap_entry, sizeof(pagemap_entry));
    close(fd);

    if (ret != sizeof(pagemap_entry) || ((pagemap_entry & (1ULL << 63)) == 0)) {
        return 0;
    }

    return pagemap_entry & ((1ULL << 55) - 1);
}

/*
 * 打印地址映射信息
 */
static void print_address_info(void *vaddr, size_t length)
{
    long page_size = sysconf(_SC_PAGESIZE);
    uint64_t phys_addr = get_physical_address(vaddr);
    uint64_t pfn = get_page_frame_number(vaddr);

    printf("\n");
    printf("  ┌─────────────────────────────────────────────────────┐\n");
    printf("  │              地 址 信 息                             │\n");
    printf("  ├─────────────────────────────────────────────────────┤\n");
    printf("  │  虚拟地址 (VA):     0x%016lx                   │\n", (unsigned long)vaddr);
    printf("  │  物理地址 (PA):     0x%016lx                   │\n", phys_addr);
    printf("  │  页帧号 (PFN):     %-20lu              │\n", (unsigned long)pfn);
    printf("  │  页大小:           %ld bytes                        │\n", page_size);
    printf("  │  映射长度:         %zu bytes                        │\n", length);
    printf("  │  页内偏移:         %ld                               │\n", (unsigned long)vaddr % page_size);
    printf("  └─────────────────────────────────────────────────────┘\n");
    printf("\n");
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

    /* ---- 映射共享内存 ---- */
    printf("[Step 3] 映射共享内存到本地...\n");
    ret = ubsmem_shmem_map(NULL,                    /* addr: NULL由内核选择 */
                           length,                  /* 长度 */
                           PROT_WRITE | PROT_READ,  /* 读写权限 */
                           MAP_SHARED,             /* 共享映射 */
                           shm_name,               /* 共享内存名 */
                           0,                      /* 偏移 */
                           &addr);                 /* out: 本地指针 */
    CHECK_RET(ret, "shmem_map");
    printf("  OK: 映射地址 = %p\n", addr);

    /* ---- 打印地址映射信息 ---- */
    print_address_info(addr, length);

    /* ---- 写入测试 ---- */
    printf("[Step 4] 执行写入测试...\n");
    printf("------------------------------------------------------------\n");
    printf("写入虚拟地址: %p\n", addr);
    printf("写入物理地址: 0x%lx\n", get_physical_address(addr));

    double t0 = now_ms();
    for (int i = 0; i < iterations; i++) {
        /* ★ 核心写操作: 直接通过指针写入，与本地内存无异 */
        *(int64_t *)addr = i;
    }
    double t1 = now_ms();
    printf("写入值: %ld (最后写入的值)\n", (long)*(int64_t*)addr);
    printf("写入耗时: %.3f ms (%.2f 万次/秒)\n\n",
           t1 - t0, iterations / (t1 - t0) / 10.0);

    /* ---- 读取测试 ---- */
    printf("[Step 5] 执行读取测试...\n");
    printf("------------------------------------------------------------\n");
    printf("读取虚拟地址: %p\n", addr);
    printf("读取物理地址: 0x%lx\n", get_physical_address(addr));

    int64_t sum = 0;
    int64_t expected = (int64_t)iterations * (iterations - 1) / 2;

    t0 = now_ms();
    for (int i = 0; i < iterations; i++) {
        /* ★ 核心读操作: 直接通过指针读取 */
        sum += *(int64_t *)addr;
    }
    t1 = now_ms();
    printf("读取值: %ld\n", (long)*(int64_t*)addr);
    printf("读取耗时: %.3f ms (%.2f 万次/秒)\n", t1 - t0, iterations / (t1 - t0) / 10.0);
    printf("校验和: %ld vs 期望: %ld %s\n\n",
           (long)sum, (long)expected,
           (sum == expected) ? "✓ 通过" : "✗ 失败");

    /* ---- 关键: 刷新缓存 ---- */
    printf("[Step 6] 刷新缓存确保一致性...\n");
    printf("------------------------------------------------------------\n");
    /*
     * 对于缓存模式(UBSM_FLAG_CACHE)的共享内存:
     * 读写后调用 set_ownership(..., PROT_NONE) 刷除和失效数据缓存
     */
    ret = ubsmem_shmem_set_ownership(shm_name, addr, length, PROT_NONE);
    CHECK_RET(ret, "set_ownership (flush)");

    /* 刷新后再次打印地址信息 */
    printf("刷新后:\n");
    printf("  虚拟地址: %p\n", addr);
    printf("  物理地址: 0x%lx\n", get_physical_address(addr));
    printf("  状态: PROT_NONE (CPU缓存已失效)\n\n");

    /* ---- 清理 ---- */
    printf("[Step 7] 清理资源...\n");
    CHECK_RET(ubsmem_shmem_unmap(addr, length),    "shmem_unmap");
    CHECK_RET(ubsmem_shmem_deallocate(shm_name),  "shmem_deallocate");
    CHECK_RET(ubsmem_finalize(),                  "finalize");
    printf("  OK\n");

    printf("============================================================\n");
    printf("                       完成\n");
    printf("============================================================\n");
    return 0;
}
