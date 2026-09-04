
#pragma once
#include "HAL/Platform.hpp"
#include "Unreal/Core/Templates/UnrealTemplate.hpp"
#include "SDK/Structs/Char.h"

namespace UECustom {
	class FAsciiSet
	{
	public:
		template<typename CharType, int N>
		constexpr FAsciiSet(const CharType(&Chars)[N])
			: FAsciiSet(StringToBitset(Chars))
		{
		}

		template<typename CharType>
		constexpr bool Contains(CharType Char) const
		{
			return !!TestImpl(TChar<CharType>::ToUnsigned(Char));
		}

		template<typename CharType>
		constexpr RC::Unreal::uint64 Test(CharType Char) const
		{
			return TestImpl(TChar<CharType>::ToUnsigned(Char));
		}

		constexpr FORCEINLINE FAsciiSet operator+(char Char) const
		{
			InitData Bitset = { LoMask, HiMask };
			SetImpl(Bitset, TChar<char>::ToUnsigned(Char));
			return FAsciiSet(Bitset);
		}

		constexpr FORCEINLINE FAsciiSet operator|(FAsciiSet OtherSet) const
		{
			return FAsciiSet(LoMask | OtherSet.LoMask, HiMask | OtherSet.HiMask);
		}

		constexpr FORCEINLINE FAsciiSet operator&(FAsciiSet OtherSet) const
		{
			return FAsciiSet(LoMask & OtherSet.LoMask, HiMask & OtherSet.HiMask);
		}

		constexpr FORCEINLINE FAsciiSet operator~() const
		{
			return FAsciiSet(~LoMask, ~HiMask);
		}


		template<class CharType>
		static constexpr const CharType* FindFirstOrEnd(const CharType* Str, FAsciiSet Set)
		{
			for (FAsciiSet SetOrNil(Set.LoMask | NilMask, Set.HiMask); !SetOrNil.Test(*Str); ++Str);

			return Str;
		}

		template<class CharType>
		static constexpr const CharType* FindLastOrEnd(const CharType* Str, FAsciiSet Set)
		{
			const CharType* Last = FindFirstOrEnd(Str, Set);

			for (const CharType* It = Last; *It; It = FindFirstOrEnd(It + 1, Set))
			{
				Last = It;
			}

			return Last;
		}

		template<typename CharType>
		static constexpr const CharType* Skip(const CharType* Str, FAsciiSet Set)
		{
			while (Set.Contains(*Str))
			{
				++Str;
			}

			return Str;
		}

		template<typename CharType>
		static constexpr bool HasAny(const CharType* Str, FAsciiSet Set)
		{
			return *FindFirstOrEnd(Str, Set) != '\0';
		}

		template<typename CharType>
		static constexpr bool HasNone(const CharType* Str, FAsciiSet Set)
		{
			return *FindFirstOrEnd(Str, Set) == '\0';
		}

		template<typename CharType>
		static constexpr bool HasOnly(const CharType* Str, FAsciiSet Set)
		{
			return *Skip(Str, Set) == '\0';
		}


		template<class StringType>
		static constexpr StringType FindPrefixWith(const StringType& Str, FAsciiSet Set)
		{
			return Scan<EDir::Forward, EInclude::Members, EKeep::Head>(Str, Set);
		}

		template<class StringType>
		static constexpr StringType FindPrefixWithout(const StringType& Str, FAsciiSet Set)
		{
			return Scan<EDir::Forward, EInclude::NonMembers, EKeep::Head>(Str, Set);
		}

		template<class StringType>
		static constexpr StringType TrimPrefixWith(const StringType& Str, FAsciiSet Set)
		{
			return Scan<EDir::Forward, EInclude::Members, EKeep::Tail>(Str, Set);
		}

		template<class StringType>
		static constexpr StringType TrimPrefixWithout(const StringType& Str, FAsciiSet Set)
		{
			return Scan<EDir::Forward, EInclude::NonMembers, EKeep::Tail>(Str, Set);
		}

		template<class StringType>
		static constexpr StringType FindSuffixWith(const StringType& Str, FAsciiSet Set)
		{
			return Scan<EDir::Reverse, EInclude::Members, EKeep::Tail>(Str, Set);
		}

		template<class StringType>
		static constexpr StringType FindSuffixWithout(const StringType& Str, FAsciiSet Set)
		{
			return Scan<EDir::Reverse, EInclude::NonMembers, EKeep::Tail>(Str, Set);
		}

		template<class StringType>
		static constexpr StringType TrimSuffixWith(const StringType& Str, FAsciiSet Set)
		{
			return Scan<EDir::Reverse, EInclude::Members, EKeep::Head>(Str, Set);
		}

		template<class StringType>
		static constexpr StringType TrimSuffixWithout(const StringType& Str, FAsciiSet Set)
		{
			return Scan<EDir::Reverse, EInclude::NonMembers, EKeep::Head>(Str, Set);
		}

		template<class StringType>
		static constexpr bool HasAny(const StringType& Str, FAsciiSet Set)
		{
			return !HasNone(Str, Set);
		}

		template<class StringType>
		static constexpr bool HasNone(const StringType& Str, FAsciiSet Set)
		{
			RC::Unreal::uint64 Match = 0;
			for (auto Char : Str)
			{
				Match |= Set.Test(Char);
			}
			return Match == 0;
		}

		template<class StringType>
		static constexpr bool HasOnly(const StringType& Str, FAsciiSet Set)
		{
			auto End = GetData(Str) + GetNum(Str);
			return FindFirst<EInclude::Members>(Set, GetData(Str), End) == End;
		}

	private:
		enum class EDir { Forward, Reverse };
		enum class EInclude { Members, NonMembers };
		enum class EKeep { Head, Tail };

		template<EInclude Include, typename CharType>
		static constexpr const CharType* FindFirst(FAsciiSet Set, const CharType* It, const CharType* End)
		{
			for (; It != End && (Include == EInclude::Members) == !!Set.Test(*It); ++It);
			return It;
		}

		template<EInclude Include, typename CharType>
		static constexpr const CharType* FindLast(FAsciiSet Set, const CharType* It, const CharType* End)
		{
			for (; It != End && (Include == EInclude::Members) == !!Set.Test(*It); --It);
			return It;
		}

		template<EDir Dir, EInclude Include, EKeep Keep, class StringType>
		static constexpr StringType Scan(const StringType& Str, FAsciiSet Set)
		{
			auto Begin = Str.data();
			auto End = Str.data() + Str.size();
			auto It = Dir == EDir::Forward ? FindFirst<Include>(Set, Begin, End)
				: FindLast<Include>(Set, End - 1, Begin - 1) + 1;

			return Keep == EKeep::Head ? StringType(Begin, static_cast<RC::Unreal::int32>(It - Begin))
				: StringType(It, static_cast<RC::Unreal::int32>(End - It));
		}

		struct InitData { RC::Unreal::uint64 Lo, Hi; };
		static constexpr RC::Unreal::uint64 NilMask = RC::Unreal::uint64(1) << '\0';

		static constexpr FORCEINLINE void SetImpl(InitData& Bitset, RC::Unreal::uint32 Char)
		{
			RC::Unreal::uint64 IsLo = RC::Unreal::uint64(0) - (Char >> 6 == 0);
			RC::Unreal::uint64 IsHi = RC::Unreal::uint64(0) - (Char >> 6 == 1);
			RC::Unreal::uint64 Bit = RC::Unreal::uint64(1) << RC::Unreal::uint8(Char & 0x3f);

			Bitset.Lo |= Bit & IsLo;
			Bitset.Hi |= Bit & IsHi;
		}

		constexpr FORCEINLINE RC::Unreal::uint64 TestImpl(RC::Unreal::uint32 Char) const
		{
			RC::Unreal::uint64 IsLo = RC::Unreal::uint64(0) - (Char >> 6 == 0);
			RC::Unreal::uint64 IsHi = RC::Unreal::uint64(0) - (Char >> 6 == 1);
			RC::Unreal::uint64 Bit = RC::Unreal::uint64(1) << (Char & 0x3f);

			return (Bit & IsLo & LoMask) | (Bit & IsHi & HiMask);
		}

		template<typename CharType, int N>
		static constexpr InitData StringToBitset(const CharType(&Chars)[N])
		{
			InitData Bitset = { 0, 0 };
			for (int I = 0; I < N - 1; ++I)
			{
				SetImpl(Bitset, TChar<CharType>::ToUnsigned(Chars[I]));
			}

			return Bitset;
		}

		constexpr FAsciiSet(InitData Bitset)
			: LoMask(Bitset.Lo), HiMask(Bitset.Hi)
		{
		}

		constexpr FAsciiSet(RC::Unreal::uint64 Lo, RC::Unreal::uint64 Hi)
			: LoMask(Lo), HiMask(Hi)
		{
		}

		RC::Unreal::uint64 LoMask, HiMask;
	};
}
