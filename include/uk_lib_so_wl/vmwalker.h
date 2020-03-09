#pragma once

#include <uk/config.h>

#ifdef CONFIG_MAP_SPARE_VM_SPACE

/**
 * This functrion configures the spare vm space to a mapping, which is only
 * valid for the specified are. Every other region causes traps on an access.
 * The function returns the base of the page descriptor, which is valid
 */
unsigned long set_spare_mapping(unsigned long valid_base,
                                unsigned long number_valid_pages);

#endif