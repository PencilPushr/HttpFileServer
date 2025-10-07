#pragma once

#include <string>
#include <wtypes.h>

//
// used for when the client is asking for a range
// of data from a file
//
typedef struct _HTTP_PARSED_BYTE_RANGE
{
	ULONG64 Start;
	ULONG64 End;

	BOOL HasStart;
	BOOL HasEnd;

} HTTP_PARSED_BYTE_RANGE, *PHTTP_PARSED_BYTE_RANGE;

BOOL
HttpParseRangeHeader(
	_In_ CONST std::string& RangeHeader,
	_Inout_ PHTTP_PARSED_BYTE_RANGE ParsedByteRange
);