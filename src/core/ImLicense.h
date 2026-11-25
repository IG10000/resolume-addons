#pragma once
#include <string>

// Unlock handling for the IMMINENCE pack.
//
// The unlock code itself is never stored anywhere. When the right code is typed in, we save a key derived
// from it to a small file on this computer, and every IMMINENCE plugin checks that file. The code only holds a
// SHA-256 fingerprint of that key, so reading the source does not reveal the code or the key.
namespace imlic
{
std::string Sha256Hex( const std::string& data );

// True once this computer has been unlocked. Cheap to call every frame: it re-reads the key file at most every
// couple of seconds while still locked, so unlocking in one plugin unlocks every other plugin too.
bool IsUnlocked();

// Checks a typed code. On success the key file is written and the whole pack unlocks. Returns true on success.
bool TryUnlock( const std::string& code );

// Where the key file lives on this computer (for the user guide and the test host).
std::string KeyFilePath();
}// namespace imlic
