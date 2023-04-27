#include "crashdumper.h"

#include <iostream>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

#include <iosfwd>
#include <string>

#include "windows.h"
//#include "WinNT.h"
#include "dbghelp.h"

namespace
{
    /*!
     * \brief Get monotonic time with 1ms resolution
     *
     * \return time in milliseconds
     */
    inline std::string
        getTimeStamp()
    {
        struct timespec now;
        char            buff[128];

        timespec_get(&now, TIME_UTC);

        strftime(buff, sizeof(buff), "%Y-%m-%dT%H_%M_%S", gmtime(&now.tv_sec)); // make ISO format

        return buff;
    }
}  // namespace

class CrashHandling
{
public:
    static void start(); // initialize crash handling. Do it once only.

    //static std::string dumpFileName();
    //static std::string dumpLogName();

private:
    static LONG WINAPI handleDump(EXCEPTION_POINTERS* info);
    static void catchSignal(int signum);
    static void initializeDump();

    static void terminateHandler();
    static void unexpectedHandler();
    static void seHandler(unsigned int u, EXCEPTION_POINTERS* pExp);

    static void createWinDumpFile(std::ostream& stream, EXCEPTION_POINTERS* info);

    // based on dbghelp.h
    typedef BOOL(WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
        CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
        CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
        CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
        );
    typedef LPAPI_VERSION(WINAPI *IMAGEHLPAPIVERSION)(void);
    typedef LPAPI_VERSION(WINAPI *IMAGEHLPAPIVERSIONEX)(LPAPI_VERSION AppVersion);

    static std::string dumpInfoString;
    static EXCEPTION_POINTERS* ex_info;
};

class StackTrace
{
public:
    // Creates a stacktrace from the current location.
    StackTrace();

    // Creates a stacktrace from the current location, of up to |count| entries.
    // |count| will be limited to at most |kMaxTraces|.
    explicit StackTrace(size_t count);

    // Creates a stacktrace from an existing array of instruction
    // pointers (such as returned by Addresses()).  |count| will be
    // limited to at most |kMaxTraces|.
    StackTrace(const void* const* trace, size_t count);

    // Creates a stacktrace for an exception.
    // Note: this function will throw an import not found (StackWalk64) exception
    // on system without dbghelp 5.1.
    StackTrace(EXCEPTION_POINTERS* exception_pointers);
    StackTrace(const _CONTEXT* context);

    // Copying and assignment are allowed with the default functions.

    // Gets an array of instruction pointer values. |*count| will be set to the
    // number of elements in the returned array.
    const void* const* Addresses(size_t* count) const;

    // Prints the stack trace to stderr.
    void Print() const;

    // Resolves backtrace to symbols and write to stream.
    void OutputToStream(std::ostream& os) const;

    // Resolves backtrace to symbols and returns as string.
    std::string ToString() const;

private:
    void InitTrace(const _CONTEXT* context_record);

    // From http://msdn.microsoft.com/en-us/library/bb204633.aspx,
    // the sum of FramesToSkip and FramesToCapture must be less than 63,
    // so set it to 62. Even if on POSIX it could be a larger value, it usually
    // doesn't give much more information.
    static const int kMaxTraces = 62;

    void* trace_[kMaxTraces];

    // The number of valid frames in |trace_|.
    size_t count_;
};

// Previous unhandled filter. Will be called if not NULL when we intercept an
// exception. Only used in unit tests.
LPTOP_LEVEL_EXCEPTION_FILTER g_previous_filter = NULL;

static std::string g_workDir;
static bool g_crashDumpCreated = false;
bool g_initialized_symbols = false;
DWORD g_init_error = ERROR_SUCCESS;

std::string CrashHandling::dumpInfoString;


///////////////////////////////////////////////////////////////////////////////
// Custom minidump callback 
//

BOOL CALLBACK miniDumpCallback(
    PVOID                            pParam,
    const PMINIDUMP_CALLBACK_INPUT   pInput,
    PMINIDUMP_CALLBACK_OUTPUT        pOutput
)
{
    BOOL bRet = FALSE;


    // Check parameters 

    if (pInput == 0)
        return FALSE;

    if (pOutput == 0)
        return FALSE;


    // Process the callbacks 

    switch (pInput->CallbackType)
    {
    case IncludeModuleCallback:
    {
        // Include the module into the dump 
        bRet = TRUE;
    }
    break;

    case IncludeThreadCallback:
    {
        // Include the thread into the dump 
        bRet = TRUE;
    }
    break;

    case ModuleCallback:
    {
        // Are data sections available for this module ? 
        bRet = TRUE;
    }
    break;

    case ThreadCallback:
    {
        // Include all thread information into the minidump 
        bRet = TRUE;
    }
    break;

    case ThreadExCallback:
    {
        // Include this information 
        bRet = TRUE;
    }
    break;

    case MemoryCallback:
    {
        // We do not include any information here -> return FALSE 
        bRet = FALSE;
    }
    break;

    case CancelCallback:
        break;
    }

    return bRet;

}

// Prints the exception call stack.
// This is the unit tests exception filter.
long WINAPI StackDumpExceptionFilter(EXCEPTION_POINTERS* info)
{
    std::stringstream ss;
    DWORD exc_code = info->ExceptionRecord->ExceptionCode;
    //	ss << "Received fatal exception ";
    switch (exc_code) {
    case EXCEPTION_ACCESS_VIOLATION:
        ss << "EXCEPTION_ACCESS_VIOLATION";
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        ss << "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        break;
    case EXCEPTION_BREAKPOINT:
        ss << "EXCEPTION_BREAKPOINT";
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        ss << "EXCEPTION_DATATYPE_MISALIGNMENT";
        break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        ss << "EXCEPTION_FLT_DENORMAL_OPERAND";
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        ss << "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        break;
    case EXCEPTION_FLT_INEXACT_RESULT:
        ss << "EXCEPTION_FLT_INEXACT_RESULT";
        break;
    case EXCEPTION_FLT_INVALID_OPERATION:
        ss << "EXCEPTION_FLT_INVALID_OPERATION";
        break;
    case EXCEPTION_FLT_OVERFLOW:
        ss << "EXCEPTION_FLT_OVERFLOW";
        break;
    case EXCEPTION_FLT_STACK_CHECK:
        ss << "EXCEPTION_FLT_STACK_CHECK";
        break;
    case EXCEPTION_FLT_UNDERFLOW:
        ss << "EXCEPTION_FLT_UNDERFLOW";
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        ss << "EXCEPTION_ILLEGAL_INSTRUCTION";
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        ss << "EXCEPTION_IN_PAGE_ERROR";
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        ss << "EXCEPTION_INT_DIVIDE_BY_ZERO";
        break;
    case EXCEPTION_INT_OVERFLOW:
        ss << "EXCEPTION_INT_OVERFLOW";
        break;
    case EXCEPTION_INVALID_DISPOSITION:
        ss << "EXCEPTION_INVALID_DISPOSITION";
        break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        ss << "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        ss << "EXCEPTION_PRIV_INSTRUCTION";
        break;
    case EXCEPTION_SINGLE_STEP:
        ss << "EXCEPTION_SINGLE_STEP";
        break;
    case EXCEPTION_STACK_OVERFLOW:
        ss << "EXCEPTION_STACK_OVERFLOW";
        break;
    default:
        ss << "0x" << std::hex << exc_code;
        break;
    }
    ss << "\n";

    std::string crashpath = g_workDir + "pj-app." + getTimeStamp() + ".CRASH.log";

    std::ofstream fs;
    fs.open(crashpath);

    if (fs)
    {
        fs << "\n------------------------ CRASH INFO 1 ------------------------\n";
        StackTrace(info).OutputToStream(fs);

        fs.flush();
    }

    if (g_previous_filter)
        return g_previous_filter(info);

    return EXCEPTION_CONTINUE_SEARCH;
}

bool InitializeSymbols() {
    if (g_initialized_symbols)
        return g_init_error == ERROR_SUCCESS;
    g_initialized_symbols = true;
    // Defer symbol load until they're needed, use undecorated names, and get line
    // numbers.
    SymSetOptions(SYMOPT_DEFERRED_LOADS |
        SYMOPT_UNDNAME |
        SYMOPT_LOAD_LINES);
    if (!SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
        g_init_error = GetLastError();
        // TODO(awong): Handle error: SymInitialize can fail with
        // ERROR_INVALID_PARAMETER.
        // When it fails, we should not call debugbreak since it kills the current
        // process (prevents future tests from running or kills the browser
        // process).
        //DLOG(ERROR) << "SymInitialize failed: " << g_init_error;
        return false;
    }

    // When transferring the binaries e.g. between bots, path put
    // into the executable will get off. To still retrieve symbols correctly,
    // add the directory of the executable to symbol search path.
    // All following errors are non-fatal.
    const size_t kSymbolsArraySize = 1024;
    std::unique_ptr<wchar_t[]> symbols_path(new wchar_t[kSymbolsArraySize]);

    // Note: The below function takes buffer size as number of characters,
    // not number of bytes!
    if (!SymGetSearchPathW(GetCurrentProcess(),
        symbols_path.get(),
        kSymbolsArraySize)) {
        g_init_error = GetLastError();
        //DLOG(WARNING) << "SymGetSearchPath failed: " << g_init_error;
        return false;
    }
/*
    std::wstring new_path(std::wstring(symbols_path.get()) +
        L";" + QCoreApplication::applicationDirPath().toStdWString());
    if (!SymSetSearchPathW(GetCurrentProcess(), new_path.c_str())) {
        g_init_error = GetLastError();
        //DLOG(WARNING) << "SymSetSearchPath failed." << g_init_error;
        return false;
    }
*/
    g_init_error = ERROR_SUCCESS;
    return true;
}

// SymbolContext is a threadsafe singleton that wraps the DbgHelp Sym* family
// of functions.  The Sym* family of functions may only be invoked by one
// thread at a time.  SymbolContext code may access a symbol server over the
// network while holding the lock for this singleton.  In the case of high
// latency, this code will adversely affect performance.
//
// There is also a known issue where this backtrace code can interact
// badly with breakpad if breakpad is invoked in a separate thread while
// we are using the Sym* functions.  This is because breakpad does now
// share a lock with this function.  See this related bug:
//
//   https://crbug.com/google-breakpad/311
//
// This is a very unlikely edge case, and the current solution is to
// just ignore it.
class SymbolContext
{
public:
    static SymbolContext* GetInstance() {
        static SymbolContext instance;

        return &instance;
    }

    // For the given trace, attempts to resolve the symbols, and output a trace
    // to the ostream os.  The format for each line of the backtrace is:
    //
    //    <tab>SymbolName[0xAddress+Offset] (FileName:LineNo)
    //
    // This function should only be called if Init() has been called.  We do not
    // LOG(FATAL) here because this code is called might be triggered by a
    // LOG(FATAL) itself. Also, it should not be calling complex code that is
    // extensible like PathService since that can in turn fire CHECKs.
    void OutputTraceToStream(const void* const* trace,
        size_t count,
        std::ostream& os) {
        std::lock_guard<std::mutex> lock(m_mtx);

        for (size_t i = 0; (i < count) && !os.bad(); ++i) {
            const int kMaxNameLength = 256;
            DWORD_PTR frame = reinterpret_cast<DWORD_PTR>(trace[i]);

            // Code adapted from MSDN example:
            // http://msdn.microsoft.com/en-us/library/ms680578(VS.85).aspx
            ULONG64 buffer[
                (sizeof(SYMBOL_INFO) +
                    kMaxNameLength * sizeof(wchar_t) +
                    sizeof(ULONG64) - 1) /
                    sizeof(ULONG64)];
            ::memset(buffer, 0, sizeof(buffer));

            // Initialize symbol information retrieval structures.
            DWORD64 sym_displacement = 0;
            PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(&buffer[0]);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = kMaxNameLength - 1;
            BOOL has_symbol = ::SymFromAddr(::GetCurrentProcess(), frame,
                &sym_displacement, symbol);

            // Attempt to retrieve line number information.
            DWORD line_displacement = 0;
            IMAGEHLP_LINE line = {};
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE);
            BOOL has_line = SymGetLineFromAddr(::GetCurrentProcess(), frame,
                &line_displacement, &line);

            // Output the backtrace line.
            os << "\t";
            if (has_symbol) {
                os << symbol->Name << " [0x" << trace[i] << "+"
                    << sym_displacement << "]";
            }
            else {
                // If there is no symbol information, add a spacer.
                os << "(No symbol) [0x" << trace[i] << "]";
            }
            if (has_line) {
                os << " (" << line.FileName << ":" << line.LineNumber << ")";
            }
            os << "\n";
        }
    }

private:

    SymbolContext() {
        InitializeSymbols();
    }
    SymbolContext(const SymbolContext&) = delete;
    SymbolContext& operator=(const SymbolContext&) = delete;

    std::mutex m_mtx;
};

bool EnableInProcessStackDumping() {
    // Add stack dumping support on exception on windows. Similar to OS_POSIX
    // signal() handling in process_util_posix.cc.
    g_previous_filter = ::SetUnhandledExceptionFilter(&StackDumpExceptionFilter);

    // Need to initialize symbols early in the process or else this fails on
    // swarming (since symbols are in different directory than in the exes) and
    // also release x64.
    return InitializeSymbols();
}

// Disable optimizations for the StackTrace::StackTrace function. It is
// important to disable at least frame pointer optimization ("y"), since
// that breaks CaptureStackBackTrace() and prevents StackTrace from working
// in Release builds (it may still be janky if other frames are using FPO,
// but at least it will make it further).
#if defined(COMPILER_MSVC)
#pragma optimize("", off)
#endif

StackTrace::StackTrace(size_t count)
{
    count = min(kMaxTraces, count);

    // When walking our own stack, use CaptureStackBackTrace().
    count_ = CaptureStackBackTrace(0, count, trace_, NULL);
}

#if defined(COMPILER_MSVC)
#pragma optimize("", on)
#endif

StackTrace::StackTrace(EXCEPTION_POINTERS* exception_pointers)
{
    InitTrace(exception_pointers->ContextRecord);
}

StackTrace::StackTrace(const CONTEXT* context)
{
    InitTrace(context);
}

void StackTrace::InitTrace(const CONTEXT* context_record) {
    // StackWalk64 modifies the register context in place, so we have to copy it
    // so that downstream exception handlers get the right context.  The incoming
    // context may have had more register state (YMM, etc) than we need to unwind
    // the stack. Typically StackWalk64 only needs integer and control registers.
    CONTEXT context_copy;
    memcpy(&context_copy, context_record, sizeof(context_copy));
    context_copy.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;

    // When walking an exception stack, we need to use StackWalk64().
    count_ = 0;
    // Initialize stack walking.
    STACKFRAME64 stack_frame;
    memset(&stack_frame, 0, sizeof(stack_frame));
#if defined(_WIN64)
    int machine_type = IMAGE_FILE_MACHINE_AMD64;
    stack_frame.AddrPC.Offset = context_record->Rip;
    stack_frame.AddrFrame.Offset = context_record->Rbp;
    stack_frame.AddrStack.Offset = context_record->Rsp;
#else
    int machine_type = IMAGE_FILE_MACHINE_I386;
    stack_frame.AddrPC.Offset = context_record->Eip;
    stack_frame.AddrFrame.Offset = context_record->Ebp;
    stack_frame.AddrStack.Offset = context_record->Esp;
#endif
    stack_frame.AddrPC.Mode = AddrModeFlat;
    stack_frame.AddrFrame.Mode = AddrModeFlat;
    stack_frame.AddrStack.Mode = AddrModeFlat;
    while (StackWalk64(machine_type,
        GetCurrentProcess(),
        GetCurrentThread(),
        &stack_frame,
        &context_copy,
        NULL,
        &SymFunctionTableAccess64,
        &SymGetModuleBase64,
        NULL) &&
        count_ < kMaxTraces) {
        trace_[count_++] = reinterpret_cast<void*>(stack_frame.AddrPC.Offset);
    }

    for (size_t i = count_; i < kMaxTraces; ++i)
        trace_[i] = NULL;
}

void StackTrace::Print() const {
    //  OutputToStream(&std::cerr);
}

void StackTrace::OutputToStream(std::ostream& os) const {
    SymbolContext* context = SymbolContext::GetInstance();
    if (g_init_error != ERROR_SUCCESS) {
        os << "Error initializing symbols (" << g_init_error
            << ").  Dumping unresolved backtrace:\n";
        for (size_t i = 0; (i < count_) && !os.bad(); ++i) {
            os << "\t" << trace_[i] << "\n";
        }
    }
    else {
        os << "Backtrace:\n";
        context->OutputTraceToStream(trace_, count_, os);
    }
}


EXCEPTION_POINTERS *CrashHandling::ex_info = 0;

void CrashHandling::catchSignal(int signum)
{
    std::string crashpath = g_workDir + "pj-app." + getTimeStamp() + ".CRASH.log";

    std::ofstream fs;
    fs.open(crashpath);

    if (fs)
    {
        fs << "\n------------------------ CRASH INFO ------------------------\n";
        createWinDumpFile(fs, NULL);

        if (ex_info) {
            StackTrace(ex_info).OutputToStream(fs);
        }
        else {
            fs << "\n------------------------ CRASH INFO 2 ------------------------\n";
            StackTrace(32).OutputToStream(fs);
        }

        fs.flush();
    }

    ::exit(signum);
}



void CrashHandling::initializeDump()
{
    g_previous_filter = ::SetUnhandledExceptionFilter(CrashHandling::handleDump);
}

void CrashHandling::terminateHandler()
{
    catchSignal(-SIGTERM);
}

void CrashHandling::unexpectedHandler()
{
    catchSignal(-SIGABRT);
}

void CrashHandling::seHandler(unsigned int u, EXCEPTION_POINTERS* pExp)
{
    handleDump(pExp);
}

void CrashHandling::createWinDumpFile(std::ostream& stream, EXCEPTION_POINTERS * info)
{
    if (g_crashDumpCreated)
        return;

    HMODULE dllHandle = ::LoadLibraryA("DBGHELP.DLL");

    dumpInfoString.clear(); // = "";

    if (dllHandle)
    {
        IMAGEHLPAPIVERSIONEX pImagehlpApiVersionEx = (IMAGEHLPAPIVERSIONEX)::GetProcAddress(dllHandle, "ImagehlpApiVersionEx");
        int apiVer = 0;
        API_VERSION appApiVer;
        appApiVer.MajorVersion = 6;
        appApiVer.MinorVersion = 1;
        appApiVer.Revision = 6;
        LPAPI_VERSION lpApiVer = NULL;
        if (pImagehlpApiVersionEx) {
            lpApiVer = pImagehlpApiVersionEx(&appApiVer);
            apiVer = lpApiVer->MajorVersion * 0x0100 + lpApiVer->Revision;
        }

        MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress(dllHandle, "MiniDumpWriteDump");
        if (pDump != NULL)
        {
            std::string crashDumpPath = g_workDir + "pj-app." + getTimeStamp() + ".dmp";

            // create the file
            HANDLE fileHandle = ::CreateFileA(crashDumpPath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL, NULL);

            if (fileHandle != INVALID_HANDLE_VALUE)
            {
                _MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;

                exceptionInfo.ThreadId = ::GetCurrentThreadId();
                exceptionInfo.ExceptionPointers = info;
                exceptionInfo.ClientPointers = false;

                MINIDUMP_CALLBACK_INFORMATION mci;

                mci.CallbackRoutine = (MINIDUMP_CALLBACK_ROUTINE)miniDumpCallback;
                mci.CallbackParam = 0;

                // write the dump
                MINIDUMP_TYPE mdt = (MINIDUMP_TYPE)MiniDumpNormal;

                //MiniDumpNormal = Include just the information necessary to capture stack traces for all existing threads in a process.
                //MiniDumpWithDataSegs = Include the data sections from all loaded modules. This results in the inclusion of global variables, which can make the minidump file significantly larger.
                //MiniDumpWithIndirectlyReferencedMemory = Include pages with data referenced by locals or other stack memory. This option can increase the size of the minidump file significantly.
                //MiniDumpWithProcessThreadData = Include complete per-process and per-thread information from the operating system.

                // And additionally if dump is created under Win7.
                //MiniDumpWithPrivateReadWriteMemory = Scan the virtual address space for PAGE_READWRITE memory to be included.
                //MiniDumpWithThreadInfo = Include thread state information.

                if (apiVer > 0x501)
                    mdt = (MINIDUMP_TYPE)((unsigned int)mdt | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithProcessThreadData);
                if (apiVer > 0x604)
                    // Warning ! Adding these flags increases size of dump file
                    mdt = (MINIDUMP_TYPE)((unsigned int)mdt | MiniDumpWithThreadInfo);

                BOOL dumpSuccess = pDump(GetCurrentProcess(), GetCurrentProcessId(), fileHandle, mdt,
                    (info != 0) ? &exceptionInfo : 0, NULL, &mci);
                ::CloseHandle(fileHandle);
                if (dumpSuccess)
                {
                    stream << "Saved dump file to '" << crashDumpPath << "'.\nDump infomation flags: " << std::hex << mdt << std::dec << std::endl;
                }
                else
                {
                    stream << "Failed to save dump file to '" << crashDumpPath << "'.\nError: " << std::hex << GetLastError() << std::dec << std::endl;
                    ::DeleteFileA(crashDumpPath.c_str());
                }
            }
            else
            {
                stream << "Failed to create dump file '" << crashDumpPath << "'.\nError: " << std::hex << GetLastError() << std::dec << std::endl;
            }
            if (lpApiVer)
                stream << "\nDbgHelp.dll Version: " << lpApiVer->MajorVersion << '.' << lpApiVer->MinorVersion << '.' << lpApiVer->Revision << std::endl;
        }
        else
            stream << "DBGHELP.DLL too old\n";
    }
    else
        stream << "DBGHELP.DLL not found\n";

    g_crashDumpCreated = true;
}

// Routine to handle an otherwise unhandled exception in the code.
// It should be set with this routine as the argument to SetUnhandledExceptionFilter() .
// Based on code at http://www.codeproject.com/debug/postmortemdebug_standalone1.asp
//
LONG WINAPI CrashHandling::handleDump(EXCEPTION_POINTERS *info)
{
    std::stringstream ss;
    ex_info = info;

    createWinDumpFile(ss, info);

    DWORD exc_code = info->ExceptionRecord->ExceptionCode;
    //	ss << "Received fatal exception ";
    switch (exc_code) {
    case EXCEPTION_ACCESS_VIOLATION:
        ss << "EXCEPTION_ACCESS_VIOLATION";
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        ss << "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        break;
    case EXCEPTION_BREAKPOINT:
        ss << "EXCEPTION_BREAKPOINT";
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        ss << "EXCEPTION_DATATYPE_MISALIGNMENT";
        break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        ss << "EXCEPTION_FLT_DENORMAL_OPERAND";
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        ss << "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        break;
    case EXCEPTION_FLT_INEXACT_RESULT:
        ss << "EXCEPTION_FLT_INEXACT_RESULT";
        break;
    case EXCEPTION_FLT_INVALID_OPERATION:
        ss << "EXCEPTION_FLT_INVALID_OPERATION";
        break;
    case EXCEPTION_FLT_OVERFLOW:
        ss << "EXCEPTION_FLT_OVERFLOW";
        break;
    case EXCEPTION_FLT_STACK_CHECK:
        ss << "EXCEPTION_FLT_STACK_CHECK";
        break;
    case EXCEPTION_FLT_UNDERFLOW:
        ss << "EXCEPTION_FLT_UNDERFLOW";
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        ss << "EXCEPTION_ILLEGAL_INSTRUCTION";
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        ss << "EXCEPTION_IN_PAGE_ERROR";
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        ss << "EXCEPTION_INT_DIVIDE_BY_ZERO";
        break;
    case EXCEPTION_INT_OVERFLOW:
        ss << "EXCEPTION_INT_OVERFLOW";
        break;
    case EXCEPTION_INVALID_DISPOSITION:
        ss << "EXCEPTION_INVALID_DISPOSITION";
        break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        ss << "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        ss << "EXCEPTION_PRIV_INSTRUCTION";
        break;
    case EXCEPTION_SINGLE_STEP:
        ss << "EXCEPTION_SINGLE_STEP";
        break;
    case EXCEPTION_STACK_OVERFLOW:
        ss << "EXCEPTION_STACK_OVERFLOW";
        break;
    case 0xE06D7363:
        ss << "MSVC_THROW_EXCEPTION";
        break;
    default:
        ss << "UNKNOWN_EXCEPTION: 0x" << std::hex << exc_code;
        break;
    }
    ss << "\n";

    dumpInfoString = ss.str().c_str();
    catchSignal(0);

    dumpInfoString.clear();

    if (g_previous_filter)
        return g_previous_filter(info);

    return EXCEPTION_CONTINUE_SEARCH;
}

void CrashHandling::start()
{
    ex_info = 0;
    initializeDump();

    //set_terminate(terminateHandler);
    //set_unexpected(unexpectedHandler);

    //_set_se_translator(seHandler);

    _set_abort_behavior(_CALL_REPORTFAULT, _CALL_REPORTFAULT);

    // We are commenting out the CatchSignal for certain errors,
    // because we want them to be caught instead by HandleDump.
    ::signal(SIGABRT, catchSignal);
    //::signal(SIGFPE, catchSignal);
    //::signal(SIGILL, catchSignal);
    //::signal(SIGSEGV, catchSignal);
    ::signal(SIGINT, catchSignal);
    ::signal(SIGTERM, catchSignal);
}

//#include "backward.hpp"

void initCrashHandling(const char* dump_dir)
{
    g_workDir = dump_dir;
    CrashHandling::start();
    //static backward::SignalHandling sh{};
}
