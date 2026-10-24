#pragma once

namespace UECustom {
    class FWeakObjectPtr
    {
    public:
        FWeakObjectPtr()
        {
        }
    public:
        int ObjectIndex = 0;
        int ObjectSerialNumber = 0;
    };
}
