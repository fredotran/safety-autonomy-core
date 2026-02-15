#pragma once

namespace safety_core::platform::watchdog
{

    class Watchdog
    {
      public:
        virtual ~Watchdog()            = default;
        virtual void kick() noexcept   = 0;
        virtual void expire() noexcept = 0;
    };

    class ManualWatchdog final : public Watchdog
    {
      public:
        void kick() noexcept override
        {
            expired_ = false;
            kicked_  = true;
        }

        void expire() noexcept override
        {
            expired_ = true;
        }

        [[nodiscard]] bool expired() const noexcept
        {
            return expired_;
        }
        [[nodiscard]] bool kicked() const noexcept
        {
            return kicked_;
        }

      private:
        bool expired_{false};
        bool kicked_{false};
    };

} // namespace safety_core::platform::watchdog
