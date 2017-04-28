#pragma once
#include <stdint.h>
#include <fstream>
#include "API.h"

#pragma pack(push, 1)
struct FileHeader {
	uint32_t Signature;					// identifier for CLEO scripts
	uint32_t Architecture;				// identifier script assembly format (you never know..)
	union {
		struct {
			uint8_t	VersionMinor : 4;	// we may use this to identify minor changes in the format
			uint8_t	VersionMajor : 4;	// major changes we can fork and run older scripts using the original code
		};
		uint8_t	Version;
	};
	uint8_t InstructionSize;
	uint8_t IntegerSize;
	uint32_t NumGlobals;		// specifies the amount of global space to reserve
	uint32_t GlobalsOffset;		// 

	uint32_t StackSize;			// how much space is needed for the stack in total?
	uint32_t StringSegmentSize;	// specifies the total size of the strings segment

	FileHeader() {
		// The signature and architecture of loaded scripts must match or the file is considered invalid
		Signature = 'CLE';			// CLEO signature
		Architecture = 'RSCM';		// Reduced (instruction) SCM

									// Make sure any loaded script was not built for a newer version
		VersionMinor = CLARA_ASSEMBLY_VER_MINOR;
		VersionMajor = CLARA_ASSEMBLY_VER_MINOR;

		// Set up defaults
		InstructionSize = 1;
		IntegerSize = 4;
		NumGlobals = 0;
		GlobalsOffset = 0;

		StackSize = 0;
		StringSegmentSize = 0;
	}

	// Attempts to validate the header - returns true on success
	inline bool Validate() {
		return (
			Signature == 'CLE' &&
			Architecture == 'RSCM' &&
			(VersionMajor <= CLARA_ASSEMBLY_VER_MAJOR && VersionMinor <= CLARA_ASSEMBLY_VER_MINOR)
			);
	}

	// Loads header data from a loaded ifstream - returns true if valid data was loaded
	bool Load(std::ifstream& file) {
		file.read((char*)this, sizeof(*this));
		//file >> Signature >> Architecture >> Version;
		//file >> StackSize >> NumGlobals >> StringSegmentSize;
		return Validate();
	}
};
#pragma pack(pop)