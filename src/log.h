#ifndef RV32_EMU_LOG_H
#define RV32_EMU_LOG_H

/**
 * \brief Sets the verbose mode.
 *
 * \param verbose The new value for verbose mode.
 *
 * \sa ver_printf
 */
void set_verbose(bool verbose);

/**
 * \brief Calls printf if verbose mode is on
 *
 * \param fmt Format string.
 * \param ... Format arguments.
 *
 * \sa set_verbose, printf
 */
void ver_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
