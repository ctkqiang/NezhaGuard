//
// Created by 钟智强 on 2026/7/30.
//

#ifndef NEZHAGUARD_ARENA_H
#define NEZHAGUARD_ARENA_H

#include <cstddef>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace Nezha::Core {
    class Arena {
    public:
        explicit Arena(std::size_t block_size = 64 * 1024);

        ~Arena();

        Arena(const Arena &) = delete;

        Arena &operator=(const Arena &) = delete;

        Arena(Arena &&) noexcept;

        Arena &operator=(Arena &&) noexcept;

        void *allocate(std::size_t n, std::size_t align = alignof(std::max_align_t));

        std::string_view intern(std::string_view s);

        const char *intern_cstr(std::string_view s);

        template<class T, class... A>
        T *create(A &&... args) {
            static_assert(std::is_trivially_destructible_v<T>,
                          "Arena 不调析构，仅限可平凡析构类型");
            void *p = allocate(sizeof(T), alignof(T));
            return new(p) T(std::forward<A>(args)...);
        }

        void reset() noexcept;

        [[nodiscard]] std::size_t bytes_used() const noexcept;

        [[nodiscard]] std::size_t block_count() const noexcept { return blocks_.size(); }

    private:
        struct Block {
            std::unique_ptr<std::byte[]> data;
            std::size_t size;
        };

        void add_block(std::size_t min_size);

        std::vector<Block> blocks_;
        std::size_t cur_ = 0;
        std::size_t head_ = 0;
        std::size_t block_size_;
    };
}

#endif //NEZHAGUARD_ARENA_H
