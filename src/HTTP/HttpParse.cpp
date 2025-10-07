#include "HttpParse.h"

#include <string_view>

BOOL
HttpParseRangeHeader(
	_In_ CONST std::string& RangeHeader,
	_Inout_ PHTTP_PARSED_BYTE_RANGE ParsedByteRange
)
{
	if ( RangeHeader.empty( ) || ParsedByteRange == NULL )
	{
		return FALSE;
	}

	constexpr const char* TYPE_PREFIX = "bytes=";
	constexpr size_t TYPE_PREFIX_SIZE = 6; // strlen( TYPE_PREFIX );

	if ( RangeHeader.compare( 0, TYPE_PREFIX_SIZE, TYPE_PREFIX ) != NULL )
	{
		//
		// unsupported type
		//
		return FALSE;
	}

	std::string_view RangeMarker = RangeHeader.data( ) + TYPE_PREFIX_SIZE;

	ParsedByteRange->Start    = NULL;
	ParsedByteRange->End      = NULL;
	ParsedByteRange->HasStart = FALSE;
	ParsedByteRange->HasEnd   = FALSE;

	size_t rangeSeperator = RangeMarker.find( '-' );
	if ( rangeSeperator == std::string::npos )
	{
		return FALSE;
	}

	if ( rangeSeperator > 0 )
	{
		ParsedByteRange->Start = std::stoull( std::string( RangeMarker.substr( 0, rangeSeperator ) ) );
		ParsedByteRange->HasStart = TRUE;
	}

	if ( rangeSeperator + 1 < RangeMarker.size( ) )
	{
		ParsedByteRange->End = std::stoull( std::string( RangeMarker.substr( rangeSeperator + 1 ) ) );
		ParsedByteRange->HasEnd = TRUE;
	}

	return TRUE;
}