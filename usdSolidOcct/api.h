// SPDX-License-Identifier: MIT
#ifndef USDSOLIDOCCT_API_H
#define USDSOLIDOCCT_API_H
#include "pxr/base/arch/export.h"
#ifdef USDSOLIDOCCT_EXPORTS
#define USDSOLIDOCCT_API ARCH_EXPORT
#else
#define USDSOLIDOCCT_API ARCH_IMPORT
#endif
#endif
