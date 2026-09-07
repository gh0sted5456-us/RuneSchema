#include "SDK/UnrealOffsets.h"
#include "SDK/DragonWildsSignatures.h"
#include "Unreal/NameTypes.hpp"
#include "Unreal/FMemory.hpp"
#include "Unreal/UnrealVersion.hpp"
#include "Unreal/UnrealInitializer.hpp"
#include "Zydis/Zydis.h"
#include "Utility/Logging.h"
#include <bit>
#include <format>
#include <stdexcept>

using namespace RC;
using namespace RC::Unreal;

void DragonWilds::UnrealOffsets::Initialize()
{
    Unreal::Version::Major = 5;
    Unreal::Version::Minor = 6;

    PS::Log<LogLevel::Verbose>(STR("Unreal Version set to {}.{}.\n"), Unreal::Version::Major, Unreal::Version::Minor);

    auto FNameConstructorAddress = DragonWilds::SignatureManager::GetSignature("FName::Constructor");
    FName::ConstructorInternal.assign_address(FNameConstructorAddress);
    PS::Log<LogLevel::Verbose>(STR("FName::Constructor was assigned address of {}\n"), FNameConstructorAddress);

    auto FNameToStringAddress = DragonWilds::SignatureManager::GetSignature("FName::ToString_Wchar");
    FName::ToStringInternal.assign_address(FNameToStringAddress);
    PS::Log<LogLevel::Verbose>(STR("FName::ToString was assigned address of {}\n"), FNameToStringAddress);

    UnrealInitializer::InitializeVersionedContainer();
    PS::Log<LogLevel::Verbose>(STR("Versioned Container initialized.\n"));
}

void DragonWilds::UnrealOffsets::InitializeGMalloc()
{
    if (RC::Unreal::GMalloc) return;

    auto StartAddr = static_cast<uint8_t*>(DragonWilds::SignatureManager::GetSignature("FMemory::Free"));
    if (!StartAddr)
    {
        throw std::runtime_error("Signature for FMemory::Free was invalid.");
    }

    ZydisDecoder Decoder{};
    ZydisDecoderInit(&Decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    ZyanUSize Offset = 0;
    ZydisDecodedInstruction Instruction{};
    ZydisDecodedOperand Operands[10]{};
    bool CallFound = false;

    while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&Decoder, StartAddr + Offset, 100 - Offset, &Instruction, Operands)))
    {
        if (CallFound)
        {
            break;
        }

        if (Instruction.mnemonic == ZYDIS_MNEMONIC_CALL)
        {
            CallFound = true;
        }

        Offset += Instruction.length;
    }

    if (Instruction.mnemonic != ZYDIS_MNEMONIC_MOV)
    {
        throw std::runtime_error(std::format("Expected MOV instruction after CALL, but found {}", ZydisMnemonicGetString(Instruction.mnemonic)));
    }

    if (Instruction.operand_count < 2)
    {
        throw std::runtime_error("MOV instruction has less than 2 operands.");
    }

    if (Operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER || Operands[1].type != ZYDIS_OPERAND_TYPE_MEMORY)
    {
        throw std::runtime_error("Unexpected operand types. Expected [REGISTER, MEMORY].");
    }

    const auto& MemOp = Operands[1].mem;
    if (MemOp.base != ZYDIS_REGISTER_RIP)
    {
        throw std::runtime_error(std::format("Unexpected base register. Expected [RIP]."));
    }

    if (!MemOp.disp.has_displacement)
    {
        throw std::runtime_error(std::format("RIP operand is missing displacement field."));
    }

    uint8_t* MovInstructionAddr = StartAddr + Offset;
    uint8_t* NextInstructionAddr = MovInstructionAddr + Instruction.length;
    int64_t DispValue = MemOp.disp.value;
    uint8_t* GMallocAddr = static_cast<uint8_t*>(NextInstructionAddr) + DispValue;

    RC::Unreal::GMalloc = std::bit_cast<RC::Unreal::FMalloc**>(GMallocAddr);

    PS::Log<LogLevel::Verbose>(STR("Found GMalloc: {}\n"), static_cast<void*>(GMallocAddr));
}
