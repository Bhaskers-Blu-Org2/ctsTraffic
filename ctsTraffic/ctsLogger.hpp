/*

Copyright (c) Microsoft Corporation
All rights reserved.

Licensed under the Apache License, Version 2.0 (the ""License""); you may not use this file except in compliance with the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0

THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.

See the Apache Version 2.0 License for specific language governing permissions and limitations under the License.

*/

#pragma once

// cpp headers
#include <memory>
// os headers
#include <windows.h>
// wil headers
#include <wil/resource.h>
// ctl headers
#include <ctException.hpp>
#include <ctString.hpp>
// project headers
#include "ctsConfig.h"
#include "ctsPrintStatus.hpp"

namespace ctsTraffic
{
////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// Base class for all ctsTraffic Loggers
///
///
/// - all concrete types must implement:
///     message_impl(PCWSTR)
///     error_impl(PCWSTR)
///
///   Note: all logging functions are no-throw
///         only the c'tor can throw
///
////////////////////////////////////////////////////////////////////////////////////////////////////

    class ctsLogger
    {
    public:
        explicit ctsLogger(ctsConfig::StatusFormatting _format) noexcept :
            format(_format)
        {
        }
        virtual ~ctsLogger() noexcept
            = default;

        void LogLegend(const std::shared_ptr<ctsStatusInformation>& _status_info) noexcept
        {
            PCWSTR const message = _status_info->print_legend(this->format);
            if (message != nullptr)
            {
                log_message_impl(message);
            }
        }

        void LogHeader(const std::shared_ptr<ctsStatusInformation>& _status_info) noexcept
        {
            PCWSTR const message = _status_info->print_header(this->format);
            if (message != nullptr)
            {
                log_message_impl(message);
            }
        }

        void LogStatus(const std::shared_ptr<ctsStatusInformation>& _status_info, long long _current_time, bool _clear_status) noexcept
        {
            PCWSTR const message = _status_info->print_status(this->format, _current_time, _clear_status);
            if (message != nullptr)
            {
                log_message_impl(message);
            }
        }

        void LogMessage(PCWSTR _message) noexcept
        {
            log_message_impl(_message);
        }

        void LogError(PCWSTR _message) noexcept
        {
            log_error_impl(_message);
        }

        bool IsCsvFormat() const noexcept
        {
            return ctsConfig::StatusFormatting::Csv == this->format;
        }

        // not copyable
        ctsLogger(const ctsLogger&) = delete;
        ctsLogger& operator=(const ctsLogger&) = delete;

    private:
        ctsConfig::StatusFormatting format;

        /// pure virtual methods concrete classes must implement
        virtual void log_message_impl(PCWSTR _message) noexcept = 0;
        virtual void log_error_impl(PCWSTR _message) noexcept = 0;
    };

    class ctsTextLogger : public ctsLogger
    {
    public:
        ctsTextLogger(PCWSTR _file_name, ctsConfig::StatusFormatting _format) :
            ctsLogger(_format)
        {
            file_handle.reset(CreateFileW(
                _file_name,
                GENERIC_WRITE,
                FILE_SHARE_READ, // allow others to read the file while we write to it
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr));
            if (!file_handle)
            {
                const auto gle = GetLastError();
                throw ctl::ctException(
                    gle,
                    ctl::ctString::ctFormatString(L"CreateFile(%ws)", _file_name).c_str(),
                    L"ctsTextLogger",
                    true);
            }

            // write the UTF16 Byte order mark
            constexpr WCHAR bom_utf16 = 0xFEFF;
            DWORD bytesWritten{};
            if (!WriteFile(
                file_handle.get(),
                &bom_utf16,
                static_cast<DWORD>(sizeof WCHAR),
                &bytesWritten,
                nullptr))
            {
                const auto gle = GetLastError();
                throw ctl::ctException(gle, L"WriteFile", L"ctsTextLogger", false);
            }
        }
        ~ctsTextLogger() noexcept = default;

        void log_message_impl(PCWSTR _message) noexcept override
        {
            write_impl(_message);
        }

        void log_error_impl(PCWSTR _message) noexcept override
        {
            write_impl(_message);
        }

        ctsTextLogger(const ctsTextLogger&) = delete;
        ctsTextLogger& operator=(const ctsTextLogger&) = delete;
        ctsTextLogger(ctsTextLogger&&) = delete;
        ctsTextLogger& operator=(ctsTextLogger&&) = delete;

    private:
        wil::critical_section file_cs;
        wil::unique_hfile file_handle;

        void write_impl(PCWSTR _message) noexcept
        {
            const auto lock = file_cs.lock();
            DWORD bytesWritten{};
            if (!WriteFile(
                file_handle.get(),
                _message,
                static_cast<DWORD>(wcslen(_message) * sizeof(WCHAR)),
                &bytesWritten,
                nullptr))
            {
                const auto gle = GetLastError();
                ctsConfig::PrintException(
                    ctl::ctException(gle, L"WriteFile", L"ctsTextLogger", false));
            }
        }
    };

} // namespace
