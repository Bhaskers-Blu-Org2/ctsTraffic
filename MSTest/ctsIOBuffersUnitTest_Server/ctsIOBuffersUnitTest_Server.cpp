/*

Copyright (c) Microsoft Corporation
All rights reserved.

Licensed under the Apache License, Version 2.0 (the ""License""); you may not use this file except in compliance with the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0

THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.

See the Apache Version 2.0 License for specific language governing permissions and limitations under the License.

*/

#include <SDKDDKVer.h>
#include "CppUnitTest.h"

#include <vector>
#include <algorithm>

#include <wil/resource.h>

#include <ctString.hpp>

#include "ctsIOBuffers.hpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

ctsTraffic::ctsUnsignedLongLong s_TransferSize = 0ULL;
bool s_Listening = true;
///
/// Fakes
///
namespace ctsTraffic::ctsConfig
{
    ctsConfigSettings* Settings;

    void PrintConnectionResults(const ctl::ctSockaddr&, const ctl::ctSockaddr&, unsigned long) noexcept
    {
    }
    void PrintConnectionResults(const ctl::ctSockaddr&, const ctl::ctSockaddr&, unsigned long, const ctsTcpStatistics&) noexcept
    {
    }
    void PrintConnectionResults(const ctl::ctSockaddr&, const ctl::ctSockaddr&, unsigned long, const ctsUdpStatistics&) noexcept
    {
    }
    void PrintDebug(_In_z_ _Printf_format_string_ PCWSTR, ...) noexcept
    {
    }
    void PrintException(const std::exception&) noexcept
    {
    }
    void PrintErrorInfo(_In_ PCSTR) noexcept
    {
    }

    bool IsListening() noexcept
    {
        return s_Listening;
    }

    ctsUnsignedLongLong GetTransferSize() noexcept
    {
        return s_TransferSize;
    }
    bool ShutdownCalled() noexcept
    {
        return false;
    }
    unsigned long ConsoleVerbosity() noexcept
    {
        return 0;
    }
}

///
/// End of Fakes
///

using namespace ctsTraffic;
namespace ctsUnitTest
{
    TEST_CLASS(ctsIOBuffersUnitTest_Server)
    {
    private:
        ctsTcpStatistics stats;

    public:
        TEST_CLASS_INITIALIZE(Setup)
        {
            ctsConfig::Settings = new ctsConfig::ctsConfigSettings;
            ctsConfig::Settings->Protocol = ctsConfig::ProtocolType::TCP;
            statics::ServerConnectionGrowthRate = 10; // changing the growth rate so that it's easier to test (we don't want to test growing every 4096)
        }

        TEST_CLASS_CLEANUP(Cleanup)
        {
            delete ctsConfig::Settings;
        }

        TEST_METHOD(RequestAndReturnOneConnection)
        {
            ctsIOTask test_task;
            auto return_test_task = wil::scope_exit([&]() { ctsIOBuffers::ReleaseConnectionIdBuffer(test_task); });

            test_task = ctsIOBuffers::NewConnectionIdBuffer(stats.connection_identifier);
            Assert::AreEqual(ctsStatistics::ConnectionIdLength, test_task.buffer_length);
            Assert::IsNotNull(test_task.buffer);
            Assert::AreEqual(0UL, test_task.buffer_offset);

            return_test_task.reset();

            ctsIOTask test_task_second;
            auto return_test_task_second = wil::scope_exit([&]() { ctsIOBuffers::ReleaseConnectionIdBuffer(test_task_second); });
            test_task_second = ctsIOBuffers::NewConnectionIdBuffer(stats.connection_identifier);
            Assert::AreEqual(test_task_second.buffer, test_task.buffer);

            // scope guard will return the buffers
        }

        TEST_METHOD(RequestAndReturnAllConnections)
        {
            std::vector<ctsIOTask> test_tasks;
            auto return_test_tasks = wil::scope_exit([&]() {
                for (auto& task : test_tasks)
                {
                    ctsIOBuffers::ReleaseConnectionIdBuffer(task);
                }
            });

            for (unsigned long add_tasks = 0; add_tasks < statics::ServerConnectionGrowthRate; ++add_tasks)
            {
                test_tasks.push_back(ctsIOBuffers::NewConnectionIdBuffer(stats.connection_identifier));
            }
            for (auto& task : test_tasks)
            {
                Assert::AreEqual(ctsStatistics::ConnectionIdLength, task.buffer_length);
                Assert::IsNotNull(task.buffer);
                Assert::AreEqual(0UL, task.buffer_offset);
            }

            // return the buffers back via the scope guard
            return_test_tasks.reset();

            std::vector<ctsIOTask> test_tasks_second;
            auto return_test_tasks_second = wil::scope_exit([&]() {
                for (auto& task : test_tasks_second)
                {
                    ctsIOBuffers::ReleaseConnectionIdBuffer(task);
                }
            });

            for (unsigned long add_tasks = 0; add_tasks < statics::ServerConnectionGrowthRate; ++add_tasks)
            {
                test_tasks_second.push_back(ctsIOBuffers::NewConnectionIdBuffer(stats.connection_identifier));
            }

            // since I returned the first buffers in reverse order, they'll be reversed here
            auto iter_first_task = test_tasks.rbegin();
            for (auto& task : test_tasks_second)
            {
                Assert::AreEqual(iter_first_task->buffer, task.buffer);
                ++iter_first_task;
            }

            // scope guard will return the buffers
        }

        TEST_METHOD(RequestAndReturnDoubleGrowthRate)
        {
            std::vector<ctsIOTask> test_tasks;
            auto return_test_tasks = wil::scope_exit([&]() {
                for (auto& task : test_tasks)
                {
                    ctsIOBuffers::ReleaseConnectionIdBuffer(task);
                }
            });

            for (unsigned long add_tasks = 0; add_tasks < statics::ServerConnectionGrowthRate; ++add_tasks)
            {
                test_tasks.push_back(ctsIOBuffers::NewConnectionIdBuffer(stats.connection_identifier));
            }
            for (auto& task : test_tasks)
            {
                Assert::AreEqual(ctsStatistics::ConnectionIdLength, task.buffer_length);
                Assert::IsNotNull(task.buffer);
                Assert::AreEqual(0UL, task.buffer_offset);
            }

            std::vector<ctsIOTask> test_tasks_second;
            auto return_test_tasks_second = wil::scope_exit([&]() {
                for (auto& task : test_tasks_second)
                {
                    ctsIOBuffers::ReleaseConnectionIdBuffer(task);
                }
            });

            for (unsigned long add_tasks = 0; add_tasks < statics::ServerConnectionGrowthRate; ++add_tasks)
            {
                test_tasks_second.push_back(ctsIOBuffers::NewConnectionIdBuffer(stats.connection_identifier));
            }
            for (auto& task : test_tasks)
            {
                Assert::AreEqual(ctsStatistics::ConnectionIdLength, task.buffer_length);
                Assert::IsNotNull(task.buffer);
                Assert::AreEqual(0UL, task.buffer_offset);
            }

            // scope guard will return the buffers
        }

        TEST_METHOD(RequestAndReturnOnePageOfBuffers)
        {
            ::SYSTEM_INFO sysInfo;         // Useful information about the system
            ::GetSystemInfo(&sysInfo);     // Initialize the structure.
            const unsigned long buffersToAdd = (sysInfo.dwPageSize / ctsStatistics::ConnectionIdLength);

            std::vector<char*> test_buffers;
            std::vector<ctsIOTask> test_tasks;
            auto return_test_tasks = wil::scope_exit([&]() {
                for (auto& task : test_tasks)
                {
                    ctsIOBuffers::ReleaseConnectionIdBuffer(task);
                }
            });

            for (unsigned long add_tasks = 0; add_tasks < buffersToAdd; ++add_tasks)
            {
                ctsIOTask temp_task = ctsIOBuffers::NewConnectionIdBuffer(stats.connection_identifier);
                test_tasks.push_back(temp_task);
                test_buffers.push_back(temp_task.buffer);
            }
            for (auto& task : test_tasks)
            {
                Assert::AreEqual(ctsStatistics::ConnectionIdLength, task.buffer_length);
                Assert::IsNotNull(task.buffer);
                Assert::AreEqual(0UL, task.buffer_offset);
            }

            std::sort(test_buffers.begin(), test_buffers.end());
            if (std::adjacent_find(test_buffers.begin(), test_buffers.end()) != test_buffers.end())
            {
                Assert::Fail(L"The same buffer was given to 2 different ctsIOTasks");
            }

            return_test_tasks.reset();
            test_tasks.clear();
            test_buffers.clear();


            auto return_test_tasks2 = wil::scope_exit([&]() {
                for (auto& task : test_tasks)
                {
                    ctsIOBuffers::ReleaseConnectionIdBuffer(task);
                }
            });

            for (unsigned long add_tasks = 0; add_tasks < buffersToAdd; ++add_tasks)
            {
                ctsIOTask temp_task = ctsIOBuffers::NewConnectionIdBuffer(stats.connection_identifier);
                test_tasks.push_back(temp_task);
                test_buffers.push_back(temp_task.buffer);
            }
            for (auto& task : test_tasks)
            {
                Assert::AreEqual(ctsStatistics::ConnectionIdLength, task.buffer_length);
                Assert::IsNotNull(task.buffer);
                Assert::AreEqual(0UL, task.buffer_offset);
            }

            std::sort(test_buffers.begin(), test_buffers.end());
            if (std::adjacent_find(test_buffers.begin(), test_buffers.end()) != test_buffers.end())
            {
                Assert::Fail(L"The same buffer was given to 2 different ctsIOTasks");
            }

            return_test_tasks2.reset();
        }

        TEST_METHOD(RequestAndReturnOverOnePageOfBuffers)
        {
            ::SYSTEM_INFO sysInfo;         // Useful information about the system
            ::GetSystemInfo(&sysInfo);     // Initialize the structure.
            const unsigned long buffersToAdd = (sysInfo.dwPageSize / ctsStatistics::ConnectionIdLength) + 1;

            std::vector<char*> test_buffers;
            std::vector<ctsIOTask> test_tasks;
            auto return_test_tasks = wil::scope_exit([&]() {
                for (auto& task : test_tasks)
                {
                    ctsIOBuffers::ReleaseConnectionIdBuffer(task);
                }
            });

            for (unsigned long add_tasks = 0; add_tasks < buffersToAdd; ++add_tasks)
            {
                ctsIOTask temp_task = ctsIOBuffers::NewConnectionIdBuffer(stats.connection_identifier);
                test_tasks.push_back(temp_task);
                test_buffers.push_back(temp_task.buffer);
            }
            for (auto& task : test_tasks)
            {
                Assert::AreEqual(ctsStatistics::ConnectionIdLength, task.buffer_length);
                Assert::IsNotNull(task.buffer);
                Assert::AreEqual(0UL, task.buffer_offset);
            }

            std::sort(test_buffers.begin(), test_buffers.end());
            if (std::adjacent_find(test_buffers.begin(), test_buffers.end()) != test_buffers.end())
            {
                Assert::Fail(L"The same buffer was given to 2 different ctsIOTasks");
            }

            return_test_tasks.reset();
            test_tasks.clear();
            test_buffers.clear();


            auto return_test_tasks2 = wil::scope_exit([&]() {
                for (auto& task : test_tasks)
                {
                    ctsIOBuffers::ReleaseConnectionIdBuffer(task);
                }
            });

            for (unsigned long add_tasks = 0; add_tasks < buffersToAdd; ++add_tasks)
            {
                ctsIOTask temp_task = ctsIOBuffers::NewConnectionIdBuffer(stats.connection_identifier);
                test_tasks.push_back(temp_task);
                test_buffers.push_back(temp_task.buffer);
            }
            for (auto& task : test_tasks)
            {
                Assert::AreEqual(ctsStatistics::ConnectionIdLength, task.buffer_length);
                Assert::IsNotNull(task.buffer);
                Assert::AreEqual(0UL, task.buffer_offset);
            }

            std::sort(test_buffers.begin(), test_buffers.end());
            if (std::adjacent_find(test_buffers.begin(), test_buffers.end()) != test_buffers.end())
            {
                Assert::Fail(L"The same buffer was given to 2 different ctsIOTasks");
            }

            return_test_tasks2.reset();
        }

        TEST_METHOD(RequestAndReturnOverTwoPagesOfBuffers)
        {
            ::SYSTEM_INFO sysInfo;         // Useful information about the system
            ::GetSystemInfo(&sysInfo);     // Initialize the structure.
            const unsigned long buffersToAdd = ((sysInfo.dwPageSize / ctsStatistics::ConnectionIdLength) + 1) * 2;

            std::vector<char*> test_buffers;
            std::vector<ctsIOTask> test_tasks;
            auto return_test_tasks = wil::scope_exit([&]() {
                for (auto& task : test_tasks)
                {
                    ctsIOBuffers::ReleaseConnectionIdBuffer(task);
                }
            });

            for (unsigned long add_tasks = 0; add_tasks < buffersToAdd; ++add_tasks)
            {
                ctsIOTask temp_task = ctsIOBuffers::NewConnectionIdBuffer(stats.connection_identifier);
                test_tasks.push_back(temp_task);
                test_buffers.push_back(temp_task.buffer);
            }
            for (auto& task : test_tasks)
            {
                Assert::AreEqual(ctsStatistics::ConnectionIdLength, task.buffer_length);
                Assert::IsNotNull(task.buffer);
                Assert::AreEqual(0UL, task.buffer_offset);
            }

            std::sort(test_buffers.begin(), test_buffers.end());
            if (std::adjacent_find(test_buffers.begin(), test_buffers.end()) != test_buffers.end())
            {
                Assert::Fail(L"The same buffer was given to 2 different ctsIOTasks");
            }

            return_test_tasks.reset();
            test_tasks.clear();
            test_buffers.clear();


            auto return_test_tasks2 = wil::scope_exit([&]() {
                for (auto& task : test_tasks)
                {
                    ctsIOBuffers::ReleaseConnectionIdBuffer(task);
                }
            });

            for (unsigned long add_tasks = 0; add_tasks < buffersToAdd; ++add_tasks)
            {
                ctsIOTask temp_task = ctsIOBuffers::NewConnectionIdBuffer(stats.connection_identifier);
                test_tasks.push_back(temp_task);
                test_buffers.push_back(temp_task.buffer);
            }
            for (auto& task : test_tasks)
            {
                Assert::AreEqual(ctsStatistics::ConnectionIdLength, task.buffer_length);
                Assert::IsNotNull(task.buffer);
                Assert::AreEqual(0UL, task.buffer_offset);
            }

            std::sort(test_buffers.begin(), test_buffers.end());
            if (std::adjacent_find(test_buffers.begin(), test_buffers.end()) != test_buffers.end())
            {
                Assert::Fail(L"The same buffer was given to 2 different ctsIOTasks");
            }

            return_test_tasks2.reset();
        }
    };
}