/* audit_internal.h - test hooks for the audit logger. NOT part of the public API. */
#ifndef RRS_AUDIT_INTERNAL_H
#define RRS_AUDIT_INTERNAL_H

#include <cstddef>
#include <ctime>
#include <string>

/* Only the most recent kAuditMemoryEntries lines are kept in memory. */
const size_t kAuditMemoryEntries = 1000;

size_t audit_entry_count_for_tests(void);
bool audit_entry_for_tests(size_t index, std::string& line_out);   /* 0 = oldest kept */
void audit_reset_for_tests(void);                                   /* clear memory, close file */
void audit_set_fixed_time_for_tests(time_t t);                      /* 0 = real clock */

#endif
