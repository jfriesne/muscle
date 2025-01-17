/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleMiscUtilityFunctions_h
#define MuscleMiscUtilityFunctions_h

#include "message/Message.h"
#include "util/Queue.h"

namespace muscle {

/** @defgroup miscutilityfunctions The MiscUtilityFunctions function API
 *  These functions are all defined in MiscUtilityFunctions.h, and are stand-alone
 *  functions that do various small convenient things that don't fit anywhere else.
 *  @{
 */

/** Parses the given arguments into a Message full of string fields.
  * Arguments should be of the form argname or argname=value.
  * The latter will be added as string fields; the former will
  * be treated the same, with the string equal to "".
  * Any prefixed dashes will be stripped/ignored.
  * All argument names will be forced to lower case.
  * @param argc As passed in to main()
  * @param argv As passed in to main
  * @param addTo The message to add the arguments to
  * @param caseSensitive Defaults to false.  If true, the case of the arguments will be retained; if false, they will be forced to lower case.
  * @returns B_NO_ERROR on success, B_OUT_OF_MEMORY on failure.
  */
status_t ParseArgs(int argc, char ** argv, Message & addTo, bool caseSensitive = false);

/** Parses the given arguments into a Message full of string fields.
  * Arguments should be of the form argname or argname=value.
  * The latter will be added as string fields; the former will
  * be treated the same, with the string equal to "".
  * Any prefixed dashes will be stripped/ignored.
  * All argument names will be forced to lower case.
  * @param args a list of String arguments
  * @param addTo The message to add the arguments to
  * @param caseSensitive Defaults to false.  If true, the case of the arguments will be retained; if false, they will be forced to lower case.
  * @returns B_NO_ERROR on success, B_OUT_OF_MEMORY on failure.
  */
status_t ParseArgs(const Queue<String> & args, Message & addTo, bool caseSensitive = false);

/** Parses settings from the given file.  Works similarly to
 *  ParseArgs() (above), only the values are read from a file
 *  instead of from the arguments vector.  The file may contain
 *  comments that are prepended with a hash symbol (#); these
 *  will be safely ignored.
 *
 *  Also, the file may contain special lines beginning with the
 *  keyword "begin" or "end".  These lines delineate a sub-section,
 *  which will show up in (addTo) as a sub-Message.  This can
 *  be useful for expressing hierarchical data.  The "begin" line
 *  may optionally contain the name to store the sub-Message under;
 *  if no name is present, the sub-Message will be added with name "".
 *  Multiple sub-sections with the same name are supported.
 *
 *  @param file File pointer to read from.  This file must be
 *              opened for reading, and will not be fclosed() by this function.
 *  @param addTo The message to add the arguments to
 *  @param caseSensitive Defaults to false.  If true, the case of the arguments will be retained; if false, they will be forced to lower case.
 *  @return B_NO_ERROR on success, or an error code on failure.
 */
status_t ParseFile(FILE * file, Message & addTo, bool caseSensitive = false);

/** Same as above, except that the file data is passed in as a String
  * instead being read from a FILE handle.
  * @param fileData a String containing the contents of the file to parse.
  * @param addTo The Message to add the arguments to
  * @param caseSensitive Defaults to false.  If true, the case of the arguments will be retained; if false, they will be forced to lower case.
  * @return B_NO_ERROR on success, or an error code on failure.
  */
status_t ParseFile(const String & fileData, Message & addTo, bool caseSensitive = false);

/** Does the opposite of ParseFile():  it takes the contents of
 *  (writeFrom) and saves it into (file) as a human-readable
 *  text file.
 *  @param readFrom The Message to scan for data to write to (file).
 *                  This Message should be of the same structure as returned by ParseFile().
 *  @param file File pointer to write to.  This file must be
 *              opened for writing, and will not be fclosed() by this function.
 *  @return B_NO_ERROR on success, or an error code on failure.
 */
status_t UnparseFile(const Message & readFrom, FILE * file);

/** Same as above, except that the text data returned as a String.
  * @param readFrom The Message to scan for data to write to (file).
  *                 This Message should be of the same structure as returned by ParseFile().
  * @return A string representation of (readFrom) on success, or "" on failure.
  *         Note that only fields of B_STRING_TYPE and B_MESSAGE_TYPE in (readFrom)
  *         will be converted into string format.  Other data types will be ignored.
  */
String UnparseFile(const Message & readFrom);

/** Parses the single string argument (arg) and adds the results to (addTo).
 *  Formatting rules for the string are described above in the ParseArgs() documentation.
 *  @param arg string (or string=value pair) to parse.
 *  @param addTo The Message to add the parsed arguments to (as String fields).
  * @param caseSensitive Defaults to false.  If true, the case of the arguments will be retained; if false, they will be forced to lower case.
 *  @return B_NO_ERROR on success, or an error code on failure.
 */
status_t ParseArg(const String & arg, Message & addTo, bool caseSensitive = false);

/** Scans a line for multiple arguments and calls ParseArg() on each one found.
 *  Quotation marks will be used to group tokens if necessary.
 *  @param arg A line of text, eg  arg1=blah arg2=borf arg3="quoted borf"
 *  @param addTo Message to add the argument results to.
  * @param caseSensitive Defaults to false.  If true, the case of the arguments will be retained; if false, they will be forced to lower case.
 *  @return B_NO_ERROR on success, or an error code on failure.
 */
status_t ParseArgs(const String & arg, Message & addTo, bool caseSensitive = false);

/** This does the inverse operation of ParseArgs().  That is to say, it
  * takes a Message that was previously created via ParseArgs() and returns
  * a String representing the same data.  Note that the returned String isn't
  * guaranteed to be identical to the one that was previously passed in to
  * ParseArgs(); in particular, any comments will have been stripped out,
  * and the order of the items may be different in some cases.
  * @param argMsg A Message containing String field indicating the arguments
  *               to add to the String
  * @return The resulting String.
  */
String UnparseArgs(const Message & argMsg);

/** Parses the given arguments into a Queue of Strings.
  * Any prefixed dashes will be stripped/ignored.
  * All argument names will be forced to lower case.
  * @param argc As passed in to main()
  * @param argv As passed in to main
  * @param addTo The Queue to add the arguments to
  * @param caseSensitive Defaults to false.  If true, the case of the arguments will be retained; if false, they will be forced to lower case.
  * @returns B_NO_ERROR on success, B_OUT_OF_MEMORY on failure.
  */
status_t ParseArgs(int argc, char ** argv, Queue<String> & addTo, bool caseSensitive = false);

/** Parses settings from the given file.  Works similarly to
 *  ParseArgs() (above), only the values are read from a file
 *  instead of from the arguments vector.  The file may contain
 *  comments that are prepended with a hash symbol (#); these
 *  will be safely ignored.
 *
 *  @param file File pointer to read from.  This file must be
 *              opened for reading, and will not be fclosed() by this function.
  * @param addTo The Queue to add the arguments to
  * @param caseSensitive Defaults to false.  If true, the case of the arguments will be retained; if false, they will be forced to lower case.
 *  @return B_NO_ERROR on success, or an error code on failure.
 */
status_t ParseFile(FILE * file, Queue<String> & addTo, bool caseSensitive = false);

/** Same as above, except that the file data is passed in as a String
  * instead of as a FILE handle.
  * @param fileData a String containing the contents of the file to parse.
  * @param addTo The Queue to add the arguments to
  * @param caseSensitive Defaults to false.  If true, the case of the arguments will be retained; if false, they will be forced to lower case.
  * @return B_NO_ERROR on success, or an error code on failure.
  */
status_t ParseFile(const String & fileData, Queue<String> & addTo, bool caseSensitive = false);

/** Parses the single string argument (arg) and adds the results to (addTo).
 *  Formatting rules for the string are described above in the ParseArgs() documentation.
 *  @param arg string to parse.
 *  @param addTo The Queue to add the argument to.
  * @param caseSensitive Defaults to false.  If true, the case of the arguments will be retained; if false, they will be forced to lower case.
 *  @return B_NO_ERROR on success, or an error code on failure.
 */
status_t ParseArg(const String & arg, Queue<String> & addTo, bool caseSensitive = false);

/** Scans a line for multiple arguments and calls ParseArg() on each one found.
 *  Quotation marks will be used to group tokens if necessary.
 *  @param arg A line of text, eg  arg1=blah arg2=borf arg3="quoted borf"
 *  @param addTo Queue to add the argument results to.
  * @param caseSensitive Defaults to false.  If true, the case of the arguments will be retained; if false, they will be forced to lower case.
 *  @return B_NO_ERROR on success, or an error code on failure.
 */
status_t ParseArgs(const String & arg, Queue<String> & addTo, bool caseSensitive = false);

/** This does the inverse operation of ParseArgs().  That is to say, it
  * takes a Queue that was previously created via ParseArgs() and returns
  * a String representing the same data.  Note that the returned String isn't
  * guaranteed to be identical to the one that was previously passed in to
  * ParseArgs(); in particular, any comments will have been stripped out.
  * @param argQ A Queue containing String field indicating the arguments to add to the String
  * @param startIdx Index to start reading the Queue at.  Defaults to zero.
  * @param afterEndIdx One greater than the largest index in the Queue to read.  If this value
  *                    is greater than the length of the queue, it will be treated as if it was the
  *                    length of the Queue.  Defaults to MUSCLE_NO_LIMIT.
  * @return The resulting String.
  */
String UnparseArgs(const Queue<String> & argQ, uint32 startIdx=0, uint32 afterEndIdx=MUSCLE_NO_LIMIT);

/** Convenience method:  Looks for a given hostname or hostname:port string in
 *  the given field of the args Message, and returns the appropriate parsed
 *  values if it is found.
 *  @param args the Message that was returned by ParseArg().
 *  @param fn the field name to look for in (args)
 *  @param retHost On successful return, the hostname or IP address to connect to will be written here.
 *  @param retPort On successful return, if a port number was parsed it will be written here.
 *  @param portRequired If false, this function will succeed even if no port was specified.
 *                      If true, the function will fail if a port was not specified (eg "localhost:5555").
 *                      Defaults to false.
 *  @param argIdx The field index to use when parsing the argument string from (args).  Defaults to zero.
 *  @returns B_NO_ERROR if an argument was parsed, or an error code if it wasn't.
 */
status_t ParseConnectArg(const Message & args, const String & fn, String & retHost, uint16 & retPort, bool portRequired = false, uint32 argIdx = 0);

/** Same as above, except that instead of looking for the specified string in a Message, in this
 *  case the string is passed in directly.
 *  @param arg The connect string (eg "localhost:2960")
 *  @param retHost On successful return, the hostname or IP address to connect to will be written here.
 *  @param retPort On successful return, if a port number was parsed it will be written here.
 *  @param portRequired If false, this function will succeed even if no port was specified.
 *                      If true, the function will fail if a port was not specified (eg "localhost:5555").
 *                      Defaults to false.
 *  @returns B_NO_ERROR if an argument was parsed, or an error code if it wasn't.
 */
status_t ParseConnectArg(const String & arg, String & retHost, uint16 & retPort, bool portRequired = false);

/** Given a hostname (or IP address) and a port number, returns the associated connect-string (eg "localhost:9999")
  * or ("[ff05::1]:5555")
  * @param host A hostname or IP address
  * @param port A port number.
  */
String GetConnectString(const String & host, uint16 port);

/** Convenience method:  Looks for a port number in the given field of the Message,
 *  and sets (retPort) if it finds one.
 *  @param args the Message that was returned by ParseArg().
 *  @param fn the field name to look for in (args)
 *  @param retPort On successful return, if a port number was parsed it will be written here.
 *  @param argIdx optional index to look in for the string to parse.
 *  @returns B_NO_ERROR if an argument was parsed, or an error code if it wasn't.
 */
status_t ParsePortArg(const Message & args, const String & fn, uint16 & retPort, uint32 argIdx = 0);

/** Given an English word representing a boolean (eg "on", "off", "enable", "disable", "true", "false", "1", "0", etc),
  * returns the corresponding boolean value.
  * @param word The english word string to parse.
  * @param defaultValue what to return if the passed-in string is not recognized.
  * @returns true or false, depending on the word.  If the word isn't recognized, (defaultValue) will be returned.  Defaults to true.
  */
MUSCLE_NODISCARD bool ParseBool(const String & word, bool defaultValue=true);

/** Looks for some globally useful startup arguments in the (args)
 *  Message and handles them by calling the appropriate setup routines.
 *  Recognized arguments currently include the following:
 *     daemon                -- Non-Windows only:  Run this process in the background
 *     localhost=ip          -- Treat connections from localhost as if they were coming from (ip)
 *     displaylevel=levelstr -- Set the stdout output display filter level to (levelstr)
 *     filelevel=levelstr    -- Set the output log file filter level to (levelstr)
 *     logfile=levelstr      -- Force the log file to have this name/location
 *     nice[=niceLevel]      -- Linux/OSX only: makes this process nicer (ie lower priority)
 *     mean[=meanLevel]      -- Linux/OSX only: makes this process meaner (ie higher priority)
 *     realtime[=priority]   -- Linux only: makes this process real-time (requires root access)
 *     debugcrashes          -- Linux only: print a stack trace when a crash occurs
 *     console               -- Windows only:  open a DOS box to display this window's output
 *  @param args an arguments Message, as produced by ParseArgs() or ParseFile() or etc.
 */
void HandleStandardDaemonArgs(const Message & args);

/** Similar to the standard exit() call, except that no global object destructors will
  * be called.  This is sometimes useful, eg in fork() situations where you want the
  * parent process to just go away without any chance of a crash during cleanup.
  * @param exitCode the exit code value that should be passed back to our parent process
  *                 (ie the argument to pass to exit() or _exit())
  */
//coverity[+kill]
void ExitWithoutCleanup(int exitCode);

/** Causes this process to terminate abnormally (ie with a crash).
  * Under Windows, this is implemented by calling RaiseException(EXCEPTION_BREAKPOINT).
  * Under all other OS's, this is implemented by calling abort().
  * @note this function will not return!
  */
//coverity[+kill]
void Crash();

/** Calls fork(), setsid(), chdir(), umask(), etc, to fork an independent daemon process.
 *  Also closes all open file descriptors.
 *  Note that this function will call ExitWithoutCleanup() on the parent process if successful,
 *  and thus won't ever return in that process.
 *  @param optNewDir If specified, the daemon will chdir() to the directory specified here.
 *  @param optOutputTo Where to redirect stderr and stdout to.  Defaults to "/dev/null".
 *                     If set to NULL, or if the output device can't be opened, output
 *                     will not be rerouted.
 *  @param createOutputFileIfNecessary if set true, and (optOutputTo) can't be opened,
 *                                     (optOutputTo) will be created.
 *  @return B_NO_ERROR on success (the child process will see this), an error code on failure.
 */
status_t BecomeDaemonProcess(const char * optNewDir = NULL, const char * optOutputTo = "/dev/null", bool createOutputFileIfNecessary = true);

/** Returns true iff we are a daemon process created via BecomeDaemonProcess() or SpawnDaemonProcess() */
MUSCLE_NODISCARD bool IsDaemonProcess();

/** Same as BecomeDaemonProcess(), except that the parent process returns as well as the child process.
 *  @param returningAsParent Set to true on return of the parent process, or false on return of the child process.
 *  @param optNewDir If specified, the child will chdir() to the directory specified here.
 *  @param optOutputTo Where to redirect stderr and stdout to.  Defaults to "/dev/null".
 *                     If set to NULL, or if the output device can't be opened, output
 *                     will not be rerouted.
 *  @param createOutputFileIfNecessary if set true, and (optOutputTo) can't be opened,
 *                                     (optOutputTo) will be created.
 *  @return B_NO_ERROR (twice!) on success, an error code on failure.
 */
status_t SpawnDaemonProcess(bool & returningAsParent, const char * optNewDir = NULL, const char * optOutputTo = "/dev/null", bool createOutputFileIfNecessary = true);

/** Convenience function:  Removes any ANSI formatting escape-sequences from (s), so
  * that (s) can be displayed as plain text without a bunch of garbage showing up in it.
  * @param s a String to remove ANSI sequences from, in-place.
  */
void RemoveANSISequences(String & s);

/** Given a string, returns that same string except with any symbols that are not illegal
  * in a DNS label removed.  (According to DNS rules, only letters, digits, and the '-'
  * character are legal in a DNS label, and the label must be less than 64 characters long).
  * Note that this string cleans up just a single part of a DNS hostname path.
  * If you want to clean up a path string (eg "www.foo.com"), call CleanupDNSPath() instead.
  * @param s A string that is presented as a candidate for being a DNS label.
  * @param optAdditionalAllowedChars if specified, this string represents a list of other
  *                                  characters that we will allow into the returned String as a
  *                                  special case (ie we won't filter them out)
  * @returns the DNS label that most closely resembles (s).
  */
String CleanupDNSLabel(const String & s, const String & optAdditionalAllowedChars = GetEmptyString());

/** Given a DNS path string (eg "www.foo.com") runs each dot-separated portion of the
  * path through CleanupDNSLabel() and returns the cleaned up result.
  * @param s A string that is presented as a candidate for being a DNS path.
  * @param optAdditionalAllowedChars if specified, this string represents a list of other
  *                                  characters that we will allow into the returned String as a
  *                                  special case (ie we won't filter them out)
  * @returns the DNS path that most closely resembles (s).
  */
String CleanupDNSPath(const String & s, const String & optAdditionalAllowedChars = GetEmptyString());

/** Convenience function.  Given a buffer of arbitrary data, returns a nybble-ized String
  * that represents that same data using only the ASCII characters 'A' through 'P.  The
  * returned String will be twice the length of the passed-in buffer, and the original
  * data can be recovered from the String by calling DenybbleizeData().
  * @param buf The data to nybbleize
  * @param retString On success, the nybbleized String is written here.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t NybbleizeData(const ByteBuffer & buf, String & retString);

/** Convenience function.  Given a buffer of arbitrary data, returns a nybble-ized String
  * that represents that same data using only the ASCII characters 'A' through 'P.  The
  * returned String will be twice the length of the passed-in buffer, and the original
  * data can be recovered from the String by calling DenybbleizeData().
  * @param inBytes Pointer to the bytes to nybbleize.
  * @param numInBytes Number of bytes pointed to by (inBytes).
  * @param retString On success, the nybbleized String is written here.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t NybbleizeData(const uint8 * inBytes, uint32 numInBytes, String & retString);

/** Convenience function.  Given a String that was produced by NybblizedData(),
  * returns the eqivalent ByteBuffer.
  * @param nybbleizedText The String to de-nybbleize
  * @param retBuf On success, the de-nybbleized data is written here.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t DenybbleizeData(const String & nybbleizedText, ByteBuffer & retBuf);

/** Convenience function:  Returns a string which is a nybbleized representation of
  * the passed-in string's contents (not including its NUL terminator byte)
  * @param str A string to nybbleize
  * @returns A nybbleized equivalent of (str), as described in NybbleizeData().
  */
String NybbleizeString(const String & str);

/** Convenience function:  Returns a string which is the denybbleized
  * representation of the passed-in nybbleized string.
  * @param nybStr A string to denybbleize.  Note that not all nybbleized
  *               strings can be de-nybblized correctly back into a
  *               String object:  in particular, if the de-nybbleized
  *               data contains any NUL bytes, then the String
  *               returned by this function will be truncated at the first NUL.
  *               If you need to be able to decode any legal nybbleized string,
  *               call NybbleizeData() instead of this function.
  */
String DenybbleizeString(const String & nybStr);

/** This function is like strstr(), except that instead of searching for a substring
  * within a string, it looks for a given binary pattern inside a binary buffer.
  * @param lookIn The buffer to look for the sub-region inside
  * @param numLookInBytes The number of bytes pointed to by (lookIn)
  * @param lookFor The byte pattern to look for inside of (lookin)
  * @param numLookForBytes The number of bytes pointed to by (lookFor)
  * @returns A pointer to the first instance of (lookFor) found inside of (lookIn), or NULL
  *          if no such pattern was found inside of (lookIn).
  */
MUSCLE_NODISCARD const uint8 * MemMem(const uint8 * lookIn, uint32 numLookInBytes, const uint8 * lookFor, uint32 numLookForBytes);

/** This is a convenience function for debugging.  It will print to stdout the
  * specified array of bytes in human-readable hexadecimal format, along with
  * an ASCII sidebar when possible.
  * @param bytes The bytes to print out.  May be NULL.
  * @param numBytes How many bytes (bytes) points to
  * @param optDesc if non-NULL, this will be used as a prefix/title string.
  * @param numColumns If specified non zero, then the bytes will be printed
  *                   out with this many bytes per row.  Defaults to 16.
  *                   If set to zero, then all the output will be placed
  *                   on a single line, using a simpler hex-only format.
  * @param p The OutputPrinter to use for printing.  A FILE (e.g. stdout), a String, or a MUSCLE_LOG_SEVERITY_* value can 
  *          be specified here, depending on where you'd like the text output to go to.  Defaults to stdout.
  */
void PrintHexBytes(const void * bytes, uint32 numBytes, const char * optDesc = NULL, uint32 numColumns = 16, const OutputPrinter & p = stdout);

/** This is a convenience function for debugging.  It will print to stdout the
  * specified array of bytes in human-readable hexadecimal format, along with
  * an ASCII sidebar when possible.
  * @param bbRef A ByteBufferRef referencing the bytes to print.  May be NULL.
  * @param optDesc if non-NULL, this will be used as a prefix/title string.
  * @param numColumns If specified non zero, then the bytes will be printed
  *                   out with this many bytes per row.  Defaults to 16.
  *                   If set to zero, then all the output will be placed
  *                   on a single line, using a simpler hex-only format.
  * @param p The OutputPrinter to use for printing.  A FILE (e.g. stdout), a String, or a MUSCLE_LOG_SEVERITY_* value can 
  *          be specified here, depending on where you'd like the text output to go to.  Defaults to stdout.
  */
void PrintHexBytes(const ConstByteBufferRef & bbRef, const char * optDesc = NULL, uint32 numColumns = 16, const OutputPrinter & p = stdout);

/** This is a convenience function for debugging.  It will print to stdout the
  * specified array of bytes in human-readable hexadecimal format, along with
  * an ASCII sidebar when possible.
  * @param bb A ByteBufferRef referencing the bytes to print.
  * @param optDesc if non-NULL, this will be used as a prefix/title string.
  * @param numColumns If specified non zero, then the bytes will be printed
  *                   out with this many bytes per row.  Defaults to 16.
  *                   If set to zero, then all the output will be placed
  *                   on a single line, using a simpler hex-only format.
  * @param p The OutputPrinter to use for printing.  A FILE (e.g. stdout), a String, or a MUSCLE_LOG_SEVERITY_* value can 
  *          be specified here, depending on where you'd like the text output to go to.  Defaults to stdout.
  */
void PrintHexBytes(const ByteBuffer & bb, const char * optDesc = NULL, uint32 numColumns = 16, const OutputPrinter & p = stdout);

/** This is a convenience function for debugging.  It will print to stdout the
  * specified array of bytes in human-readable hexadecimal format, along with
  * an ASCII sidebar when possible.
  * @param bytes A Queue of uint8s representing the bytes to print out.
  * @param optDesc if non-NULL, this will be used as a prefix/title string.
  * @param numColumns If specified non zero, then the bytes will be printed
  *                   out with this many bytes per row.  Defaults to 16.
  *                   If set to zero, then all the output will be placed
  *                   on a single line, using a simpler hex-only format.
  * @param p The OutputPrinter to use for printing.  A FILE (e.g. stdout), a String, or a MUSCLE_LOG_SEVERITY_* value can 
  *          be specified here, depending on where you'd like the text output to go to.  Defaults to stdout.
  */
void PrintHexBytes(const Queue<uint8> & bytes, const char * optDesc = NULL, uint32 numColumns = 16, const OutputPrinter & p = stdout);

/** This function is the same as PrintHexBytes(), but the output is sent to LogPlain() instead of fprintf().
  * @param logLevel The MUSCLE_LOG_* value indicating the severity level to log the hex bytes at.
  * @param bytes The bytes to print out.  May be NULL.
  * @param numBytes How many bytes (bytes) points to
  * @param optDesc if non-NULL, this will be used as a prefix/title string.
  * @param numColumns If specified non zero, then the bytes will be printed
  *                   out with this many bytes per row.  Defaults to 16.
  *                   If set to zero, then all the output will be placed
  *                   on a single line, using a simpler hex-only format.
  */
void LogHexBytes(int logLevel, const void * bytes, uint32 numBytes, const char * optDesc = NULL, uint32 numColumns = 16);

/** This function is the same as PrintHexBytes(), but the output is sent to LogPlain() instead of fprintf().
  * @param logLevel The MUSCLE_LOG_* value indicating the severity level to log the hex bytes at.
  * @param bbRef Reference to the ByteBuffer to print out.  May be a NULL reference.
  * @param optDesc if non-NULL, this will be used as a prefix/title string.
  * @param numColumns If specified non zero, then the bytes will be printed
  *                   out with this many bytes per row.  Defaults to 16.
  *                   If set to zero, then all the output will be placed
  *                   on a single line, using a simpler hex-only format.
  */
void LogHexBytes(int logLevel, const ConstByteBufferRef & bbRef, const char * optDesc = NULL, uint32 numColumns = 16);

/** This function is the same as PrintHexBytes(), but the output is sent to LogPlain() instead of fprintf().
  * @param logLevel The MUSCLE_LOG_* value indicating the severity level to log the hex bytes at.
  * @param bb The ByteBuffer to print out.
  * @param optDesc if non-NULL, this will be used as a prefix/title string.
  * @param numColumns If specified non zero, then the bytes will be printed
  *                   out with this many bytes per row.  Defaults to 16.
  *                   If set to zero, then all the output will be placed
  *                   on a single line, using a simpler hex-only format.
  */
void LogHexBytes(int logLevel, const ByteBuffer & bb, const char * optDesc = NULL, uint32 numColumns = 16);

/** This function is the same as PrintHexBytes(), but the output is sent to LogPlain() instead of fprintf().
  * @param logLevel The MUSCLE_LOG_* value indicating the severity level to log the hex bytes at.
  * @param bytes A Queue of uint8s representing the bytes to print out.
  * @param optDesc if non-NULL, this will be used as a prefix/title string.
  * @param numColumns If specified non zero, then the bytes will be printed
  *                   out with this many bytes per row.  Defaults to 16.
  *                   If set to zero, then all the output will be placed
  *                   on a single line, using a simpler hex-only format.
  */
void LogHexBytes(int logLevel, const Queue<uint8> & bytes, const char * optDesc = NULL, uint32 numColumns = 16);

/** This function is the same as PrintHexBytes(), but the output is returned as a String.
  * @param bytes The bytes to return a structured-text description of.  May be NULL.
  * @param numBytes How many bytes (bytes) points to
  * @param optDesc if non-NULL, this will be used as a prefix/title string.
  * @param numColumns If specified non zero, then the bytes will be generated
  *                   with this many bytes per row.  Defaults to 16.
  *                   If set to zero, then all the output will be placed
  *                   on a single line, using a simpler hex-only format.
  */
String HexBytesToAnnotatedString(const void * bytes, uint32 numBytes, const char * optDesc = NULL, uint32 numColumns = 16);

/** This function is the same as PrintHexBytes(), but the output is returned as a String.
  * @param bbRef Reference to the ByteBuffer to return a structured-text description of.  May be a NULL reference.
  * @param optDesc if non-NULL, this will be used as a prefix/title string.
  * @param numColumns If specified non zero, then the bytes will be generated
  *                   with this many bytes per row.  Defaults to 16.
  *                   If set to zero, then all the output will be placed
  *                   on a single line, using a simpler hex-only format.
  */
String HexBytesToAnnotatedString(const ConstByteBufferRef & bbRef, const char * optDesc = NULL, uint32 numColumns = 16);

/** This function is the same as PrintHexBytes(), but the output is returned as a String.
  * @param bb The ByteBuffer to return a structured-text description of.
  * @param optDesc if non-NULL, this will be used as a prefix/title string.
  * @param numColumns If specified non zero, then the bytes will be generated
  *                   with this many bytes per row.  Defaults to 16.
  *                   If set to zero, then all the output will be placed
  *                   on a single line, using a simpler hex-only format.
  */
String HexBytesToAnnotatedString(const ByteBuffer & bb, const char * optDesc = NULL, uint32 numColumns = 16);

/** This function is the same as PrintHexBytes(), but the output is returned as a String.
  * @param bytes A Queue of uint8s representing the bytes to return a structured-text description of.
  * @param optDesc if non-NULL, this will be used as a prefix/title string.
  * @param numColumns If specified non zero, then the bytes will be generated
  *                   with this many bytes per row.  Defaults to 16.
  *                   If set to zero, then all the output will be placed
  *                   on a single line, using a simpler hex-only format.
  */
String HexBytesToAnnotatedString(const Queue<uint8> & bytes, const char * optDesc = NULL, uint32 numColumns = 16);

/** Given a string with an ASCII representation of hexadecimal bytes,
  * returns the corresponding binary data.
  * @param buf A hexadecimal string.  Each hex byte should be expressed as
  *            two ASCII characters (eg "f0 1f 7e f7"), or alternatively
  *            you can enter chars in ASCII if you prepend each one with
  *            a slash (eg "/h /e /l /l /o").
  * @returns a ByteBufferRef containing the corresponding binary data,
  *          or a NULL ByteBufferRef on failure (out of memory?)
  */
ByteBufferRef HexBytesFromString(const char * buf);

/** Given a byte buffer, returns an ASCII representation of it.
  * @param buf A buffer of bytes
  * @param numBytes The number of bytes that (buf) points to.
  * @param withSpaces if left as true (the default value) then the returned
  *                   String will contain a space between each hex-digit-pair.
  *                   If false, then the hex-digit-pairs won't be separated by spaces.
  * @returns a String with human-readable contents:  eg "5F A3 A2" (if withSpaces was
  *          pass as true) or "5FA3A2" (if withSpaces is passed as false)
  */
String HexBytesToString(const uint8 * buf, uint32 numBytes, bool withSpaces = true);

/** Given a reference to a byte buffer, returns an ASCII representation of it.
  * @param bbRef Reference to a buffer of bytes.
  * @param withSpaces if left as true (the default value) then the returned
  *                   String will contain a space between each hex-digit-pair.
  *                   If false, then the hex-digit-pairs won't be separated by spaces.
  * @returns a String with human-readable contents:  eg "5F A3 A2" (if withSpaces was
  *          pass as true) or "5FA3A2" (if withSpaces is passed as false)
  */
String HexBytesToString(const ConstByteBufferRef & bbRef, bool withSpaces = true);

/** Given a byte buffer, returns an ASCII representation of it.
  * @param bb a buffer of bytes.
  * @param withSpaces if left as true (the default value) then the returned
  *                   String will contain a space between each hex-digit-pair.
  *                   If false, then the hex-digit-pairs won't be separated by spaces.
  * @returns a String with human-readable contents:  eg "5F A3 A2" (if withSpaces was
  *          pass as true) or "5FA3A2" (if withSpaces is passed as false)
  */
String HexBytesToString(const ByteBuffer & bb, bool withSpaces = true);

/** Given a Queue of uint8s, returns an ASCII representation of it.
  * @param bytes a Queue of uint8s.
  * @param withSpaces if left as true (the default value) then the returned
  *                   String will contain a space between each hex-digit-pair.
  *                   If false, then the hex-digit-pairs won't be separated by spaces.
  * @returns a String with human-readable contents:  eg "5F A3 A2" (if withSpaces was
  *          pass as true) or "5FA3A2" (if withSpaces is passed as false)
  */
String HexBytesToString(const Queue<uint8> & bytes, bool withSpaces = true);

/** Returns a Base-64-encoded string representing the passed-in byte array.
  * @param inBytes Pointer to the bytes to encode
  * @param numInBytes The number of bytes pointed to by (inBytes)
  * @returns the Base-64 string.
  */
String Base64Encode(const uint8 * inBytes, uint32 numInBytes);

/** Given a Base-64-encoded string (e.g. as previously returned by Base64Encode())
  * returns the corresponding original (unencoded) data, or an error if the base64
  * string could not be encoded
  * @param base64String a base64-encoded string.
  */
ByteBufferRef Base64Decode(const String & base64String);

/** Same as above, but the String is passed as a pointer-to-char rather than as a String object.
  * @param base64String Pointer to an optionally-NUL-terminated base64-encoded string.
  * @param maxBytes the maximum number of bytes to parse.  Defaults to MUSCLE_NO_LIMIT,
  *                 which is equivalent to passing in strlen(base64String).
  * @note if (base64String) is not NUL-terminated, then it is up to the caller to pass
  *       in an appropriate value for (maxBytes).
  * @note if (base64String) is NULL, it will be treated as an zero-byte string ("")
  */
ByteBufferRef Base64Decode(const char * base64String, uint32 maxBytes = MUSCLE_NO_LIMIT);

/** A convenience method, useful to optimally create a single PR_COMMAND_BATCH
  * Message out of a set of zero or more other Messages.  Here's how it works:
  * If (batchMsg) is a NULL ref, then (batchMsg) is set to reference the same
  * Message as (newMsg).  If (batchMsg) is a PR_COMMAND_BATCH Message, then
  * (newMsg) is appended to its PR_NAME_KEYS field.  Otherwise, a new
  * PR_COMMAND_BATCH Message is created, and both (batchMsg) and (newMsg) are
  * added to it.
  * @param batchMsg Reference to the Message that you will want to eventually
  *                 send to the server.  This Reference will be modified.
  *                 May be a NULL ref.
  * @param newMsg Reference to the Message to add to (batchMsg).  May not be a NULL ref.
  * @param prepend If set to true, the new Message will be added to the beginning
  *                of the assembled batch-Message's sub-Messages list, rather than the end.
  *                Default to false.
  * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
  */
status_t AssembleBatchMessage(MessageRef & batchMsg, const MessageRef & newMsg, bool prepend = false);

/** Returns true iff the file with the specified path exists and is readable.
  * @param filePath Path of the file to check for.
  * @returns true if the file exists (and is readable), false otherwise.
  */
MUSCLE_NODISCARD bool FileExists(const char * filePath);

/** Attempts to rename a file or directory from (oldPath) to (newPath).
  * @param oldPath the path of an existing file or directory.
  * @param newPath the new name that the file should have.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t RenameFile(const char * oldPath, const char * newPath);

/** Attempts to copy from (oldPath) to (newPath).
  * @param oldPath the path of an existing file or directory.
  * @param newPath the name that the new file should have.
  * @param allowCopyFolderRecursive If set true, and (oldPath) points to a folder, then this
  *                                 function will recursively copy the folder to the specified destination path.
  *                                 If left false (the default), then this method will return an error code if
  *                                 (oldPath) points to a folder instead of to a file.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t CopyFile(const char * oldPath, const char * newPath, bool allowCopyFolderRecursive = false);

/** Attempts to delete the file with the specified file path.
  * @param filePath Path of the file to delete.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t DeleteFile(const char * filePath);

/** Given argv[0], returns a human-readable program title based on the file name.
  * For example, "c:\Program Files\Blah.exe" is returned as "Blah", or
  * "/Users/jaf/MyProg/MyProg.app/Contents/MacOS/MyProg" is returned as "MyProg".
  * @param argv0 argv[0], as passed to main().
  * @returns a human-readable title string.
  */
String GetHumanReadableProgramNameFromArgv0(const char * argv0);

#ifdef WIN32
/** This function is only available on Win32, and does the
  * standard AllocConsole() and freopen() trick to cause a
  * Console window to appear and be available for stdin/stdout/stderr
  * to operate on.
  * @param optOutFilePath If non-NULL, then instead of opening a console
  *                       window, the program's stdout and stderr output
  *                       will be redirected into a file that will be created
  *                       (or overwritten) at this path.  Defaults to NULL.
  */
void Win32AllocateStdioConsole(const char * optOutFilePath = NULL);
#endif

/** Returns a value between 0.0f and 1.0f, indicating what percentage of the
  * host computer's RAM is currently in use.  Does not include filesystem
  * cache in this figure.  The intent is that values close to 1.0 indicate
  * that the system is close to memory exhaustion.
  * @note This function is implemented only under Linux, MacOS/X, and Windows.
  * @returns a value between 0.0f and 1.0f representing memory usage,
  *          or a negative value on failure.
  */
MUSCLE_NODISCARD float GetSystemMemoryUsagePercentage();

/** Returns the (approximate) current size of the process-heap for the current process, in bytes.
  * Returns 0 on failure (ie unable to determine heap size)
  * @note this function is currently implemented for MacOS/X, Windows, and Linux only.
  *       On other OS's it will always return 0.
  */
MUSCLE_NODISCARD uint64 GetProcessMemoryUsage();

/** Convenience method for safely returning the value of an environment variable.
  * @param envVarName the name of the environment variable to returns
  * @param defaultValue the value to return if the requested environment variable does not exist.
  *                     Defaults to an empty String.
  */
String GetEnvironmentVariableValue(const String & envVarName, const String & defaultValue = GetEmptyString());

/** Returns a cheap-and-cheerful pseudo-random number between 0 and (maxVal-1)
  * @param maxVal If set, the number returned will be less than this value.  Defaults to MUSCLE_NO_LIMIT.
  * @note this function is not at all cryptographically secure; it should only be used when you
  *       just need something vaguely random-looking and don't care about maintaining a uniform distribution, etc.
  */
static inline uint32 GetInsecurePseudoRandomNumber(uint32 maxVal = MUSCLE_NO_LIMIT)
{
   if (maxVal == 0) return 0;

   // coverity[dont_call] - don't care that rand() isn't secure, because this function isn't meant to be secure
   const int r = rand();
   return (maxVal == MUSCLE_NO_LIMIT) ? r : (r%maxVal);
}

/** Given a mangled C++ symbol, tries to return a more human-readable (aka unmangled) form of it.
  * @param mangledName a mangled C++ symbol-name
  * @returns a more human-readable string representing the type.
  *          In the worse case it will be the same as the argument that was passed in.
  */
String GetUnmangledSymbolName(const char * mangledName);

/** @} */ // end of miscutilityfunctions doxygen group

} // end namespace muscle

#endif
