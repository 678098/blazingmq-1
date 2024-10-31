// Copyright 2024 Bloomberg Finance L.P.
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// mqbi_dispatcher.t.cpp                                              -*-C++-*-
#include <mqbi_dispatcher.h>

// BMQ
#include <bmqu_printutil.h>

// BDE
#include <bsl_cstdlib.h>
#include <bsl_ctime.h>
#include <bsl_functional.h>
#include <bsl_string.h>
#include <bsl_unordered_set.h>
#include <bsl_utility.h>
#include <bsls_timeutil.h>

// TEST DRIVER
#include <bmqtst_testhelper.h>

// CONVENIENCE
using namespace BloombergLP;
using namespace bsl;

// ============================================================================
//                            TEST HELPERS UTILITY
// ----------------------------------------------------------------------------
namespace {

struct Test {
    int  ptr;
    int  a;
    bool b;
};

void increment(size_t* calls)
{
    (*calls)++;
}

struct Dummy : mqbi::CallbackFunctor {
    char data[144];

    void operator()() const BSLS_KEYWORD_OVERRIDE {}
};

static void
printSummary(const bsl::string& desc, bsls::Types::Int64 dt, size_t iters)
{
    bsl::cout << desc << ":" << bsl::endl;
    bsl::cout << "       total: " << bmqu::PrintUtil::prettyTimeInterval(dt)
              << " (" << iters << " iterations)" << bsl::endl;
    bsl::cout << "    per call: "
              << bmqu::PrintUtil::prettyTimeInterval((dt) / iters)
              << bsl::endl;
    bsl::cout << bsl::endl;
}

struct StatsCallback : mqbi::CallbackFunctor {
    size_t& d_constructors;
    size_t& d_destructors;
    size_t& d_calls;

    StatsCallback(size_t& constructors, size_t& destructors, size_t& calls)
    : d_constructors(constructors)
    , d_destructors(destructors)
    , d_calls(calls)
    {
        d_constructors++;
    }

    ~StatsCallback() BSLS_KEYWORD_OVERRIDE { d_destructors++; }

    void operator()() const BSLS_KEYWORD_OVERRIDE { d_calls++; }
};

class ManagedCallbackBuffer BSLS_KEYWORD_FINAL {
  private:
    // DATA
    /// Reusable buffer holding the stored callback.
    char d_callbackBuffer[256];

    /// The flag indicating if `d_callbackBuffer` contains a valid callback
    /// object now.
    bool d_hasCallback;

  public:
    // TRAITS
    BSLMF_NESTED_TRAIT_DECLARATION(ManagedCallbackBuffer,
                                   bslma::UsesBslmaAllocator)

    // CREATORS
    inline explicit ManagedCallbackBuffer(bslma::Allocator* allocator = 0)
    : d_callbackBuffer()
    , d_hasCallback(false)
    {
        // NOTHING
    }

    inline ~ManagedCallbackBuffer() { reset(); }

    // MANIPULATORS
    inline void reset()
    {
        if (d_hasCallback) {
            // Not necessary to resize the vector or memset its elements to 0,
            // we just call the virtual destructor, and `d_hasCallback` flag
            // prevents us from calling outdated callback.
            reinterpret_cast<mqbi::CallbackFunctor*>(d_callbackBuffer)
                ->~CallbackFunctor();
            d_hasCallback = false;
        }
    }

    template <class CALLBACK_TYPE>
    inline char* place()
    {
        // PRECONDITIONS
        BSLS_ASSERT_SAFE(!d_hasCallback);
        /// The compilation will fail here on the outer `static_cast` if we
        /// don't provide a type that is inherited from the base
        /// `CallbackFunctor` type.
        /// TODO: replace by static_assert on C++ standard update
        BSLS_ASSERT_SAFE(0 ==
                         static_cast<CALLBACK_TYPE*>(
                             reinterpret_cast<mqbi::CallbackFunctor*>(0)));

        static_assert(sizeof(CALLBACK_TYPE) <= 128);

        d_hasCallback = true;
        return d_callbackBuffer;
    }

    // ACCESSORS

    inline bool hasCallback() const { return d_hasCallback; }

    inline void operator()() const
    {
        // PRECONDITIONS
        BSLS_ASSERT_SAFE(d_hasCallback);

        (*reinterpret_cast<const mqbi::CallbackFunctor*>(d_callbackBuffer))();
    }
};

}  // close unnamed namespace

// ============================================================================
//                                    TESTS
// ----------------------------------------------------------------------------
static void test1_ManagedCallback()
{
    bmqtst::TestHelper::printTestName("MANAGED CALLBACK");

    {
        mqbi::ManagedCallback callback(bmqtst::TestHelperUtil::allocator());
        BMQTST_ASSERT(!callback.hasCallback());

        callback.setCallback(mqbi::Dispatcher::VoidFunctor());
        BMQTST_ASSERT(callback.hasCallback());
        callback();
    }
    {
        mqbi::DispatcherEvent event(bmqtst::TestHelperUtil::allocator());
        event.setCallback(mqbi::Dispatcher::VoidFunctor());
        event.reset();

        event.callback().place<Dummy>();
    }
}

static void testN1_dispatcherEventPeformance()
// ------------------------------------------------------------------------
// DISPATCHER EVENT PERFORMANCE
//
// Concerns:
//
// Plan:
//
// Testing:
// ------------------------------------------------------------------------
{
    bmqtst::TestHelper::printTestName("DISPATCHER EVENT PERFORMANCE");

    bsl::cout << "sizeof(mqbi::DispatcherEvent): "
              << sizeof(mqbi::DispatcherEvent) << bsl::endl;
    bsl::cout << "  sizeof(mqbi::DispatcherDispatcherEvent): "
              << sizeof(mqbi::DispatcherDispatcherEvent) << bsl::endl;
    bsl::cout << "  sizeof(mqbi::DispatcherCallbackEvent): "
              << sizeof(mqbi::DispatcherCallbackEvent) << bsl::endl;
    bsl::cout << "  sizeof(mqbi::DispatcherControlMessageEvent): "
              << sizeof(mqbi::DispatcherControlMessageEvent) << bsl::endl;
    bsl::cout << "  sizeof(mqbi::DispatcherConfirmEvent): "
              << sizeof(mqbi::DispatcherConfirmEvent) << bsl::endl;
    bsl::cout << "  sizeof(mqbi::DispatcherRejectEvent): "
              << sizeof(mqbi::DispatcherRejectEvent) << bsl::endl;
    bsl::cout << "  sizeof(mqbi::DispatcherPushEvent): "
              << sizeof(mqbi::DispatcherPushEvent) << bsl::endl;
    bsl::cout << "  sizeof(mqbi::DispatcherPutEvent): "
              << sizeof(mqbi::DispatcherPutEvent) << bsl::endl;
    bsl::cout << "  sizeof(mqbi::DispatcherAckEvent): "
              << sizeof(mqbi::DispatcherAckEvent) << bsl::endl;
    bsl::cout << "  sizeof(mqbi::DispatcherClusterStateEvent): "
              << sizeof(mqbi::DispatcherClusterStateEvent) << bsl::endl;
    bsl::cout << "  sizeof(mqbi::DispatcherStorageEvent): "
              << sizeof(mqbi::DispatcherStorageEvent) << bsl::endl;
    bsl::cout << "  sizeof(mqbi::DispatcherRecoveryEvent): "
              << sizeof(mqbi::DispatcherRecoveryEvent) << bsl::endl;
    bsl::cout << "  sizeof(mqbi::DispatcherReceiptEvent): "
              << sizeof(mqbi::DispatcherReceiptEvent) << bsl::endl;

    bsl::cout << "  sizeof(bsl::function<void()>): "
              << sizeof(bsl::function<void()>) << bsl::endl;
    bsl::cout << "  sizeof(bsl::function<void(int)>): "
              << sizeof(bsl::function<void(int)>) << bsl::endl;
    bsl::cout << "  sizeof(Test): " << sizeof(Test) << bsl::endl;
    bsl::cout << "  sizeof(vector): " << sizeof(bsl::vector<char>)
              << bsl::endl;

    const size_t k_ITERS_NUM = 100000000;

    const bmqp::PutHeader                                 header;
    const bsl::shared_ptr<bdlbb::Blob>                    blob;
    const bsl::shared_ptr<BloombergLP::bmqu::AtomicState> state;
    const bmqt::MessageGUID                               guid;
    const bmqp::MessagePropertiesInfo                     info;
    const bmqp::Protocol::SubQueueInfosArray              subQueueInfos;
    const bsl::string                                     msgGroupId;
    const bmqp::ConfirmMessage                            confirm;

    mqbi::DispatcherEvent event(bmqtst::TestHelperUtil::allocator());

    {
        const bsls::Types::Int64 begin = bsls::TimeUtil::getTimer();
        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            event.reset();
        }
        const bsls::Types::Int64 end = bsls::TimeUtil::getTimer();

        bsl::cout << "mqbi::DispatcherEvent::reset():" << bsl::endl;
        bsl::cout << "       total: "
                  << bmqu::PrintUtil::prettyTimeInterval(end - begin) << " ("
                  << k_ITERS_NUM << " iterations)" << bsl::endl;
        bsl::cout << "    per call: "
                  << bmqu::PrintUtil::prettyTimeInterval((end - begin) /
                                                         k_ITERS_NUM)
                  << bsl::endl
                  << bsl::endl;
    }

    {
        const bsls::Types::Int64 begin = bsls::TimeUtil::getTimer();
        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            mqbi::DispatcherEvent event(bmqtst::TestHelperUtil::allocator());
        }
        const bsls::Types::Int64 end = bsls::TimeUtil::getTimer();

        bsl::cout << "mqbi::DispatcherEvent::DispatcherEvent():" << bsl::endl;
        bsl::cout << "       total: "
                  << bmqu::PrintUtil::prettyTimeInterval(end - begin) << " ("
                  << k_ITERS_NUM << " iterations)" << bsl::endl;
        bsl::cout << "    per call: "
                  << bmqu::PrintUtil::prettyTimeInterval((end - begin) /
                                                         k_ITERS_NUM)
                  << bsl::endl
                  << bsl::endl;
    }
}

static void testN2_managedCallbackPerformance()
{
    const size_t k_ITERS_NUM = 10000000;

    size_t constructors = 0;
    size_t destructors  = 0;
    size_t calls        = 0;

    // Warmup
    for (size_t i = 0; i < k_ITERS_NUM; i++) {
        mqbi::ManagedCallback callback(bmqtst::TestHelperUtil::allocator());
        new (callback.place<StatsCallback>())
            StatsCallback(constructors, destructors, calls);
        callback.reset();
    }

    {
        bsl::function<void(void)> callback;
        callback = bdlf::BindUtil::bind(increment, &calls);
        const bsls::Types::Int64 begin = bsls::TimeUtil::getTimer();
        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            callback();
        }
        const bsls::Types::Int64 end = bsls::TimeUtil::getTimer();

        printSummary("bsl::function<...>()", end - begin, k_ITERS_NUM);
    }
    {
        mqbi::ManagedCallback callback(bmqtst::TestHelperUtil::allocator());
        new (callback.place<StatsCallback>())
            StatsCallback(constructors, destructors, calls);

        const bsls::Types::Int64 begin = bsls::TimeUtil::getTimer();
        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            callback();
        }
        const bsls::Types::Int64 end = bsls::TimeUtil::getTimer();

        printSummary("mqbi::ManagedCallback(vector)",
                     end - begin,
                     k_ITERS_NUM);
    }
    {
        ManagedCallbackBuffer callback(bmqtst::TestHelperUtil::allocator());
        new (callback.place<StatsCallback>())
            StatsCallback(constructors, destructors, calls);

        const bsls::Types::Int64 begin = bsls::TimeUtil::getTimer();
        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            callback();
        }
        const bsls::Types::Int64 end = bsls::TimeUtil::getTimer();

        printSummary("mqbi::ManagedCallback(char[])",
                     end - begin,
                     k_ITERS_NUM);
    }
    {
        bsl::vector<bsl::function<void(void)> > cbs;
        cbs.resize(k_ITERS_NUM);

        const bsls::Types::Int64 begin = bsls::TimeUtil::getTimer();
        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            cbs[i] = bdlf::BindUtil::bindS(bmqtst::TestHelperUtil::allocator(),
                                           increment,
                                           &calls);
        }
        const bsls::Types::Int64 end = bsls::TimeUtil::getTimer();

        cbs.clear();
        cbs.shrink_to_fit();

        printSummary("bsl::function<...>() bind assign",
                     end - begin,
                     k_ITERS_NUM);
    }
    {
        bsl::vector<mqbi::ManagedCallback*> cbs;
        cbs.resize(k_ITERS_NUM);

        const bsls::Types::Int64 begin = bsls::TimeUtil::getTimer();
        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            cbs[i] = new mqbi::ManagedCallback(
                bmqtst::TestHelperUtil::allocator());
            new (cbs[i]->place<StatsCallback>())
                StatsCallback(constructors, destructors, calls);
        }
        const bsls::Types::Int64 end = bsls::TimeUtil::getTimer();

        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            cbs[i]->reset();
            delete cbs[i];
        }

        cbs.clear();
        cbs.shrink_to_fit();

        printSummary("mqbi::ManagedCallback(vector) new place",
                     end - begin,
                     k_ITERS_NUM);
    }
    {
        bsl::vector<ManagedCallbackBuffer*> cbs;
        cbs.resize(k_ITERS_NUM);

        const bsls::Types::Int64 begin = bsls::TimeUtil::getTimer();
        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            cbs[i] = new ManagedCallbackBuffer(
                bmqtst::TestHelperUtil::allocator());
            new (cbs[i]->place<StatsCallback>())
                StatsCallback(constructors, destructors, calls);
        }
        const bsls::Types::Int64 end = bsls::TimeUtil::getTimer();

        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            cbs[i]->reset();
            delete cbs[i];
        }

        cbs.clear();
        cbs.shrink_to_fit();

        printSummary("mqbi::ManagedCallback(char[]) new place",
                     end - begin,
                     k_ITERS_NUM);
    }

    {
        bsl::function<void(void)> callback;

        const bsls::Types::Int64 begin = bsls::TimeUtil::getTimer();
        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            callback = bdlf::BindUtil::bindS(
                bmqtst::TestHelperUtil::allocator(),
                increment,
                &calls);
            callback();
        }
        const bsls::Types::Int64 end = bsls::TimeUtil::getTimer();

        printSummary("bsl::function<...>() bind call reuse",
                     end - begin,
                     k_ITERS_NUM);
    }
    {
        mqbi::ManagedCallback callback(bmqtst::TestHelperUtil::allocator());

        const bsls::Types::Int64 begin = bsls::TimeUtil::getTimer();
        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            new (callback.place<StatsCallback>())
                StatsCallback(constructors, destructors, calls);
            callback();
            callback.reset();
        }
        const bsls::Types::Int64 end = bsls::TimeUtil::getTimer();

        printSummary("mqbi::ManagedCallback(vector) set call reuse",
                     end - begin,
                     k_ITERS_NUM);
    }
    {
        ManagedCallbackBuffer callback(bmqtst::TestHelperUtil::allocator());

        const bsls::Types::Int64 begin = bsls::TimeUtil::getTimer();
        for (size_t i = 0; i < k_ITERS_NUM; i++) {
            new (callback.place<StatsCallback>())
                StatsCallback(constructors, destructors, calls);
            callback();
            callback.reset();
        }
        const bsls::Types::Int64 end = bsls::TimeUtil::getTimer();

        printSummary("mqbi::ManagedCallback(char[]) set call reuse",
                     end - begin,
                     k_ITERS_NUM);
    }

    if (calls < 100000) {
        bsl::cout << calls << bsl::endl;
    }
}

// ============================================================================
//                                 MAIN PROGRAM
// ----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    // To be called only once per process instantiation.
    bsls::TimeUtil::initialize();

    TEST_PROLOG(bmqtst::TestHelper::e_DEFAULT);

    switch (_testCase) {
    case 0:
    case 1: test1_ManagedCallback(); break;
    case -1: testN1_dispatcherEventPeformance(); break;
    case -2: testN2_managedCallbackPerformance(); break;
    default: {
        bsl::cerr << "WARNING: CASE '" << _testCase << "' NOT FOUND."
                  << bsl::endl;
        bmqtst::TestHelperUtil::testStatus() = -1;
    } break;
    }

    TEST_EPILOG(bmqtst::TestHelper::e_CHECK_DEF_GBL_ALLOC);
}
