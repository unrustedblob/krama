// [PUBLIC] kramac's exit statuses — the contract between this binary and
// whatever runs it. The test driver parses this file to extract the exit codes
// on every run; keep to these rules or its map goes wrong.
//
// 1. One enumerator per line
// 2. No trailing comments on enumerator lines
// 3. Explicit values are plain integer literals, no expressions
//
// Values are append-only once a fixture asserts one: adding a class is
// free, renumbering an existing one silently breaks every case that
// checks it.
// See D-035 for why 1 is skipped and why these classes were
// cut along whose fault the failure is, rather than which call failed.
// See D-039 for why the harness parses this file.

#ifndef KRAMA_EXIT_CODES_H
#define KRAMA_EXIT_CODES_H

enum KRMExitStatus {
        KRM_EXIT_SUCCESS = 0,

        // Exit status 1 is reserved as the sanitizers emit 1 on detecting an
        // issue. The test harness should report 1 as a sanitizer failure not as
        // a general program error so we skip it here

        // kramac was invoked wrongly — wrong number of arguments, or an
        // option it does not accept. Nothing was read, nothing was compiled
        KRM_EXIT_ERR_USAGE = 2,

        // Indicates an error caused by a bad input to the program e.g., a
        // non-existent file
        KRM_EXIT_ERR_INPUT,

        // An allocation failed - arena, token buffer, diagnostic string etc.
        KRM_EXIT_ERR_OOM,

        // The machine or the environment failed under us: an operation that
        // should have worked did not, through no fault of the source being
        // compiled. Not the user's to fix and not a bug in kramac. The message
        // names what failed, with strerror(errno) where an errno applies
        KRM_EXIT_ERR_ENV,

        // kramac itself is wrong — an invariant it relies on did not hold, or
        // it reached a state it has no handling for. Not the user's to fix and
        // not the machine's fault: report it. The abort variant of that path
        // dies on SIGABRT and never returns a status here
        KRM_EXIT_ERR_INTERNAL,

        // [NOTE] All exit-error codes to be added before this one
        //
        // No test fixtures should assert this as its value would change as and
        // when errors are introduced. Failure that finds no matching class
        // exits UNKNOWN which signals that a class could be missing
        KRM_EXIT_ERR_UNKNOWN,
};

#endif // KRAMA_EXIT_CODES_H
