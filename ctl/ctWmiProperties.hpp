/*

Copyright (c) Microsoft Corporation
All rights reserved.

Licensed under the Apache License, Version 2.0 (the ""License""); you may not use this file except in compliance with the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0

THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.

See the Apache Version 2.0 License for specific language governing permissions and limitations under the License.

*/

#pragma once

// cpp headers
#include <iterator>
#include <exception>
#include <stdexcept>
#include <utility>
// os headers
#include <windows.h>
#include <OleAuto.h>
#include <Wbemidl.h>
// wil headers
#include <wil/resource.h>
#include <wil/com.h>
// local headers
#include "ctWmiService.hpp"

namespace ctl
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    /// class ctWmiProperties
    ///
    /// Exposes enumerating properties of a WMI Provider through an iterator interface.
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    class ctWmiProperties
    {
    private:
        ctWmiService m_wbemServices;
        wil::com_ptr<IWbemClassObject> m_wbemClass;

    public:
        // ReSharper disable once CppInconsistentNaming
        class iterator;

        ctWmiProperties(ctWmiService service, wil::com_ptr<IWbemClassObject> classObject) :
            m_wbemServices(std::move(service)),
            m_wbemClass(std::move(classObject))
        {
        }

        ctWmiProperties(ctWmiService service, PCWSTR className) :
            m_wbemServices(std::move(service))
        {
            THROW_IF_FAILED(m_wbemServices->GetObjectW(
                wil::make_bstr(className).get(),
                0,
                nullptr,
                m_wbemClass.put(),
                nullptr));
        }

        ctWmiProperties(ctWmiService service, const BSTR& className)
            : m_wbemServices(std::move(service))
        {
            THROW_IF_FAILED(m_wbemServices->GetObjectW(
                className,
                0,
                nullptr,
                m_wbemClass.put(),
                nullptr));
        }

        [[nodiscard]] iterator begin(const bool nonSystemPropertiesOnly = true) const
        {
            return iterator(m_wbemClass, nonSystemPropertiesOnly);
        }

        // ReSharper disable once CppMemberFunctionMayBeStatic
        [[nodiscard]] iterator end() const noexcept
        {
            return iterator();
        }

        // A forward iterator to enable forward-traversing instances of the queried WMI provider
        // ReSharper disable once CppInconsistentNaming
        class iterator
        {
            const unsigned END_ITERATOR_INDEX = 0xffffffff;

            wil::com_ptr<IWbemClassObject> m_wbemClassObject;
            wil::shared_bstr m_propertyName;
            CIMTYPE m_propertyType = 0;
            unsigned m_index = END_ITERATOR_INDEX;

        public:
            // Iterator requires the caller's IWbemServices interface and class name
            iterator() noexcept = default;

            iterator(wil::com_ptr<IWbemClassObject> classObject, const bool nonSystemPropertiesOnly) :
                m_wbemClassObject(std::move(classObject)), m_index(0)
            {
                THROW_IF_FAILED(m_wbemClassObject->BeginEnumeration(nonSystemPropertiesOnly ? WBEM_FLAG_NONSYSTEM_ONLY : 0));
                increment();
            }

            ~iterator() noexcept = default;
            iterator(const iterator&) noexcept = default;
            iterator(iterator&&) noexcept = default;

            iterator& operator =(const iterator&) noexcept = delete;
            iterator& operator =(iterator&&) noexcept = delete;

            void swap(_Inout_ iterator& rhs) noexcept
            {
                using std::swap;
                swap(m_index, rhs.m_index);
                swap(m_wbemClassObject, rhs.m_wbemClassObject);
                swap(m_propertyName, rhs.m_propertyName);
                swap(m_propertyType, rhs.m_propertyType);
            }

            wil::shared_bstr operator*() const
            {
                if (m_index == END_ITERATOR_INDEX)
                {
                    throw std::out_of_range("ctWmiProperties::iterator::operator - invalid subscript");
                }
                return m_propertyName;
            }
            const wil::shared_bstr* operator->() const
            {
                if (m_index == END_ITERATOR_INDEX)
                {
                    throw std::out_of_range("ctWmiProperties::iterator::operator-> - invalid subscript");
                }
                return &m_propertyName;
            }

            [[nodiscard]] CIMTYPE type() const
            {
                if (m_index == END_ITERATOR_INDEX)
                {
                    throw std::out_of_range("ctWmiProperties::iterator::type - invalid subscript");
                }
                return m_propertyType;
            }

            bool operator==(const iterator& iter) const noexcept
            {
                if (m_index != END_ITERATOR_INDEX)
                {
                    return m_index == iter.m_index &&
                        m_wbemClassObject == iter.m_wbemClassObject;
                }
                return m_index == iter.m_index;
            }
            bool operator!=(const iterator& iter) const noexcept
            {
                return !(*this == iter);
            }

            // preincrement
            iterator& operator++()
            {
                increment();
                return *this;
            }

            // postincrement
            iterator operator++(int)
            {
                iterator temp(*this);
                increment();
                return temp;
            }

            // increment by integer
            iterator& operator+=(DWORD _inc)
            {
                for (unsigned loop = 0; loop < _inc; ++loop)
                {
                    increment();
                    if (m_index == END_ITERATOR_INDEX)
                    {
                        throw std::out_of_range("ctWmiProperties::iterator::operator+= - invalid subscript");
                    }
                }
                return *this;
            }

            // iterator_traits (allows <algorithm> functions to be used)
            typedef std::forward_iterator_tag iterator_category;
            typedef wil::shared_bstr value_type;
            typedef int difference_type;
            typedef wil::shared_bstr* pointer;
            typedef wil::shared_bstr& reference;

        private:
            void increment()
            {
                if (m_index == END_ITERATOR_INDEX)
                {
                    throw std::out_of_range("ctWmiProperties::iterator - cannot increment: at the end");
                }

                CIMTYPE next_cimtype;
                wil::shared_bstr next_name;
                const auto hr = THROW_IF_FAILED(m_wbemClassObject->Next(
                    0,
                    next_name.addressof(),
                    nullptr,
                    &next_cimtype,
                    nullptr));
                switch (hr)
                {
                    case WBEM_S_NO_ERROR:
                    {
                        // update the instance members
                        ++m_index;
                        using std::swap;
                        swap(m_propertyName, next_name);
                        swap(m_propertyType, next_cimtype);
                        break;
                    }

                    case WBEM_S_NO_MORE_DATA:
                    {
                        // at the end...
                        m_index = END_ITERATOR_INDEX;
                        m_propertyName.reset();
                        m_propertyType = 0;
                        break;
                    }

                    default: FAIL_FAST();
                }
            }
        };
    };
} // namespace ctl
