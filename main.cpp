
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#if __has_include(<readline/readline.h>)
#include <readline/readline.h>
#include <readline/history.h>
#else
    #define NO_READLINE
#endif
#include "element.h"

int InterpretFile(element::VirtualMachine& vm, const char* fileString)
{
    element::Value result = vm.evalFile(fileString);
    std::cout << result.asString();
    vm.getMemoryManager().GarbageCollect();
    std::cout.flush();
    return 0;
}


#if !defined(NO_READLINE)
/*
* returns true if $line is more than just space.
* used to ignore blank lines.
*/
static bool notjustspace(const char* line)
{
    int c;
    size_t i;
    for(i=0; (c = line[i]) != 0; i++)
    {
        if(!isspace(c))
        {
            return true;
        }
    }
    return false;
}

int InterpretREPL(element::VirtualMachine& vm)
{
    size_t varid;
    size_t nowid;
    char* line;
    std::string exeme;
    std::string varname;
    
    varid = 0;
    while(true)
    {
        line = readline ("> ");
        fflush(stdout);
        if(line == NULL)
        {
            fprintf(stderr, "readline() returned NULL\n");
        }
        if((line != NULL) && (line[0] != '\0') && notjustspace(line))
        {
            exeme = line;
            add_history(line);
            {
                std::stringstream strm;
                strm << exeme;
                auto result = vm.evalStream(strm);
                if(result.isError())
                {
                    std::cout << "ERROR: ";
                }
                else
                {
                    std::cout << "= ";
                }
                std::cout << result.asString() << std::endl;
                vm.getMemoryManager().GarbageCollect();
                nowid = varid;
                varid++;
            }
        }
    }
    vm.getMemoryManager().GarbageCollect();
}
#endif

void DebugPrintFile(const char* fileString, bool ast, bool symbols, bool constants)
{
    std::ifstream file(fileString);

    if(!file.is_open())
    {
        std::cout << "Could not open file: " << fileString;
        return;
    }

    element::Logger logger;
    element::Parser parser(logger);

    std::shared_ptr<element::ast::FunctionNode> node = parser.Parse(file);

    if(logger.hasMessages())
    {
        std::cout << logger.getCombined();
        return;
    }

    if(ast)
        std::cout << element::ast::nodeToString(node);

    if(!(symbols || constants))
        return;

    element::SemanticAnalyzer semanticAnalyzer(logger);

    int index = 0;
    for(const auto& native : element::nativefunctions::GetAllFunctions())
        semanticAnalyzer.AddNativeFunction(native.name, index++);

    semanticAnalyzer.Analyze(node);

    if(logger.hasMessages())
    {
        std::cout << logger.getCombined();
        return;
    }

    element::Compiler compiler(logger);

    std::unique_ptr<char[]> bytecode = compiler.compile(node);

    if(logger.hasMessages())
    {
        std::cout << logger.getCombined();
        return;
    }

    if(symbols)
        std::cout << element::bytecodeSymbolsToString(bytecode.get());

    if(constants)
        std::cout << element::bytecodeConstantsToString(bytecode.get());
}

int main(int argc, char** argv)
{
    const char* usage =(
        "usage: element [ OPTIONS ] ... [ FILE ]\n"
        "OPTIONS:\n"
        "-h -? --help  : print this help\n"
        "-v --version  : print the interpreter version\n"
        "-da           : debug print the Abstract Syntax Tree\n"
        "-ds           : debug print the generated symbols\n"
        "-dc           : debug print the constants\n"
        "-dr           : run the file after debug printing\n"
    );

    bool printAst = false;
    bool printSymbols = false;
    bool printConstants = false;
    bool runAfterPrinting = false;

    const char* fileString = nullptr;
    element::VirtualMachine vm;

    for(int i = 1; i < argc; ++i)
    {
        if(argv[i][0] == '-')
        {
            if(argv[i][1] == 'd')// -d
            {
                if(strstr(argv[i], "a") != nullptr)
                    printAst = true;
                if(strstr(argv[i], "s") != nullptr)
                    printSymbols = true;
                if(strstr(argv[i], "c") != nullptr)
                    printConstants = true;
                if(strstr(argv[i], "r") != nullptr)
                    runAfterPrinting = true;
            }
            else if(argv[i][1] == 'v')// -v
            {
                std::cout << vm.getVersion() << '\n';
                return 0;
            }
            else if(argv[i][1] == 'h' || argv[i][1] == '?')// -h -?
            {
                std::cout << usage << std::endl;
                return 0;
            }
            else if(argv[i][1] == '-')// --
            {
                if(strstr(argv[i], "version") != nullptr)// --version
                {
                    std::cout << vm.getVersion() << '\n';
                    return 0;
                }
                else if(strstr(argv[i], "help") != nullptr)// --help
                {
                    std::cout << usage << std::endl;
                    return 0;
                }
                else
                {
                    std::cout << usage << std::endl;
                    return 0;
                }
            }
            else
            {
                std::cout << usage << std::endl;
                return 0;
            }
        }
        else// file
        {
            fileString = argv[i];
            break;
        }
    }

    if(fileString)
    {
        if(printAst || printSymbols || printConstants)
        {
            DebugPrintFile(fileString, printAst, printSymbols, printConstants);

            if(!runAfterPrinting)
                return 0;
        }
        return InterpretFile(vm, fileString);
    }
    #if !defined(NO_READLINE)
        return InterpretREPL(vm);
    #endif
    std::cerr << "no readline support compiled in, so no REPL. sorry" << std::endl;
    return 1;
}

