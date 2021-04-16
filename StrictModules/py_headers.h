#ifndef __STRICTM_HEADER_H__
#define __STRICTM_HEADER_H__
#include "Python.h"
#include "symtable.h"

// remove conflicting macros from python-ast.h
#ifdef Compare
#undef Compare
#endif
#ifdef Set
#undef Set
#endif
#ifdef arg
#undef arg
#endif

#endif // __STRICTM_HEADER_H__