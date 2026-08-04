//
// Created by 钟智强 on 2026/7/30.
//

#include "arena.h"
#include <cstring>

namespace Nezha::Core {
    /*
     * 构造函数
     * 作用:
     *   初始化 Arena(内存竞技场/线性分配器), 并预先申请第一块内存块。
     *
     * 参数:
     *   block_size - 期望的默认块大小(单位: 字节)。
     *                若传入 0, 则回退为 4096(即一页内存的大小),
     *                这样可以保证 block_size_ 永远为正数, 避免后续
     *                分配逻辑中出现除零或零大小块的异常情况。
     *
     * 说明:
     *   构造时立即调用 add_block() 申请首块内存, 使得对象一旦创建
     *   便可直接用于分配, 无需额外的惰性初始化判断。
     */
    Arena::Arena(std::size_t block_size) : block_size_(block_size ? block_size : 4096) {
        // 4096 = 最小一页
        add_block(block_size_);
    }

    /*
     * 析构函数
     * 使用 = default 由编译器自动生成。
     * 由于内存块 blocks_ 中的数据均由 std::unique_ptr 管理,
     * 对象销毁时会自动释放全部堆内存, 无需手动 delete, 天然做到 RAII。
     */
    Arena::~Arena() = default;

    /*
     * 移动构造函数
     * 作用:
     *   将源对象 o 的全部资源(内存块、游标、当前块内偏移、块大小)
     *   转移给新构造的对象, 实现零拷贝的所有权转移。
     *
     * 参数:
     *   o - 被移动(掏空)的源 Arena 对象(右值引用)。
     *
     * 说明:
     *   移动完成后, 将源对象重置为一个有效但为空的状态
     *   (游标清零、清空块列表), 以保证源对象后续能被安全析构,
     *   不会出现悬空指针或重复释放的问题。
     */
    Arena::Arena(Arena &&o) noexcept
        : blocks_(std::move(o.blocks_)), cur_(o.cur_), head_(o.head_), block_size_(o.block_size_) {
        o.cur_ = 0;
        o.head_ = 0;
        o.blocks_.clear();
    }

    /*
     * 移动赋值运算符
     * 作用:
     *   将源对象 o 的资源转移到当前已存在的对象中。
     *
     * 参数:
     *   o - 被移动的源 Arena 对象(右值引用)。
     *
     * 返回:
     *   *this - 当前对象的引用, 以支持链式赋值。
     *
     * 说明:
     *   首先进行自赋值检测(this != &o), 避免把自己移动给自己
     *   而导致资源被意外清空; 移动完成后同样将源对象重置为
     *   空的有效状态。
     */
    Arena &Arena::operator=(Arena &&o) noexcept {
        if (this != &o) {
            blocks_ = std::move(o.blocks_);
            cur_ = o.cur_;
            head_ = o.head_;
            block_size_ = o.block_size_;
            o.cur_ = 0;
            o.head_ = 0;
            o.blocks_.clear();
        }
        return *this;
    }

    /*
     * add_block - 追加一块新的内存块
     * 作用:
     *   向 blocks_ 列表尾部申请并追加一块新的内存。
     *
     * 参数:
     *   min_size - 本次请求所需的最小字节数(通常来自一次超大分配)。
     *
     * 说明:
     *   实际申请大小 sz 取 min_size 与默认块大小 block_size_ 中的
     *   较大者, 既保证能满足当前的大对象请求, 又保持内存块不会
     *   过于零碎; 内存以 std::byte[] 形式由 unique_ptr 持有,
     *   确保生命周期自动管理。
     */
    void Arena::add_block(std::size_t min_size) {
        std::size_t sz = min_size > block_size_ ? min_size : block_size_;
        blocks_.push_back(Block{std::make_unique<std::byte[]>(sz), sz});
    }

    /*
     * allocate - 从竞技场中分配一段对齐的原始内存
     * 作用:
     *   在当前内存块中划出 n 字节且满足 align 对齐要求的内存区域。
     *
     * 参数:
     *   n     - 需要分配的字节数。
     *   align - 内存对齐要求(必须为 2 的幂)。
     *
     * 返回:
     *   指向已分配内存起始位置的裸指针。
     *
     * 实现要点:
     *   1) 对齐计算: (head_ + (align - 1)) & ~(align - 1)
     *      将当前偏移 head_ 向上取整到 align 的整数倍。
     *   2) 容量检查: 若对齐后所需空间超出当前块剩余容量, 则:
     *      - 优先复用下一块已存在且足够大的内存块(避免频繁申请);
     *      - 否则调用 add_block 申请一块新的足够大的内存块。
     *      切换到新块后将块内偏移 aligned 置 0
     *      (因为每块块首地址天然满足最大对齐)。
     *   3) 更新游标: 计算返回指针 p, 并把 head_ 前移到本次
     *      分配之后的位置, 供下一次分配继续使用。
     *
     * 注意:
     *   本分配器仅做线性(bump-pointer)分配, 不支持单独释放,
     *   只能通过 reset() 或整体析构统一回收。
     */
    void *Arena::allocate(std::size_t n, std::size_t align) {
        // 对齐填充: (ptr + (align-1)) & ~(align-1) 向上取整到 align 倍数
        std::size_t aligned = (head_ + (align - 1)) & ~(align - 1);
        if (aligned + n > blocks_[cur_].size) {
            if (cur_ + 1 < blocks_.size() && n <= blocks_[cur_ + 1].size) {
                ++cur_;
                head_ = 0;
            } else {
                add_block(n);
                cur_ = blocks_.size() - 1;
                head_ = 0;
            }
            aligned = 0; // 新块块首已最大对齐
        }
        std::byte *p = blocks_[cur_].data.get() + aligned;
        head_ = aligned + n;
        return p;
    }

    /*
     * intern - 将字符串驻留(拷贝)到竞技场中
     * 作用:
     *   把传入的字符串内容复制一份到 Arena 管理的内存里,
     *   并返回指向该副本的 string_view。
     *
     * 参数:
     *   s - 待驻留的源字符串视图。
     *
     * 返回:
     *   指向 Arena 内部副本的 std::string_view;
     *   若源为空串, 则返回一个空视图。
     *
     * 说明:
     *   返回的视图不含结尾的 '\0', 其生命周期与 Arena 绑定,
     *   在 Arena 被 reset 或析构前始终有效。对齐参数取 1,
     *   因为字符数据本身无特殊对齐需求。
     */
    std::string_view Arena::intern(std::string_view s) {
        if (s.empty()) return {};
        void *p = allocate(s.size(), 1);
        std::memcpy(p, s.data(), s.size());
        return {static_cast<const char *>(p), s.size()};
    }

    /*
     * intern_cstr - 驻留为以 '\0' 结尾的 C 风格字符串
     * 作用:
     *   将字符串拷贝到 Arena, 并在末尾追加终止符 '\0',
     *   返回可直接用于 C API 的 const char* 指针。
     *
     * 参数:
     *   s - 待驻留的源字符串视图。
     *
     * 返回:
     *   指向 Arena 内部、以 '\0' 结尾的字符串首地址。
     *
     * 说明:
     *   分配大小为 s.size() + 1, 多出的一个字节用于存放
     *   字符串终止符, 从而兼容需要 C 字符串的接口。
     */
    const char *Arena::intern_cstr(std::string_view s) {
        char *p = static_cast<char *>(allocate(s.size() + 1, 1));
        std::memcpy(p, s.data(), s.size());
        p[s.size()] = '\0';
        return p;
    }

    /*
     * reset - 重置竞技场
     * 作用:
     *   将分配游标复位, 使得已申请的内存块可被重复利用。
     *
     * 说明:
     *   仅重置当前块索引 cur_ 与块内偏移 head_, 并不释放
     *   任何已申请的内存块(blocks_ 保持不变)。因此下一轮
     *   分配可直接复用现有内存, 避免反复向系统申请/归还内存,
     *   非常适合"多轮处理、每轮结束批量回收"的使用场景。
     */
    void Arena::reset() noexcept {
        cur_ = 0;
        head_ = 0;
    }

    /*
     * bytes_used - 统计当前已使用的字节总量
     * 作用:
     *   计算竞技场中当前实际占用的字节数。
     *
     * 返回:
     *   已使用的字节总数。
     *
     * 计算方式:
     *   累加当前块之前所有块的完整容量(这些块已被用满并切换过去),
     *   再加上当前块内已使用的偏移量 head_, 即为总使用量。
     *   注意: 该值为"已用容量"而非"已申请容量",
     *   不包含当前块中尚未使用的剩余空间。
     */
    std::size_t Arena::bytes_used() const noexcept {
        std::size_t used = 0;
        for (std::size_t i = 0; i < cur_; ++i) used += blocks_[i].size;
        return used + head_;
    }
}
