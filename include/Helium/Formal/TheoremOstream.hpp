#ifndef HELIUM_FORMAL_THEOREMOSTREAM_HPP
#define HELIUM_FORMAL_THEOREMOSTREAM_HPP

#include <ostream>

namespace Helium {

class Theorem;

std::ostream& operator<<(std::ostream& stream, const Theorem& theorem);

}

#endif
