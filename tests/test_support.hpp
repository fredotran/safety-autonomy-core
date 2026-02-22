#pragma once

#include "safety_core/diag/diagnostic_transport.hpp"

#include <iostream>
#include <vector>

namespace safety_core::test_support
{

    inline bool check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "[FAIL] " << message << '\n';
            return false;
        }
        return true;
    }

    class CaptureTransport final : public diag::DiagnosticTransport
    {
      public:
        void publish(const diag::DiagnosticEvent& event) noexcept override
        {
            events.push_back(event);
        }

        std::vector<diag::DiagnosticEvent> events{};
    };

} // namespace safety_core::test_support
