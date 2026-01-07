// precompiled.h
#ifndef PRECOMPILED_H
#define PRECOMPILED_H

// Устранить конфликт с byte в Windows
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#endif