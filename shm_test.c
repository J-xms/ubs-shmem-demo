/*
 * UBS Shared Memory 读写性能测试 Demo
 * ==============================================================
 *
 * 功能: 演示在UBS内存导入端如何对共享内存进行读写，并测试10万次读写的性能
 *
 * 编译:
 *   gcc -o shm_test shm_test.c \
 *       -I<ubs-mem源码目录>/src/app_lib/include \
 *       -L<ubs-mem构建目录>/lib \
 *       -lubsm -lpthread -lm -ldl -lnuma
 *
 * 运行:
 *   ./shm_test <共享内存名> [迭代次数]
 *   示例:
 *     ./shm_test my_shm_001           # 使用默认10万次迭代
 *     ./shm_test my_shm_001 50000     # 指定5万次迭代
 *
 * 前提条件:
 *   - 已在 openEuler 系统上安装并启动 ubs-engine 服务
 *   - 已构建并安装 ubs-mem 库
 *
 *   编译 ubs-mem:
 *     cd <ubs-mem源码目录>
 *     sh build.sh -t release
 *     rpm -ivh build/release/output/ubs-mem-*.rpm
 *
 *   启动服务:
 *     systemctl start ubsmd
 *
 * ==============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

/* UBS 库头文件 */
#include "ubs_mem.h"

/* 默认迭代次数 */
#define DEFAULT_ITERATIONS 100000

/* 默认Region名称 - 使用默认的all2all共享区域 */
#define DEFAULT_REGION_NAME "default"

/* 共享内存对象最小size (页大小) */
#define MIN_SHM_SIZE 4096

/* 校验和对的期望值计算 */
static inline int64_t calc_expected_sum(int32_t n)
{
    return (int64_t)n * (n - 1) / 2;
}

/* 获取当前时间戳(毫秒) */
static inline double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/* 打印使用说明 */
static void print_usage(const char *prog_name)
{
    printf("用法: %s <共享内存名> [迭代次数]\n\n", prog_name);
    printf("参数说明:\n");
    printf("  共享内存名   共享内存的唯一标识名称 (最大48字符)\n");
    printf("  迭代次数     可选, 读写测试的迭代次数, 默认100000\n\n");
    printf("示例:\n");
    printf("  %s my_shm_001\n", prog_name);
    printf("  %s my_shm_001 50000\n", prog_name);
    printf("\n注意:\n");
    printf("  - 需先安装并启动 ubsmd 服务\n");
    printf("  - 共享内存名不能超过48字符\n");
    printf("  - 共享内存名在系统中唯一,已存在的会被先删除\n");
}

/* 验证共享内存名合法性 */
static int validate_shm_name(const char *name)
{
    size_t len;

    if (name == NULL || *name == '\0') {
        fprintf(stderr, "错误: 共享内存名不能为空\n");
        return -1;
    }

    len = strlen(name);
    if (len >= MAX_SHM_NAME_LENGTH) {
        fprintf(stderr, "错误: 共享内存名长度%zu超过限制(%d)\n",
                len, MAX_SHM_NAME_LENGTH - 1);
        return -1;
    }

    /* 名称只允许字母、数字、下划线 */
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            fprintf(stderr, "错误: 共享内存名只允许字母、数字、下划线\n");
            return -1;
        }
    }

    return 0;
}

/* 打印测试结果 */
static void print_result(int32_t iterations,
                         double write_time_ms,
                         double read_time_ms,
                         double mixed_time_ms,
                         int64_t verify_sum,
                         int64_t expected_sum)
{
    printf("\n");
    printf("===========================================================\n");
    printf("                     测试结果汇总                          \n");
    printf("===========================================================\n");
    printf("  总迭代次数:           %d 次\n", iterations);
    printf("  数据类型:             int64_t (8 bytes)\n");
    printf("\n");
    printf("  [纯写入] 耗时:        %.3f ms\n", write_time_ms);
    printf("         吞吐量:        %.2f 万次/秒\n",
           iterations / write_time_ms * 1000.0 / 10000.0);
    printf("\n");
    printf("  [纯读取] 耗时:        %.3f ms\n", read_time_ms);
    printf("         吞吐量:        %.2f 万次/秒\n",
           iterations / read_time_ms * 1000.0 / 10000.0);
    printf("\n");
    printf("  [混合读写] 耗时:      %.3f ms (写+读各%d次)\n",
           mixed_time_ms, iterations);
    printf("         吞吐量:        %.2f 万次/秒\n",
           iterations * 2 / mixed_time_ms * 1000.0 / 10000.0);
    printf("\n");
    printf("  [校验] 读取值求和:    %ld\n", (long)verify_sum);
    printf("         期望求和:      %ld\n", (long)expected_sum);
    printf("         校验结果:      %s\n",
           (verify_sum == expected_sum) ? "✓ 通过" : "✗ 失败");
    printf("===========================================================\n");
}

int main(int argc, char *argv[])
{
    int ret;
    void *shm_ptr = NULL;
    size_t shm_size;
    char *shm_name = NULL;
    char *region_name = NULL;
    int32_t iterations = DEFAULT_ITERATIONS;
    mode_t mode;

    int64_t write_value;
    int64_t read_value;
    int64_t verify_sum;
    int64_t expected_sum;

    double write_start, write_end;
    double read_start, read_end;
    double mixed_start, mixed_end;

    /* -------------------- 参数解析 -------------------- */
    if (argc < 2) {
        fprintf(stderr, "错误: 缺少必需参数\n\n");
        print_usage(argv[0]);
        return 1;
    }

    shm_name = argv[1];
    if (validate_shm_name(shm_name) != 0) {
        return 1;
    }

    if (argc >= 3) {
        iterations = atoi(argv[2]);
        if (iterations <= 0) {
            fprintf(stderr, "错误: 迭代次数必须为正整数\n");
            return 1;
        }
    }

    /* -------------------- 获取系统页大小 -------------------- */
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = MIN_SHM_SIZE;
    }
    shm_size = (size_t)page_size;  /* 至少一个页 */

    /* -------------------- 打印测试信息 -------------------- */
    printf("\n");
    printf("===========================================================\n");
    printf("       UBS Shared Memory 读写性能测试 (v1.0)\n");
    printf("===========================================================\n");
    printf("\n[测试配置]");
    printf("\n  共享内存名:    %s", shm_name);
    printf("\n  Region:        %s", DEFAULT_REGION_NAME);
    printf("\n  映射大小:       %zu bytes", shm_size);
    printf("\n  系统页大小:     %ld bytes", page_size);
    printf("\n  迭代次数:       %d 次", iterations);
    printf("\n  UBS库版本:      openEuler UBS-Memory\n");
    printf("\n");

    /* -------------------- 初始化UBS库 -------------------- */
    printf("[Step 1] 初始化UBS库...\n");
    ubsmem_options_t options;

    ret = ubsmem_init_attributes(&options);
    if (ret != UBSM_OK) {
        fprintf(stderr, "  错误: ubsmem_init_attributes 失败, ret=%d\n", ret);
        return 1;
    }

    ret = ubsmem_initialize(&options);
    if (ret != UBSM_OK) {
        fprintf(stderr, "  错误: ubsmem_initialize 失败, ret=%d\n", ret);
        return 1;
    }
    printf("  ✓ UBS库初始化成功\n\n");

    /* -------------------- 分配共享内存 -------------------- */
    printf("[Step 2] 分配共享内存 '%s'...\n", shm_name);

    /* 设置权限: 所有者读写, 组读, 其他读 */
    mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    /*
     * 尝试先删除已存在的同名共享内存(避免残留)
     * 注意: 这是一个幂等操作,不存在时返回错误码可忽略
     */
    ubsmem_shmem_deallocate(shm_name);

    ret = ubsmem_shmem_allocate(DEFAULT_REGION_NAME, shm_name,
                                 shm_size, mode, UBSM_FLAG_CACHE);
    if (ret != UBSM_OK) {
        fprintf(stderr, "  错误: ubsmem_shmem_allocate 失败, ret=%d\n", ret);
        fprintf(stderr, "  提示: 确认 ubsmd 服务已启动且 Region '%s' 存在\n",
                DEFAULT_REGION_NAME);
        ubsmem_finalize();
        return 1;
    }
    printf("  ✓ 共享内存分配成功 (size=%zu)\n\n", shm_size);

    /* -------------------- 映射共享内存 -------------------- */
    printf("[Step 3] 将共享内存映射到本地虚拟地址空间...\n");

    /*
     * 关键API: ubsmem_shmem_map()
     *
     * 参数说明:
     *   addr       - NULL: 由内核选择映射地址
     *   length     - 映射长度(会被对齐到页边界)
     *   prot       - PROT_NONE: 初始无权限,后续用set_ownership设置
     *   flags      - MAP_SHARED: 映射为共享类型
     *   name       - 共享内存名称(与allocate时一致)
     *   offset     - 偏移量(页对齐)
     *   local_ptr  - [out] 返回可直接访问的本地指针
     *
     * 返回后, 通过 *local_ptr 即可直接读写共享内存
     */
    ret = ubsmem_shmem_map(NULL, shm_size, PROT_NONE,
                           MAP_SHARED, shm_name, 0, &shm_ptr);
    if (ret != UBSM_OK) {
        fprintf(stderr, "  错误: ubsmem_shmem_map 失败, ret=%d\n", ret);
        ubsmem_shmem_deallocate(shm_name);
        ubsmem_finalize();
        return 1;
    }
    if (shm_ptr == NULL) {
        fprintf(stderr, "  错误: 映射返回指针为NULL\n");
        ubsmem_shmem_deallocate(shm_name);
        ubsmem_finalize();
        return 1;
    }
    printf("  ✓ 映射成功, 本地访问地址: %p\n\n", shm_ptr);

    /* -------------------- 设置内存权限 -------------------- */
    printf("[Step 4] 设置共享内存读写权限...\n");

    /*
     * 关键API: ubsmem_shmem_set_ownership()
     *
     * 参数说明:
     *   name    - 共享内存名称
     *   start   - 映射起始地址(与map时一致)
     *   length  - 权限作用长度(必须为页大小倍数)
     *   prot    - PROT_NONE  : 无访问权限
     *            - PROT_READ : 只读
     *            - PROT_READ | PROT_WRITE : 读写
     *
     * 此处设置为读写权限,之后即可像本地内存一样直接访问
     */
    ret = ubsmem_shmem_set_ownership(shm_name, shm_ptr, shm_size,
                                     PROT_READ | PROT_WRITE);
    if (ret != UBSM_OK) {
        fprintf(stderr, "  错误: ubsmem_shmem_set_ownership 失败, ret=%d\n", ret);
        ubsmem_shmem_unmap(shm_ptr, shm_size);
        ubsmem_shmem_deallocate(shm_name);
        ubsmem_finalize();
        return 1;
    }
    printf("  ✓ 权限设置成功 (PROT_READ | PROT_WRITE)\n\n");

    /* -------------------- 10万次读写性能测试 -------------------- */
    printf("[Step 5] 开始读写性能测试...\n");
    printf("-----------------------------------------------------------\n");

    /* ==================== 写测试 ==================== */
    printf("\n[测试1] 纯写入 %d 次\n", iterations);
    write_start = get_time_ms();

    for (int32_t i = 0; i < iterations; i++) {
        /*
         * ★★★ 核心写操作 ★★★
         * 通过映射后的指针直接写入,如同操作本地内存
         */
        *(int64_t *)shm_ptr = (int64_t)i;
    }

    write_end = get_time_ms();
    printf("  写入完成! 耗时: %.3f ms\n", write_end - write_start);

    /* ==================== 读测试 ==================== */
    printf("\n[测试2] 纯读取 %d 次 + 求和校验\n", iterations);
    verify_sum = 0;
    read_start = get_time_ms();

    for (int32_t i = 0; i < iterations; i++) {
        /*
         * ★★★ 核心读操作 ★★★
         * 通过映射后的指针直接读取,如同读取本地内存
         */
        read_value = *(int64_t *)shm_ptr;
        verify_sum += read_value;
    }

    read_end = get_time_ms();
    expected_sum = calc_expected_sum(iterations);

    printf("  读取完成! 耗时: %.3f ms\n", read_end - read_start);
    printf("  校验: 求和=%ld, 期望=%ld, %s\n",
           (long)verify_sum, (long)expected_sum,
           (verify_sum == expected_sum) ? "✓ 通过" : "✗ 失败");

    /* ==================== 混合读写测试 ==================== */
    printf("\n[测试3] 混合读写(写+读) %d 次\n", iterations);
    verify_sum = 0;
    mixed_start = get_time_ms();

    for (int32_t i = 0; i < iterations; i++) {
        /* 写 */
        *(int64_t *)shm_ptr = (int64_t)i;
        /* 读 */
        read_value = *(int64_t *)shm_ptr;
        verify_sum += read_value;
    }

    mixed_end = get_time_ms();

    printf("  混合操作完成! 耗时: %.3f ms\n", mixed_end - mixed_start);

    printf("\n-----------------------------------------------------------\n");

    /* -------------------- 打印汇总结果 -------------------- */
    print_result(iterations,
                 write_end - write_start,
                 read_end - read_start,
                 mixed_end - mixed_start,
                 verify_sum,
                 expected_sum);

    /* -------------------- 清理资源 -------------------- */
    printf("\n[Step 6] 清理资源...\n");

    ret = ubsmem_shmem_unmap(shm_ptr, shm_size);
    if (ret != UBSM_OK) {
        fprintf(stderr, "  警告: ubsmem_shmem_unmap 失败, ret=%d\n", ret);
    } else {
        printf("  ✓ 解除映射成功\n");
    }

    ret = ubsmem_shmem_deallocate(shm_name);
    if (ret != UBSM_OK) {
        fprintf(stderr, "  警告: ubsmem_shmem_deallocate 失败, ret=%d\n", ret);
    } else {
        printf("  ✓ 释放共享内存成功\n");
    }

    ret = ubsmem_finalize();
    if (ret != UBSM_OK) {
        fprintf(stderr, "  警告: ubsmem_finalize 失败, ret=%d\n", ret);
    } else {
        printf("  ✓ UBS库反初始化成功\n");
    }

    printf("\n===========================================================\n");
    printf("                     测试完成                              \n");
    printf("===========================================================\n");

    return 0;
}
