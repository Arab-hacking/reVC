#pragma once

// CRC-32-IEEE key generator used for animation and bone name hashes.
// https://www.xilinx.com/support/documentation/application_notes/xapp209.pdf
class CKeyGen
{
public:
	static uint32 AppendStringToKey(uint32 key, const char *str)
	{
		for (int32 i = 0; str[i]; ++i)
			key = KeyTable[(uint8)(str[i] ^ key)] ^ (key >> 8);
		return key;
	}

	static uint32 GetKey(const char *str)
	{
		uint32 key = INITIAL_REMAINDER;
		for (int32 i = 0; str[i]; ++i)
			key = KeyTable[(uint8)(str[i] ^ key)] ^ (key >> 8);
		return key;
	}

	static uint32 GetKey(const char *str, int32 size)
	{
		uint32 key = INITIAL_REMAINDER;
		for (int32 i = 0; i < size; ++i)
			key = KeyTable[(uint8)(str[i] ^ key)] ^ (key >> 8);
		return key;
	}

	static uint32 GetUppercaseKey(const char *str)
	{
		uint32 key = INITIAL_REMAINDER;
		for (int32 i = 0; str[i]; ++i)
			key = KeyTable[(uint8)(ToUppercase((uint8)str[i]) ^ key)] ^ (key >> 8);
		return key;
	}

	static uint32 GetUppercaseKey(const char *begin, const char *end)
	{
		uint32 key = INITIAL_REMAINDER;
		for (const char *i = begin; i != end; i++)
			key = KeyTable[(uint8)(ToUppercase((uint8)*i) ^ key)] ^ (key >> 8);
		return key;
	}

private:
	static char ToUppercase(char x)
	{
		return ('a' <= x && x <= 'z') ? (char)(x - 0x20) : x;
	}

	static const uint32 INITIAL_REMAINDER = 0xFFFFFFFF;

	// https://web.mit.edu/freebsd/head/sys/libkern/crc32.c -- 0xEDB88320
	static const uint32 KeyTable[256];
};
