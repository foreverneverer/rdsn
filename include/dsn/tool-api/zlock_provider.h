/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Description:
 *     lock abstraction
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#pragma once

#include <dsn/utility/extensible_object.h>

namespace dsn {
namespace service {
class zlock;
class zrwlock_nr;
class zsemaphore;
}
}

namespace dsn {

/*!
@addtogroup tool-api-providers
@{
*/

/*! lock interface */
class ilock
{
public:
    virtual ~ilock() {}
    virtual void lock() = 0;
    virtual bool try_lock() = 0;
    virtual void unlock() = 0;
};

/*!
 recursive exclusive lock
*/
class lock_provider : public ilock, public extensible_object<lock_provider, 4>
{
public:
    template <typename T>
    static lock_provider *create(lock_provider *inner_provider)
    {
        return new T(inner_provider);
    }
    typedef lock_provider *(*factory)(lock_provider *);

public:
    lock_provider(lock_provider *inner_provider) { _inner_provider = inner_provider; }
    virtual ~lock_provider()
    {
        if (nullptr != _inner_provider)
            delete _inner_provider;
    }
    lock_provider *get_inner_provider() const { return _inner_provider; }

private:
    lock_provider *_inner_provider;
};

/*!
 non-recursive exclusive lock
*/
class lock_nr_provider : public ilock, public extensible_object<lock_nr_provider, 4>
{
public:
    template <typename T>
    static lock_nr_provider *create(lock_nr_provider *inner_provider)
    {
        return new T(inner_provider);
    }

    typedef lock_nr_provider *(*factory)(lock_nr_provider *);

public:
    lock_nr_provider(lock_nr_provider *inner_provider) { _inner_provider = inner_provider; }
    virtual ~lock_nr_provider()
    {
        if (nullptr != _inner_provider)
            delete _inner_provider;
    }
    lock_nr_provider *get_inner_provider() const { return _inner_provider; }

private:
    lock_nr_provider *_inner_provider;
};

/*!
 non-recursive rwlock
*/
class rwlock_nr_provider : public extensible_object<rwlock_nr_provider, 4>
{
public:
    template <typename T>
    static rwlock_nr_provider *create(rwlock_nr_provider *inner_provider)
    {
        return new T(inner_provider);
    }

    typedef rwlock_nr_provider *(*factory)(rwlock_nr_provider *);

public:
    rwlock_nr_provider(rwlock_nr_provider *inner_provider) { _inner_provider = inner_provider; }
    virtual ~rwlock_nr_provider()
    {
        if (nullptr != _inner_provider)
            delete _inner_provider;
    }

    virtual void lock_read() = 0;
    virtual void unlock_read() = 0;
    virtual bool try_lock_read() = 0;

    virtual void lock_write() = 0;
    virtual void unlock_write() = 0;
    virtual bool try_lock_write() = 0;

    rwlock_nr_provider *get_inner_provider() const { return _inner_provider; }

private:
    rwlock_nr_provider *_inner_provider;
};

/*!
 semaphore
*/
class semaphore_provider : public extensible_object<semaphore_provider, 4>
{
public:
    template <typename T>
    static semaphore_provider *create(int initCount, semaphore_provider *inner_provider)
    {
        return new T(initCount, inner_provider);
    }

    typedef semaphore_provider *(*factory)(int, semaphore_provider *);

public:
    semaphore_provider(int initial_count, semaphore_provider *inner_provider)
    {
        _inner_provider = inner_provider;
    }
    virtual ~semaphore_provider()
    {
        if (nullptr != _inner_provider)
            delete _inner_provider;
    }

public:
    virtual void signal(int count) = 0;
    virtual bool wait(int timeout_milliseconds = TIME_MS_MAX) = 0;

    semaphore_provider *get_inner_provider() const { return _inner_provider; }

private:
    semaphore_provider *_inner_provider;
};
/*@}*/
} // end namespace
