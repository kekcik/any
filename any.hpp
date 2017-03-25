//
//  any.hpp
//  any
//
//  Created by Ivan Trofimov on 22.03.17.
//  Copyright © 2017 Ivan Trofimov. All rights reserved.
//

#include <cstdio>
#include <utility>
#include <cstdio>
#include <type_traits>
#include <cstdlib>

namespace my_any {
    template <typename T>
    static void get_copier(void* dst, void const* src) {
        new(dst) T(*(T const*) src);
    }
    
    template <typename T>
    static void get_big_deleter(void *raw) {
        delete((T*) raw);
    }
    
    template <typename T>
    static void get_small_deleter(void *raw) {
        ((T*) raw) -> ~T();
    }
    
    template <typename T>
    static void get_mover(void *dst, void *src) {
        new(dst) T(std::move(*(T*) src));
    }
    
    template <typename T>
    static void* get_allocator() {
        return new typename std::aligned_storage < sizeof(T), alignof(T) > ::type;
    }
}

struct any {
    any() {
        status = 0;
        storage = {};
        deleter = [](void*) {};
        copier = [](void*, void const*) {};
        mover = [](void*, void*){};
        allocator = []() -> void* {return nullptr;}; {};
    }
    
    any(any const &other) {
        status = other.status;
        deleter = other.deleter;
        copier = other.copier;
        mover = other.mover;
        allocator = other.allocator;
        if (status == 1) {
            copier(&storage, &other.storage);
        }
        else if (status == 2) {
            void *tmp = allocator();
            copier(tmp, *(void**) &other.storage);
            *(void**) & storage = tmp;
        }
    }
    
    any(any &&other) {
        status = other.status;
        deleter = other.deleter;
        copier = other.copier;
        mover = other.mover;
        allocator = other.allocator;
        if (status == 1) {
            mover(&storage, &other.storage);
        } else if (status == 2) {
            void * tmp = allocator();
            mover(tmp, *(void**) & other.storage);
        }
    }
    
    template < typename T, typename TMP = typename std::enable_if <!std::is_same <std::decay_t<T>, any>::value>::type>
    any(T&& val) {
        using T_decay_t = std::decay_t <T> ;
        if (sizeof(T_decay_t) > SMALL_SIZE) {
            status = 2;
            void* tmp = new T_decay_t(std::forward <T> (val));
            *(void**)& storage = tmp;
            deleter = my_any::get_big_deleter <T_decay_t> ;
        } else {
            status = 1;
            new(&storage) T_decay_t(std::forward <T> (val));
            deleter = my_any::get_small_deleter <T_decay_t> ;
        }
        copier = my_any::get_copier <T_decay_t>;
        mover = my_any::get_mover <T_decay_t>;
        allocator = my_any::get_allocator <T_decay_t>;
    }
    
    any& operator = (any const &other) {
        any(other).swap(*this);
        return *this;
    }
    
    any& operator = (any &&other) {
        any(std::move(other)).swap(*this);
        return *this;
    }
    
    template <typename T, typename TMP = typename std::enable_if <!std::is_same <std::decay_t<T>, any>::value>::type>
    any& operator = (T&& other) {
        any(std::forward <T> (other)).swap(*this);
        return *this;
    }
    
    ~any() {
        clear();
    }
    
    void swap(any& other) {
        std::aligned_storage<SMALL_SIZE, SMALL_SIZE>::type swap_var;
        if (status == 1 && other.status == 1) {
            mover           (&swap_var, &storage);
            deleter         (&storage);
            other.mover     (&storage, &other.storage);
            other.deleter   (&other.storage);
            mover           (&other.storage, &swap_var);
            deleter         (&swap_var);
        } else if (status == 1 && other.status == 2) {
            mover           (&swap_var, &storage);
            deleter         (&storage);
            std::swap       (*(void**)&storage, *(void**)&other.storage);
            mover           (&other.storage, &swap_var);
            deleter         (&swap_var);
        } else if (status == 2 && other.status == 1) {
            std::aligned_storage<SMALL_SIZE, SMALL_SIZE>::type swap_var;
            other.mover     (&swap_var, &other.storage);
            other.deleter   (&storage);
            std::swap       (*(void**)&other.storage, *(void**)&storage);
            other.mover     (&storage, &swap_var);
            other.deleter   (&swap_var);
        } else if (status == 2 && other.status == 2) {
            std::swap(*(void**)&storage, *(void**)&other.storage);
        }
        std::swap(status, other.status);
        std::swap(deleter, other.deleter);
        std::swap(copier, other.copier);
        std::swap(mover, other.mover);
        std::swap(allocator, other.allocator);
    }
    
    template <typename T>
    friend T any_cast(const any& _any);
    
    template <typename T>
    friend T any_cast(any& _any);
    
    template <typename T>
    friend T any_cast(any&& _any);
    
    template <typename T>
    friend T
    const* any_cast(any const* _any);
    
    template <typename T>
    friend T* any_cast(any* _any);
    
private:
    static constexpr size_t SMALL_SIZE = 16;
    size_t status;
    
    std::aligned_storage <SMALL_SIZE, SMALL_SIZE> ::type storage;
    
    void(*deleter) (void*);
    void(*copier) (void*, void const*);
    void(*mover) (void*, void*);
    void*(*allocator) (void);
    
    void clear() {
        switch (status) {
            case 1:
                deleter(& storage);
                status = 0;
                break;
            case 2:
                deleter(*(void**) & storage);
                status = 0;
                break;
            default:
                break;
        }
    }
    
    void* get_storage() const {
        switch (status) {
            case 0:
                return nullptr;
            case 1:
                return (void*) & storage;
            default:
                return *(void**) & storage;
        }
    }
};

template <typename T >
T any_cast(const any &_any) {
    void * result = _any.get_storage();
    return *(std::add_const_t < std::remove_reference_t <T>> * ) result;
}

template <typename T>
T any_cast(any &_any) {
    void * result = _any.get_storage();
    return *(std::remove_reference_t <T> * ) result;
}

template <typename T>
T any_cast(any &&_any) {
    void * result = _any.get_storage();
    return *(std::remove_reference_t <T> * ) result;
}

template <typename T>
T const * any_cast(any const *_any) {
    const T * result = _any -> get_storage();
    return result;
}

template <typename T>
T * any_cast(any *_any) {
    T * result = _any -> get_storage();
    return result;
}
