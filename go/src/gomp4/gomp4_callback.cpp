#include "gomp4_callback.hpp"
#include "_cgo_export.h"

fMP4Writer CNewMP4()
{
	fMP4Writer handle = fMP4_CreateWriter(GoMP4Callback);

	return handle;
}
