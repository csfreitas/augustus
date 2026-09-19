/* Allow an external parser snapshot to find its original header without adding
 * src/core to system include search, which would shadow the C string.h header. */
#include "core/xml_parser.h"
