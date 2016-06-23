
#include "fbterm.h"

#ifndef HAVE_ISWPRINT

int iswprint (wint_t wc)
{
	if (wc >= 0x0 && wc <= 0x1F) return 0;
	if (wc >= 0x7F && wc <= 0x9F) return 0;
	if (wc >= 0xE000 && wc <= 0xF8FF) return 0;
	return 1;
}

#endif /* HAVE_ISWPRINT */
