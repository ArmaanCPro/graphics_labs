#pragma once

#include <cassert>
#include <cstdint>
#include <compare>
#include <utility>
#include <vector>

namespace enger
{
    class Device;

    /// Handles are non-owning, non-reference counted pointers.
    /// They should be used in conjunction with a corresponding Pool to manage the actual resources.
    template<typename ObjectType>
    class Handle final
    {
    private:
        uint32_t index_ = 0;
        uint32_t gen_ = 0;

        Handle(uint32_t index, uint32_t gen) :
            index_(index),
            gen_(gen)
        {}

        template<typename ObjectType, typename ImplObjectType>
        friend class Pool;

    public:
        Handle() = default;

        [[nodiscard]] bool empty() const
        {
            return gen_ == 0;
        }

        [[nodiscard]] bool valid() const
        {
            return gen_ != 0;
        }

        [[nodiscard]] uint32_t index() const
        {
            return index_;
        }

        [[nodiscard]] uint32_t gen() const
        {
            return gen_;
        }

        [[nodiscard]] void* indexAsVoid() const
        {
            return reinterpret_cast<void*>(static_cast<ptrdiff_t>(index_));
        }

        auto operator<=>(const Handle &other) const = default;

        explicit operator bool() const
        {
            return valid();
        }
    };

    static_assert(sizeof(Handle<void>) == sizeof(uint32_t) * 2);

    /// These are just tags, they don't mean anything but are useful to differentiate different Handle types.
    /// When you create a Pool, you use the Tag type and the Impl type.
    using ComputePipelineHandle = Handle<struct ComputePipelineTag>;
    using TextureHandle = Handle<struct TextureTag>;
    using DescriptorSetLayoutHandle = Handle<struct DescriptorSetLayoutTag>;

    /// These functions indirect deletion so that the Holder can directly call deletors
    void destroy(Device* device, ComputePipelineHandle handle);
    void destroy(Device* device, TextureHandle handle);
    void destroy(Device* device, DescriptorSetLayoutHandle handle);

    /// This is an RAII class that actually owns the lifetime of an object that a Handle Points to.
    /// This is an optional type, useful for when lexical scope (RAII) matches actual resource lifetime.
    /// If you'd like, you can use deferred deletion queues after extracting the Handle from a holder.
    template<typename HandleType>
    class Holder final
    {
    public:
        Holder() = default;

        Holder(Device *device, HandleType handle) :
            m_Device(device),
            m_Handle(handle)
        {}

        ~Holder()
        {
            destroy(m_Device, m_Handle);
        }

        Holder(const Holder &) = delete;

        Holder(Holder &&rhs) noexcept :
            m_Device(rhs.m_Device),
            m_Handle(rhs.m_Handle)
        {
            rhs.m_Device = nullptr;
            rhs.m_Handle = {};
        }

        Holder &operator=(const Holder &) = delete;

        Holder &operator=(Holder &&rhs) noexcept
        {
            std::swap(m_Device, rhs.m_Device);
            std::swap(m_Handle, rhs.m_Handle);
            return *this;
        }

        Holder &operator=(std::nullptr_t) noexcept
        {
            reset();
            return *this;
        }

        inline operator HandleType() const
        {
            return m_Handle;
        }

        [[nodiscard]] bool valid() const
        {
            return m_Handle.valid();
        }

        [[nodiscard]] bool empty() const
        {
            return m_Handle.empty();
        }

        void reset() noexcept
        {
            destroy(m_Device, m_Handle);
            m_Device = nullptr;
            m_Handle = {};
        }

        HandleType release() noexcept
        {
            m_Device = nullptr;
            return std::exchange(m_Handle, {});
        }

        [[nodiscard]] uint32_t index() const
        {
            return m_Handle.index();
        }

        [[nodiscard]] void* indexAsVoid() const
        {
            return m_Handle.indexAsVoid();
        }

    private:
        Device* m_Device = nullptr;
        HandleType m_Handle;
    };

    template<typename ObjectType, typename ImplObjectType>
    class Pool
    {
    private:
        static constexpr uint32_t kListEndSentinel = 0xFFFFFFFF;
        /// TODO seperate out the ImplObjectType and utility values (gen & nextFree)
        /// -> better cache line utility.
        /// Pg 103, Vk3DGRC.
        /// On a personal note, this reminds me of the Linked List Allocator design. We aren't utilizing free space for the linked list,
        /// though that is also an option.
        struct PoolEntry
        {
            explicit PoolEntry(ImplObjectType& obj)
                : obj_(std::move(obj))
            {}
            ImplObjectType obj_;
            uint32_t gen_ = 1;
            /// This maintains the "Linked List" design
            uint32_t _nextFree = kListEndSentinel;
        };
        uint32_t m_FreeListHead = kListEndSentinel;
    public:
        std::vector<PoolEntry> m_Objects;
        uint32_t m_NumObjects = 0;

        [[nodiscard]] Handle<ObjectType> create(ImplObjectType&& obj) noexcept
        {
            uint32_t index = 0;
            if (m_FreeListHead != kListEndSentinel)
            {
                // the pool is not full -> use an existing entry
                index = m_FreeListHead;
                m_FreeListHead = m_Objects[index]._nextFree;
                m_Objects[index].obj_ = std::move(obj);
            }
            else
            {
                // create a new entry into the pool
                index = static_cast<uint32_t>(m_Objects.size());
                m_Objects.emplace_back(obj);
            }

            m_NumObjects++;
            return Handle<ObjectType>{index, m_Objects[index].gen_};
        }

        void destroy(Handle<ObjectType> handle) noexcept
        {
            // error checking here
            if (handle.empty())
            {
                return;
            }
            // TODO consider std::expected here
            assert(m_NumObjects > 0);
            const auto index = handle.index();
            assert(index < m_Objects.size());

            // double deletion
            assert(handle.gen() == m_Objects[index].gen_);

            // places the array element at the front of the free list
            m_Objects[index].obj_ = ImplObjectType{};
            ++m_Objects[index].gen_;
            m_Objects[index]._nextFree = m_FreeListHead;
            m_FreeListHead = index;
            m_NumObjects--;
        }

        [[nodiscard]] ImplObjectType* get(Handle<ObjectType> handle) noexcept
        {
            if (handle.empty())
            {
                return nullptr;
            }
            const auto index = handle.index();
            assert(index < m_Objects.size());
            // accessing a deleted object
            assert(handle.gen() == m_Objects[index].gen_);

            return &m_Objects[index].obj_;
        }

        void clear()
        {
            m_Objects.clear();
            m_FreeListHead = kListEndSentinel;
            m_NumObjects = 0;
        }

        /// Consider std::expected here.
        /// This is unsafe but useful for debugging.
        [[nodiscard]] Handle<ObjectType> getHandle(uint32_t index) const noexcept
        {
            assert(index < m_Objects.size());
            if (index >= m_Objects.size())
            {
                return {};
            }
            return Handle<ObjectType>{index, m_Objects[index].gen_};
        }

        [[nodiscard]] Handle<ObjectType> findObject(const ImplObjectType& obj)
        {
            for (size_t i = 0; i < m_Objects.size(); ++i)
            {
                if (m_Objects[i].obj_ == obj)
                {
                    return Handle<ObjectType>{static_cast<uint32_t>(i), m_Objects[i].gen_};
                }
            }

            return {};
        }

        [[nodiscard]] uint32_t numObjects() const noexcept
        {
            return m_NumObjects;
        }
    };
}
