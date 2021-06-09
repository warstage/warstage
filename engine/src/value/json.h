// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__VALUE__JSON_H
#define WARSTAGE__VALUE__JSON_H

#include "./value.h"
#include <istream>
#include <ostream>


std::istream& operator>>(std::istream&, Value&);
std::ostream& operator<<(std::ostream&, const Value&);

#endif
