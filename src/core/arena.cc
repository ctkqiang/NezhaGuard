//
// Created by 钟智强 on 2026/7/30.
//

#include "arena.h"

namespace Nezha::Core {
    Arena::Arena(std::size_t block_size) : block_size_(block_size ? block_size : 4096) {
        add_block(block_size_);
    }

    Arena::~Arena() = default;

    Arena::Arena(Arena &&o) noexcept
        : blocks_(std::move(o.blocks_)), cur_(o.cur_), head_(o.head_), block_size_(o.block_size_) {
        o.cur_ = 0;
        o.head_ = 0;
        o.blocks_.clear();
    }

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

    void Arena::add_block(std::size_t min_size) {
        std::size_t sz = min_size > block_size_ ? min_size : block_size_;
        blocks_.push_back(Block{std::make_unique<std::byte[]>(sz), sz});
    }

    void *Arena::allocate(std::size_t n, std::size_t align) {
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

    std::string_view Arena::intern(std::string_view s) {
        if (s.empty()) return {};
        void *p = allocate(s.size(), 1);
        std::memcpy(p, s.data(), s.size());
        return {static_cast<const char *>(p), s.size()};
    }

    const char *Arena::intern_cstr(std::string_view s) {
        char *p = static_cast<char *>(allocate(s.size() + 1, 1));
        std::memcpy(p, s.data(), s.size());
        p[s.size()] = '\0';
        return p;
    }

    void Arena::reset() noexcept {
        cur_ = 0;
        head_ = 0;
    }

    std::size_t Arena::bytes_used() const noexcept {
        std::size_t used = 0;
        for (std::size_t i = 0; i < cur_; ++i) used += blocks_[i].size;
        return used + head_;
    }
}
